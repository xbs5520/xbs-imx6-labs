#!/usr/bin/env python3
"""
I2C Recovery serial logger: reads newline-delimited firmware logs and writes CSV.
Assumes firmware prints key=value pairs per event line, e.g.:
  iter=42 fault=sda_stuck td_ms=3.2 tr_ms=7.8 ok=1 attempts=1 pulses=9

Usage (example):
  ./serial_logger.py --port /dev/ttyUSB0 --baud 115200 --out ../data/run_$(date +%F_%H%M%S).csv
  ./serial_logger.py --infile sample.log --out ../data/run_$(date +%F_%H%M%S).csv

CSV columns:
    ir,ps,als,iter,ts_host_ms,fault_type,td_ms,tr_ms,ok,attempts,scl_pulses,notes
"""
import argparse
import csv
import re
import sys
import time
from datetime import datetime

KNOWN_KEYS = {
    "ir", "ps", "als",
    "iter", "fault", "td_ms", "tr_ms", "ok", "attempts", "pulses"
}

CSV_HEADER = [
    "ir",
    "ps",
    "als",
    "iter",
    "ts_host_ms",
    "fault_type",
    "td_ms",
    "tr_ms",
    "ok",
    "attempts",
    "scl_pulses",
    "notes",
]

# Allow optional spaces after '=' and capture a single optional second token (e.g., 'No fault')
KV_RE = re.compile(r"(\w+)=\s*([^\s,]+(?:\s[^\s,]+)?)")

# Robust KV matcher: value goes until next " key=" or end of line
KV_SPAN_RE = re.compile(r"(\w+)\s*=\s*(.*?)(?=\s+\w+\s*=|$)")


def _strip_ctrl(s: str) -> str:
    # Remove non-printable control chars (including DEL 0x7F)
    return "".join(ch for ch in s if ch == "\t" or ch == "\n" or (32 <= ord(ch) <= 126))


def _normalize_fault(v: str) -> str:
    if not v:
        return "none"
    s = _strip_ctrl(v).strip().lower().replace('-', ' ').replace('_', ' ')
    if s in ("none", "no fault", "no", "ok"):  # accept variants
        return "none"
    if s in ("sda stuck",):
        return "sda_stuck"
    if s in ("stretch", "clock stretch", "clk stretch"):
        return "stretch"
    # fallback: replace spaces with underscore
    return s.replace(' ', '_')


def parse_line(line: str):
    # First, try robust span-based parsing to tolerate spaces and multi-word values
    pairs = KV_SPAN_RE.findall(line.strip())
    if not pairs:
        return None
    kv = {k: v.strip().strip(',') for k, v in pairs}

    # Build record with conversions; tolerate missing values
    rec = {}
    def _to_float(x, default=0.0):
        try:
            return float(x)
        except Exception:
            return default

    def _to_int(x, default=0):
        try:
            xs = x.strip()
            if xs.lower().startswith("0x"):
                return int(xs, 16)
            return int(xs)
        except Exception:
            return default

    rec["iter"] = _to_int(kv.get("iter", "0"), 0)
    rec["fault_type"] = _normalize_fault(kv.get("fault", "none"))
    rec["td_ms"] = _to_float(kv.get("td_ms", "0"), 0.0)
    rec["tr_ms"] = _to_float(kv.get("tr_ms", "0"), 0.0)
    rec["ok"] = _to_int(kv.get("ok", "0"), 0)
    rec["attempts"] = _to_int(kv.get("attempts", "0"), 0)
    rec["scl_pulses"] = _to_int(kv.get("pulses", "0"), 0)
    # sensor readings (optional)
    rec["ir"] = _to_int(kv.get("ir", "0"), 0)
    rec["ps"] = _to_int(kv.get("ps", "0"), 0)
    rec["als"] = _to_int(kv.get("als", "0"), 0)

    # Extras into notes
    extras = []
    for k, v in kv.items():
        if k not in KNOWN_KEYS and v:
            extras.append(f"{k}={v}")
    return rec, (" ".join(extras) if extras else "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="Serial port, e.g., /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--infile", help="Optional input text file instead of serial")
    ap.add_argument("--out", required=True, help="Output CSV path")
    ap.add_argument("--notes", default="", help="Notes to append to each row")
    args = ap.parse_args()

    # Open CSV
    outf = open(args.out, "w", newline="")
    writer = csv.DictWriter(outf, fieldnames=CSV_HEADER)
    writer.writeheader()

    rows = 0

    if args.infile:
        src = open(args.infile, "r", encoding="utf-8", errors="ignore")
        def _readline():
            return src.readline()
        stream_mode = False  # finite file
    else:
        try:
            import serial  # pyserial
        except ImportError:
            print("pyserial not installed. Use --infile or pip install pyserial", file=sys.stderr)
            sys.exit(2)
        ser = serial.Serial(args.port, args.baud, timeout=1)
        def _readline():
            return ser.readline().decode(errors="ignore")
        stream_mode = True  # endless stream

    print(f"Logging to: {args.out}")
    try:
        while True:
            line = _readline()
            if not line:
                if stream_mode:
                    time.sleep(0.05)
                    continue
                else:
                    break  # EOF in file mode
            ts_ms = int(time.time() * 1000)
            parsed = parse_line(line)
            if not parsed:
                continue
            rec, extra_notes = parsed
            notes = args.notes
            if extra_notes:
                notes = (notes + " " + extra_notes).strip() if notes else extra_notes
            rec_row = {
                "ir": rec.get("ir", 0),
                "ps": rec.get("ps", 0),
                "als": rec.get("als", 0),
                "iter": rec["iter"],
                "ts_host_ms": ts_ms,
                "fault_type": rec["fault_type"],
                "td_ms": rec["td_ms"],
                "tr_ms": rec["tr_ms"],
                "ok": rec["ok"],
                "attempts": rec["attempts"],
                "scl_pulses": rec["scl_pulses"],
                "notes": notes,
            }
            writer.writerow(rec_row)
            rows += 1
            if stream_mode:
                outf.flush()
    except KeyboardInterrupt:
        pass
    finally:
        outf.flush()
        outf.close()
        if not stream_mode:
            print(f"Done. Wrote {rows} rows to {args.out}")

if __name__ == "__main__":
    main()
