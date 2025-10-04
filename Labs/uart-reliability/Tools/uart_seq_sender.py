#!/usr/bin/env python3
"""
UART sequence/burst/random data feeder for baseline and optimization tests.

Usage examples:
  python uart_seq_sender.py --port COM5 --baud 115200
  python uart_seq_sender.py --port COM5 --mode burst --burst-size 4096 --burst-gap-ms 10
  python uart_seq_sender.py --port COM5 --mode random --chunk 2048 --duration 30

Modes:
  seq    : Continuous 0..255 wrap sequence (best for loss & burst metrics)
  burst  : Same sequence but send in large bursts then gap (tests idle vs spike)
  random : Pseudo-random bytes (cannot use your sequence-loss logic, for stress only)

Columns printed periodically:
  elapsed(s) sent(bytes) rate(B/s) mode extra

Exit with Ctrl+C.
"""
import argparse
import serial
import time
import os
import random
import threading
import sys


def gen_seq(start=0):
    v = start & 0xFF
    while True:
        yield v
        v = (v + 1) & 0xFF


def gen_random():
    while True:
        yield random.randint(0, 255)


def open_port(port: str, baud: int, timeout: float = 0):
    # Allow COM>9 using \\.\ prefix automatically
    if os.name == 'nt' and port.upper().startswith('COM') and len(port) > 4:
        port = r"\\.\\" + port
    return serial.Serial(port, baud, bytesize=8, parity='N', stopbits=1, timeout=timeout)


def build_chunk(gen, size: int):
    return bytes(next(gen) for _ in range(size))


def run_seq(ser, args):
    g = gen_seq(0)
    run_generic_stream(ser, args, g)


def run_random(ser, args):
    g = gen_random()
    run_generic_stream(ser, args, g)


def run_generic_stream(ser, args, generator):
    start = time.time()
    last_report = start
    sent = 0
    try:
        while True:
            if args.duration and (time.time() - start) >= args.duration:
                break
            buf = build_chunk(generator, args.chunk)
            ser.write(buf)
            sent += len(buf)
            now = time.time()
            if now - last_report >= args.report:
                rate = sent / (now - start)
                print(f"[HOST] elapsed={now-start:.1f}s sent={sent} rate={rate:.1f}B/s mode={args.mode}")
                last_report = now
    except KeyboardInterrupt:
        pass
    finally:
        total_time = time.time() - start
        if total_time == 0:
            total_time = 1e-9
        print(f"[HOST] DONE sent={sent} avg_rate={sent/total_time:.1f}B/s")


def run_burst(ser, args):
    g = gen_seq(0)
    start = time.time()
    last_report = start
    sent = 0
    seq_sent = 0
    try:
        while True:
            if args.duration and (time.time() - start) >= args.duration:
                break
            # Burst phase
            burst_chunk = min(args.burst_size, args.chunk)  # inner write size
            remaining = args.burst_size
            while remaining > 0:
                n = min(burst_chunk, remaining)
                ser.write(build_chunk(g, n))
                remaining -= n
                sent += n
                seq_sent += n
            # Gap (idle)
            if args.burst_gap_ms > 0:
                time.sleep(args.burst_gap_ms / 1000.0)
            now = time.time()
            if now - last_report >= args.report:
                rate = sent / (now - start)
                print(f"[HOST] elapsed={now-start:.1f}s sent={sent} rate={rate:.1f}B/s mode=burst seq_sent={seq_sent}")
                last_report = now
    except KeyboardInterrupt:
        pass
    finally:
        total_time = time.time() - start
        if total_time == 0:
            total_time = 1e-9
        print(f"[HOST] DONE sent={sent} avg_rate={sent/total_time:.1f}B/s bursts_mode")


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', required=True, help='COM port, e.g. COM5 or \\.\\COM10')
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--mode', choices=['seq', 'burst', 'random'], default='seq')
    ap.add_argument('--chunk', type=int, default=1024, help='Bytes per write call (seq/random)')
    ap.add_argument('--duration', type=int, default=0, help='Seconds to run (0=infinite)')
    ap.add_argument('--report', type=float, default=1.0, help='Status print interval (s)')
    # Burst-specific
    ap.add_argument('--burst-size', type=int, default=8192, help='Total bytes per burst (burst mode)')
    ap.add_argument('--burst-gap-ms', type=int, default=5, help='Gap after each burst (ms)')
    # Duplex / logging
    ap.add_argument('--listen', action='store_true', help='Also read board output and print (close SecureCRT)')
    ap.add_argument('--raw-hex', action='store_true', help='When listening, dump incoming bytes as hex instead of utf-8 lines')
    ap.add_argument('--log', help='Log incoming data to file')
    ap.add_argument('--quiet-in', action='store_true', help='Suppress incoming print (still logs if --log)')
    ap.add_argument('--tx-disable', action='store_true', help='Do not transmit data; just listen')
    ap.add_argument('--no-dtr', action='store_true', help='Do not assert DTR (some boards need DTR high to run/reset)')
    ap.add_argument('--no-rts', action='store_true', help='Do not assert RTS')
    return ap.parse_args()


def receiver_thread(ser: serial.Serial, stop_evt: threading.Event, args):
    """Continuously read from serial and display/log."""
    log_f = open(args.log, 'ab') if args.log else None
    buf_line = bytearray()
    try:
        while not stop_evt.is_set():
            try:
                data = ser.read(ser.in_waiting or 1)
            except serial.SerialException:
                break
            if not data:
                # Tiny sleep to avoid busy-spin if no data
                time.sleep(0.001)
                continue
            if log_f:
                log_f.write(data)
                log_f.flush()
            if args.quiet_in:
                continue
            if args.raw_hex:
                # Print as hex chunks
                hex_str = ' '.join(f'{b:02X}' for b in data)
                print(f'[IN HEX] {hex_str}')
            else:
                # Accumulate and split by newline for readability
                buf_line.extend(data)
                while b'\n' in buf_line:
                    line, _, rest = buf_line.partition(b'\n')
                    buf_line[:] = rest
                    try:
                        print('[IN] ' + line.decode('utf-8', errors='replace'))
                    except Exception:
                        print('[IN] ' + repr(line))
    finally:
        if log_f:
            log_f.close()


def main():
    args = parse_args()
    try:
        ser = open_port(args.port, args.baud)
    except serial.SerialException as e:
        print(f"[ERR] Could not open port: {e}")
        return 1
    print(f"[HOST] Opened {args.port} @ {args.baud} mode={args.mode} listen={args.listen}")

    # Control handshake lines (some boards reset on DTR toggle; we assert unless disabled)
    try:
        ser.dtr = not args.no_dtr
        ser.rts = not args.no_rts
    except Exception:
        pass

    # Allow MCU reboot (if it resets on DTR) to finish before we start sending
    time.sleep(0.15)

    stop_evt = threading.Event()
    rx_thread = None
    if args.listen:
        # Set a small timeout so read() returns quickly
        ser.timeout = 0
        rx_thread = threading.Thread(target=receiver_thread, args=(ser, stop_evt, args), daemon=True)
        rx_thread.start()

    try:
        if args.tx_disable:
            if not args.listen:
                print('[WARN] --tx-disable given without --listen: nothing to do; exiting in 3s')
                time.sleep(3)
            else:
                print('[HOST] TX disabled; listening only (Ctrl+C to exit)')
                # Just loop until duration or Ctrl+C
                start = time.time()
                while True:
                    if args.duration and (time.time() - start) >= args.duration:
                        break
                    time.sleep(0.1)
        else:
            if args.mode == 'seq':
                run_seq(ser, args)
            elif args.mode == 'random':
                run_random(ser, args)
            else:
                run_burst(ser, args)
    finally:
        stop_evt.set()
        # Give receiver a moment to exit
        if rx_thread:
            rx_thread.join(timeout=0.5)
        ser.close()
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
