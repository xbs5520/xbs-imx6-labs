#!/usr/bin/env python3
"""
性能分析版接收器 - 详细统计和可视化
"""

import serial
import struct
import time
import sys
from collections import deque
import statistics

# ==================== 配置 ====================
# SERIAL_PORT = '/dev/ttyUSB0'  # Linux
SERIAL_PORT = 'COM5'            # Windows - 你的配置
BAUD_RATE = 115200
PACKET_SIZE = 29
GPT1_FREQ_HZ = 645000  # 根据你的实测结果调整

PACKET_FORMAT = '<2sIH6h2IB'

# ==================== 辅助函数 ====================
def ticks_to_ms(ticks):
    return (ticks * 1000.0) / GPT1_FREQ_HZ

def calculate_checksum(data):
    return sum(data[:PACKET_SIZE-1]) & 0xFF

def parse_packet(data):
    if len(data) != PACKET_SIZE:
        return None
    
    if data[0] != 0xAA or data[1] != 0x55:
        return None
    
    expected_checksum = calculate_checksum(data)
    actual_checksum = data[PACKET_SIZE-1]
    
    try:
        unpacked = struct.unpack(PACKET_FORMAT, data)
        header, timestamp, seq_num, ax, ay, az, gx, gy, gz, process_ticks, send_ticks, checksum = unpacked
        
        packet = {
            'seq_num': seq_num,
            'timestamp_ticks': timestamp,
            'timestamp_ms': ticks_to_ms(timestamp),
            'accel': {'x': ax, 'y': ay, 'z': az},
            'gyro': {'x': gx, 'y': gy, 'z': gz},
            'process_ticks': process_ticks,
            'process_ms': ticks_to_ms(process_ticks),
            'send_ticks': send_ticks,
            'send_ms': ticks_to_ms(send_ticks),
            'total_ms': ticks_to_ms(process_ticks + send_ticks),
            'checksum_ok': (expected_checksum == actual_checksum),
            'rx_time': time.time()  # PC 接收时间
        }
        return packet
    except struct.error:
        return None

# ==================== 性能分析器 ====================
class PerformanceAnalyzer:
    def __init__(self, port, baudrate):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.buffer = bytearray()
        
        # 统计
        self.packets = []
        self.total_packets = 0
        self.valid_packets = 0
        self.lost_packets = 0
        self.last_seq = None
        
        # 性能数据
        self.process_times = []
        self.send_times = []
        self.total_times = []
        self.intervals = []
        
        self.start_time = None
        self.last_print_time = 0
        self.last_packet_time = None
        
    def connect(self):
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1.0
            )
            print(f"✓ Connected to {self.port}")
            print(f"✓ Baudrate: {self.baudrate} bps")
            print(f"✓ GPT1 Clock: {GPT1_FREQ_HZ} Hz (~{GPT1_FREQ_HZ/1000:.0f} kHz)")
            print("-" * 80)
            self.start_time = time.time()
            return True
        except serial.SerialException as e:
            print(f"✗ Error: {e}")
            return False
    
    def find_sync(self):
        while len(self.buffer) >= 2:
            if self.buffer[0] == 0xAA and self.buffer[1] == 0x55:
                return True
            self.buffer.pop(0)
        return False
    
    def receive(self, duration=30):
        """接收指定时长的数据（秒）"""
        print(f"📊 Collecting data for {duration} seconds...")
        print()
        
        end_time = time.time() + duration
        
        try:
            while time.time() < end_time:
                if self.ser.in_waiting > 0:
                    data = self.ser.read(self.ser.in_waiting)
                    self.buffer.extend(data)
                
                if not self.find_sync():
                    time.sleep(0.01)
                    continue
                
                if len(self.buffer) < PACKET_SIZE:
                    time.sleep(0.01)
                    continue
                
                packet_data = bytes(self.buffer[:PACKET_SIZE])
                self.buffer = self.buffer[PACKET_SIZE:]
                
                self.total_packets += 1
                packet = parse_packet(packet_data)
                
                if packet:
                    self.process_packet(packet)
                
                # 定期打印进度
                current_time = time.time()
                if current_time - self.last_print_time >= 2.0:
                    remaining = end_time - current_time
                    print(f"⏱️  {remaining:.0f}s remaining... Packets: {self.valid_packets}")
                    self.last_print_time = current_time
        
        except KeyboardInterrupt:
            print("\n⚠️  Stopped by user")
        
        print("\n" + "=" * 80)
        self.analyze()
    
    def process_packet(self, packet):
        self.valid_packets += 1
        self.packets.append(packet)
        
        # 检测丢包
        seq = packet['seq_num']
        if self.last_seq is not None:
            expected_seq = (self.last_seq + 1) & 0xFFFF
            if seq != expected_seq:
                lost = (seq - expected_seq) & 0xFFFF
                self.lost_packets += lost
        self.last_seq = seq
        
        # 记录性能数据
        self.process_times.append(packet['process_ms'])
        self.send_times.append(packet['send_ms'])
        self.total_times.append(packet['total_ms'])
        
        # 记录接收间隔
        if self.last_packet_time:
            interval = (packet['rx_time'] - self.last_packet_time) * 1000  # ms
            self.intervals.append(interval)
        self.last_packet_time = packet['rx_time']
    
    def analyze(self):
        """详细性能分析"""
        if not self.packets:
            print("⚠️  No packets received!")
            return
        
        print("📈 PERFORMANCE ANALYSIS - BASELINE STAGE")
        print("=" * 80)
        
        # 基本统计
        print("\n【Basic Statistics】")
        print(f"  Total packets received:    {self.total_packets}")
        print(f"  Valid packets:             {self.valid_packets}")
        print(f"  Lost packets:              {self.lost_packets}")
        
        if self.valid_packets > 0:
            loss_rate = (self.lost_packets / (self.valid_packets + self.lost_packets)) * 100
            print(f"  Packet loss rate:          {loss_rate:.2f}%")
        
        elapsed = time.time() - self.start_time
        print(f"  Collection time:           {elapsed:.1f} seconds")
        
        # 吞吐率
        print("\n【Throughput】")
        avg_rate = self.valid_packets / elapsed
        bytes_per_sec = avg_rate * PACKET_SIZE
        print(f"  Average packet rate:       {avg_rate:.2f} packets/s")
        print(f"  Average throughput:        {bytes_per_sec:.0f} bytes/s")
        print(f"  Expected @ 115200 bps:     ~11520 bytes/s")
        print(f"  Efficiency:                {bytes_per_sec/11520*100:.1f}%")
        
        # 处理时间分析
        print("\n【Processing Time】(Sensor Read)")
        self.print_stats("  ", self.process_times, "ms")
        
        # 发送时间分析
        print("\n【Send Time】(UART Transmit)")
        self.print_stats("  ", self.send_times, "ms")
        
        # 总时间分析
        print("\n【Total Time】(Process + Send)")
        self.print_stats("  ", self.total_times, "ms")
        
        # 间隔分析
        if len(self.intervals) > 0:
            print("\n【Sampling Interval】(PC Side)")
            self.print_stats("  ", self.intervals, "ms")
        
        # CPU 占用率估算
        print("\n【CPU Usage Estimation】")
        if self.intervals:
            avg_work = statistics.mean(self.total_times)
            avg_period = statistics.mean(self.intervals)
            cpu_usage = (avg_work / avg_period) * 100
            idle_pct = 100 - cpu_usage
            
            print(f"  Average work time:         {avg_work:.1f} ms")
            print(f"  Average period:            {avg_period:.1f} ms")
            print(f"  CPU busy (blocking):       {cpu_usage:.1f}% ⚠️")
            print(f"  CPU idle (wasted):         {idle_pct:.1f}%")
            print(f"  → This is the problem Baseline exposes!")
        
        # 关键发现
        print("\n【Key Findings - Baseline Issues】")
        print("  ⚠️  Blocking read: CPU waits during sensor SPI transfer")
        print("  ⚠️  Blocking send: CPU waits during UART transmission")
        print("  ⚠️  No concurrency: Cannot do anything else while waiting")
        print(f"  ⚠️  Low efficiency: {idle_pct:.0f}% CPU time wasted in idle loop")
        
        print("\n【Next Stage Improvement】")
        print("  ✓ IRQ + Ring Buffer: CPU can do other tasks during I/O")
        print("  ✓ Expected CPU usage: < 20%")
        print("  ✓ Expected throughput: Same, but with concurrency")
        
        print("=" * 80)
    
    def print_stats(self, prefix, data, unit):
        """打印统计数据"""
        if not data:
            print(f"{prefix}No data")
            return
        
        mean_val = statistics.mean(data)
        median_val = statistics.median(data)
        min_val = min(data)
        max_val = max(data)
        
        if len(data) > 1:
            stdev_val = statistics.stdev(data)
        else:
            stdev_val = 0
        
        print(f"{prefix}Mean:   {mean_val:7.2f} {unit}")
        print(f"{prefix}Median: {median_val:7.2f} {unit}")
        print(f"{prefix}Min:    {min_val:7.2f} {unit}")
        print(f"{prefix}Max:    {max_val:7.2f} {unit}")
        print(f"{prefix}StdDev: {stdev_val:7.2f} {unit}")
    
    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

# ==================== 主程序 ====================
def main():
    port = sys.argv[1] if len(sys.argv) > 1 else SERIAL_PORT
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    
    print("=" * 80)
    print("🚀 BASELINE PERFORMANCE ANALYZER")
    print("=" * 80)
    
    analyzer = PerformanceAnalyzer(port, BAUD_RATE)
    
    if analyzer.connect():
        try:
            analyzer.receive(duration)
        finally:
            analyzer.close()
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()
