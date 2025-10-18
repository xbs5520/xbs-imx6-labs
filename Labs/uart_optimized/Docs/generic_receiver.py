#!/usr/bin/env python3
"""
create by gpt
get data for all stage（Baseline, IRQ, DMA)

功能：
1. 接收串口数据包
2. 实时统计性能指标
3. 保存原始数据到 JSON
4. 打印统计摘要

使用方法：
python generic_receiver.py [--duration 30] [--output result.json]
"""

import serial
import struct
import time
import sys
import argparse
import json
from collections import deque
import statistics
from datetime import datetime

# ==================== configuration ====================
SERIAL_PORT = 'COM5'  #port
BAUD_RATE = 115200
PACKET_SIZE = 30

# GPT1 实测频率
GPT1_FREQ_HZ = 645000  # 约 645 kHz

# ==================== data format ====================
# typedef struct {
#     uint8_t header[2];         // 0xAA 0x55
#     uint16_t seq_num;          // 序号
#     uint32_t timestamp;        // GPT1 ticks
#     int16_t accel_x, y, z;     // 加速度
#     int16_t gyro_x, y, z;      // 陀螺仪
#     uint32_t process_time_us;  // 处理时间
#     uint32_t send_time_us;     // 发送时间
#     uint8_t checksum;          // 校验和
#     uint8_t padding;           // 填充字节
# } __attribute__((packed)) sensor_packet_t;

PACKET_FORMAT = '<2sHI6h2I2B'

# ==================== 辅助函数 ====================
def ticks_to_ms(ticks):
    """GPT1 ticks → 毫秒"""
    return (ticks * 1000.0) / GPT1_FREQ_HZ

def calculate_checksum(data):
    """计算校验和（不包括最后2个字节）"""
    return sum(data[:-2]) & 0xFF

# ==================== 统计类 ====================
class DataCollector:
    def __init__(self):
        self.start_time = time.time()
        
        # 数据包统计
        self.valid_packets = 0
        self.checksum_errors = 0
        self.total_bytes = 0
        
        # 原始数据存储
        self.raw_intervals = []      # 采样间隔 (ms)
        self.raw_timestamps = []     # 时间戳 (ticks)
        self.raw_process_times = []  # 处理时间 (ms)
        self.raw_send_times = []     # 发送时间 (ms)
        
        # 最后一个时间戳
        self.last_timestamp = None
        
    def update(self, packet_data):
        """更新统计信息"""
        self.valid_packets += 1
        self.total_bytes += PACKET_SIZE
        
        # 解析数据包
        header, seq_num, timestamp, ax, ay, az, gx, gy, gz, proc_time, send_time, checksum, padding = packet_data
        
        # 保存时间戳
        self.raw_timestamps.append(timestamp)
        
        # 计算采样间隔
        if self.last_timestamp is not None:
            # 处理 32-bit 计数器溢出
            if timestamp > self.last_timestamp:
                delta_ticks = timestamp - self.last_timestamp
            else:
                delta_ticks = (0xFFFFFFFF - self.last_timestamp) + timestamp
            
            interval_ms = ticks_to_ms(delta_ticks)
            
            # 过滤异常值（保留合理范围）
            if 10 < interval_ms < 200:
                self.raw_intervals.append(interval_ms)
        
        self.last_timestamp = timestamp
        
        # 性能时间统计
        proc_ms = ticks_to_ms(proc_time)
        send_ms = ticks_to_ms(send_time)
        
        if proc_ms < 100:  # 过滤异常值
            self.raw_process_times.append(proc_ms)
        if send_ms < 100:
            self.raw_send_times.append(send_ms)
    
    def get_statistics(self):
        """计算统计信息"""
        elapsed = time.time() - self.start_time
        
        stats = {
            'test_info': {
                'timestamp': datetime.now().isoformat(),
                'duration_s': round(elapsed, 2),
                'packet_count': self.valid_packets,
                'checksum_errors': self.checksum_errors,
                'total_bytes': self.total_bytes,
                'packet_rate_pps': round(self.valid_packets / elapsed, 2) if elapsed > 0 else 0,
                'throughput_bps': round(self.total_bytes / elapsed, 2) if elapsed > 0 else 0
            },
            'timing': {},
            'performance': {},
            'raw_data': {
                'intervals_ms': self.raw_intervals,
                'timestamps_ticks': self.raw_timestamps,
                'process_times_ms': self.raw_process_times,
                'send_times_ms': self.raw_send_times
            }
        }
        
        # 定时精度统计
        if len(self.raw_intervals) > 0:
            intervals = self.raw_intervals
            mean_interval = statistics.mean(intervals)
            median_interval = statistics.median(intervals)
            stdev_interval = statistics.stdev(intervals) if len(intervals) > 1 else 0
            min_interval = min(intervals)
            max_interval = max(intervals)
            
            # 计算抖动（最大偏差）
            jitter = max(abs(max_interval - mean_interval), abs(mean_interval - min_interval))
            
            stats['timing'] = {
                'mean_interval_ms': round(mean_interval, 3),
                'median_interval_ms': round(median_interval, 3),
                'stdev_ms': round(stdev_interval, 3),
                'min_interval_ms': round(min_interval, 3),
                'max_interval_ms': round(max_interval, 3),
                'jitter_ms': round(jitter, 3),
                'cv_percent': round((stdev_interval / mean_interval * 100), 2) if mean_interval > 0 else 0
            }
        
        # 性能统计
        if len(self.raw_process_times) > 0:
            stats['performance']['process_time_ms'] = {
                'mean': round(statistics.mean(self.raw_process_times), 2),
                'median': round(statistics.median(self.raw_process_times), 2),
                'min': round(min(self.raw_process_times), 2),
                'max': round(max(self.raw_process_times), 2)
            }
        
        if len(self.raw_send_times) > 0:
            stats['performance']['send_time_ms'] = {
                'mean': round(statistics.mean(self.raw_send_times), 2),
                'median': round(statistics.median(self.raw_send_times), 2),
                'min': round(min(self.raw_send_times), 2),
                'max': round(max(self.raw_send_times), 2)
            }
        
        return stats
    
    def print_realtime(self):
        """打印实时统计"""
        elapsed = time.time() - self.start_time
        
        print(f"\r包: {self.valid_packets:4d} | "
              f"错误: {self.checksum_errors:3d} | "
              f"速率: {self.valid_packets/elapsed if elapsed > 0 else 0:5.1f} pkt/s | "
              f"时间: {elapsed:6.1f}s", end='', flush=True)

# ==================== 主函数 ====================
def receive_data(duration_seconds=30, output_file='result.json'):
    """接收数据"""
    print("\n" + "="*60)
    print("  通用数据接收器")
    print("="*60)
    print(f"串口: {SERIAL_PORT} @ {BAUD_RATE}")
    print(f"包大小: {PACKET_SIZE} 字节")
    print(f"测试时长: {duration_seconds} 秒")
    print(f"输出文件: {output_file}")
    print("="*60 + "\n")
    
    collector = DataCollector()
    
    try:
        # 打开串口
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"串口已打开: {ser.name}")
        print("开始接收数据...\n")
        
        buffer = bytearray()
        test_start = time.time()
        
        while True:
            # 检查测试时长
            if time.time() - test_start > duration_seconds:
                print("\n\n测试时间到，停止接收。")
                break
            
            # 读取串口数据
            if ser.in_waiting > 0:
                buffer.extend(ser.read(ser.in_waiting))
            
            # 查找数据包
            while len(buffer) >= PACKET_SIZE:
                # 查找包头 0xAA 0x55
                idx = buffer.find(b'\xAA\x55')
                
                if idx == -1:
                    buffer.clear()
                    break
                
                # 丢弃包头之前的数据
                if idx > 0:
                    buffer = buffer[idx:]
                
                # 检查是否有完整的包
                if len(buffer) < PACKET_SIZE:
                    break
                
                # 提取数据包
                packet_bytes = bytes(buffer[:PACKET_SIZE])
                buffer = buffer[PACKET_SIZE:]
                
                # 校验和检查
                expected_checksum = calculate_checksum(packet_bytes)
                
                try:
                    packet_data = struct.unpack(PACKET_FORMAT, packet_bytes)
                    actual_checksum = packet_data[-2]
                    
                    if actual_checksum == expected_checksum:
                        collector.update(packet_data)
                        collector.print_realtime()
                    else:
                        collector.checksum_errors += 1
                        
                except struct.error as e:
                    print(f"\n解析错误: {e}")
                    continue
            
            time.sleep(0.001)
        
        ser.close()
        
    except serial.SerialException as e:
        print(f"\n串口错误: {e}")
        return None
    except KeyboardInterrupt:
        print("\n\n用户中断测试。")
    
    # 获取统计结果
    stats = collector.get_statistics()
    
    # 保存到文件
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(stats, f, indent=2, ensure_ascii=False)
    print(f"\n数据已保存到: {output_file}")
    
    return stats

def print_summary(stats):
    """打印统计摘要"""
    print("\n" + "="*60)
    print("  统计摘要")
    print("="*60)
    
    # 基本信息
    info = stats['test_info']
    print(f"\n【测试信息】")
    print(f"  时间:       {info['timestamp']}")
    print(f"  时长:       {info['duration_s']} 秒")
    print(f"  数据包:     {info['packet_count']} 个")
    print(f"  错误:       {info['checksum_errors']} 个")
    print(f"  包速率:     {info['packet_rate_pps']} pkt/s")
    print(f"  吞吐量:     {info['throughput_bps']} B/s")
    
    # 定时精度
    if 'timing' in stats and stats['timing']:
        t = stats['timing']
        print(f"\n【定时精度】")
        print(f"  采样间隔:   {t['mean_interval_ms']:.3f} ms (标准差 {t['stdev_ms']:.3f} ms)")
        print(f"  抖动:       ±{t['jitter_ms']:.3f} ms")
        print(f"  范围:       [{t['min_interval_ms']:.3f}, {t['max_interval_ms']:.3f}] ms")
        print(f"  变异系数:   {t['cv_percent']:.2f}%")
    
    # 性能指标
    if 'performance' in stats and stats['performance']:
        p = stats['performance']
        print(f"\n【性能指标】")
        if 'process_time_ms' in p:
            print(f"  传感器读取: {p['process_time_ms']['mean']:.2f} ms "
                  f"(min: {p['process_time_ms']['min']:.2f}, max: {p['process_time_ms']['max']:.2f})")
        if 'send_time_ms' in p:
            print(f"  UART 发送:  {p['send_time_ms']['mean']:.2f} ms "
                  f"(min: {p['send_time_ms']['min']:.2f}, max: {p['send_time_ms']['max']:.2f})")
        
        # CPU 使用率估算
        if 'process_time_ms' in p and 'send_time_ms' in p and 'timing' in stats:
            cpu_usage = (p['process_time_ms']['mean'] + p['send_time_ms']['mean']) / stats['timing']['mean_interval_ms'] * 100
            print(f"  CPU 使用率: {cpu_usage:.1f}% (估算)")
    
    print("\n" + "="*60)

# ==================== 命令行入口 ====================
def main():
    global SERIAL_PORT  # 声明全局变量
    
    parser = argparse.ArgumentParser(description='通用数据接收器')
    parser.add_argument('--duration', type=int, default=30, 
                        help='测试时长（秒），默认 30 秒')
    parser.add_argument('--output', type=str, default='result.json', 
                        help='输出文件名，默认 result.json')
    parser.add_argument('--port', type=str, default=SERIAL_PORT, 
                        help=f'串口号，默认 {SERIAL_PORT}')
    
    args = parser.parse_args()
    
    # 更新全局串口配置
    SERIAL_PORT = args.port
    
    # 接收数据
    stats = receive_data(args.duration, args.output)
    
    # 打印摘要
    if stats:
        print_summary(stats)
        print(f"\n✅ 完成！数据已保存到 {args.output}")
        print(f"\n提示: 可以使用多次测试，然后手动对比 JSON 文件中的数据。")

if __name__ == '__main__':
    main()
