#!/usr/bin/env python3
"""
Baseline Data Receiver for IMX6ULL Project
接收并解析板子发送的传感器数据包
"""

import serial
import struct
import time
import sys
from collections import deque

# ==================== 配置参数 ====================
# SERIAL_PORT = '/dev/ttyUSB0'  # Linux
SERIAL_PORT = 'COM5'            # Windows - 你的配置
BAUD_RATE = 115200
PACKET_SIZE = 30  # 修改：29->30 (添加了padding字节)

# GPT1 实测频率 (根据你的测试结果)
GPT1_FREQ_HZ = 645000  # 约 645 kHz

# ==================== 数据包格式 ====================
# typedef struct {
#     uint8_t header[2];         // 0xAA 0x55
#     uint16_t seq_num;          // 序号 (调整顺序以对齐)
#     uint32_t timestamp;        // GPT1 ticks
#     int16_t accel_x, y, z;     // 加速度
#     int16_t gyro_x, y, z;      // 陀螺仪
#     uint32_t process_time_us;  // 处理时间 (ticks)
#     uint32_t send_time_us;     // 发送时间 (ticks)
#     uint8_t checksum;          // 校验和
#     uint8_t padding;           // 填充字节
# } __attribute__((packed)) sensor_packet_t;

# 小端格式 (Little Endian)
PACKET_FORMAT = '<2sHI6h2I2B'
# 2s: header (2 bytes)
# H:  seq_num (2 bytes unsigned) - 调整位置
# I:  timestamp (4 bytes unsigned)
# 6h: accel_x,y,z + gyro_x,y,z (6 × 2 bytes signed)
# 2I: process_time, send_time (2 × 4 bytes unsigned)
# 2B: checksum + padding (2 bytes)

# ==================== 辅助函数 ====================
def ticks_to_ms(ticks):
    """将 GPT1 ticks 转换为毫秒"""
    return (ticks * 1000.0) / GPT1_FREQ_HZ

def ticks_to_us(ticks):
    """将 GPT1 ticks 转换为微秒"""
    return (ticks * 1000000.0) / GPT1_FREQ_HZ

def calculate_checksum(data):
    """计算校验和（前29字节的和，排除checksum和padding）"""
    return sum(data[:PACKET_SIZE-2]) & 0xFF

def parse_packet(data):
    """解析数据包"""
    if len(data) != PACKET_SIZE:
        return None
    
    # 检查帧头
    if data[0] != 0xAA or data[1] != 0x55:
        return None
    
    # 校验和
    expected_checksum = calculate_checksum(data)
    actual_checksum = data[PACKET_SIZE-2]  # checksum 在倒数第2字节
    if expected_checksum != actual_checksum:
        print(f"[WARNING] Checksum mismatch: expected {expected_checksum:02X}, got {actual_checksum:02X}")
        # 继续解析，只是警告
    
    # 解包
    try:
        unpacked = struct.unpack(PACKET_FORMAT, data)
        header, seq_num, timestamp, ax, ay, az, gx, gy, gz, process_ticks, send_ticks, checksum, padding = unpacked
        
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
            'checksum_ok': (expected_checksum == actual_checksum)
        }
        return packet
    except struct.error as e:
        print(f"[ERROR] Unpack error: {e}")
        return None

# ==================== 主接收逻辑 ====================
class PacketReceiver:
    def __init__(self, port, baudrate):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.buffer = bytearray()
        
        # 统计信息
        self.total_packets = 0
        self.valid_packets = 0
        self.checksum_errors = 0
        self.lost_packets = 0
        self.last_seq = None
        
        # 性能统计
        self.throughput_window = deque(maxlen=100)  # 最近100个包
        self.start_time = None
        self.last_print_time = 0
        
    def connect(self):
        """连接串口"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1.0
            )
            print(f"[INFO] Connected to {self.port} @ {self.baudrate} bps")
            print(f"[INFO] Packet size: {PACKET_SIZE} bytes")
            print(f"[INFO] GPT1 frequency: {GPT1_FREQ_HZ} Hz")
            print("-" * 70)
            self.start_time = time.time()
            return True
        except serial.SerialException as e:
            print(f"[ERROR] Cannot open port {self.port}: {e}")
            return False
    
    def find_sync(self):
        """在缓冲区中查找帧头 0xAA 0x55"""
        while len(self.buffer) >= 2:
            if self.buffer[0] == 0xAA and self.buffer[1] == 0x55:
                return True
            # 丢弃一个字节，继续查找
            self.buffer.pop(0)
        return False
    
    def receive(self):
        """主接收循环"""
        print("[INFO] Waiting for data...")
        print()
        
        try:
            while True:
                # 读取数据
                if self.ser.in_waiting > 0:
                    data = self.ser.read(self.ser.in_waiting)
                    self.buffer.extend(data)
                
                # 查找帧头
                if not self.find_sync():
                    time.sleep(0.01)
                    continue
                
                # 等待完整的数据包
                if len(self.buffer) < PACKET_SIZE:
                    time.sleep(0.01)
                    continue
                
                # 提取一个完整包
                packet_data = bytes(self.buffer[:PACKET_SIZE])
                self.buffer = self.buffer[PACKET_SIZE:]
                
                # 解析数据包
                self.total_packets += 1
                packet = parse_packet(packet_data)
                
                if packet:
                    self.process_packet(packet)
                else:
                    print("[ERROR] Invalid packet")
        
        except KeyboardInterrupt:
            print("\n[INFO] Stopped by user")
            self.print_statistics()
        except Exception as e:
            print(f"\n[ERROR] Exception: {e}")
            import traceback
            traceback.print_exc()
    
    def process_packet(self, packet):
        """处理接收到的数据包"""
        self.valid_packets += 1
        
        if not packet['checksum_ok']:
            self.checksum_errors += 1
        
        # 检测丢包
        seq = packet['seq_num']
        if self.last_seq is not None:
            expected_seq = (self.last_seq + 1) & 0xFFFF  # 16位序号
            if seq != expected_seq:
                lost = (seq - expected_seq) & 0xFFFF
                self.lost_packets += lost
                print(f"[WARNING] Lost {lost} packet(s). Last: {self.last_seq}, Current: {seq}")
        self.last_seq = seq
        
        # 记录吞吐量
        self.throughput_window.append(time.time())
        
        # 定期打印（每秒一次）
        current_time = time.time()
        if current_time - self.last_print_time >= 1.0:
            self.print_packet(packet)
            self.last_print_time = current_time
    
    def print_packet(self, pkt):
        """打印数据包信息"""
        seq = pkt['seq_num']
        ax, ay, az = pkt['accel']['x'], pkt['accel']['y'], pkt['accel']['z']
        gx, gy, gz = pkt['gyro']['x'], pkt['gyro']['y'], pkt['gyro']['z']
        
        # 时间信息 (重点关注)
        process_ms = pkt['process_ms']
        send_ms = pkt['send_ms']
        total_ms = process_ms + send_ms
        
        # 吞吐率
        if len(self.throughput_window) >= 2:
            time_span = self.throughput_window[-1] - self.throughput_window[0]
            if time_span > 0:
                packets_per_sec = (len(self.throughput_window) - 1) / time_span
                bytes_per_sec = packets_per_sec * PACKET_SIZE
            else:
                packets_per_sec = 0
                bytes_per_sec = 0
        else:
            packets_per_sec = 0
            bytes_per_sec = 0
        
        # 打印（每秒一次）
        print(f"[Seq {seq:5d}] "
              f"Accel:({ax:5d},{ay:5d},{az:5d}) "
              f"Gyro:({gx:5d},{gy:5d},{gz:5d}) | "
              f"Process:{process_ms:5.1f}ms "
              f"Send:{send_ms:5.1f}ms "
              f"Total:{total_ms:5.1f}ms | "
              f"Rate:{packets_per_sec:4.1f}pkt/s {bytes_per_sec:5.0f}B/s")
    
    def print_statistics(self):
        """打印统计信息"""
        print("\n" + "=" * 70)
        print("Statistics:")
        print("-" * 70)
        print(f"Total packets received:  {self.total_packets}")
        print(f"Valid packets:           {self.valid_packets}")
        print(f"Checksum errors:         {self.checksum_errors}")
        print(f"Lost packets:            {self.lost_packets}")
        
        if self.valid_packets > 0:
            loss_rate = (self.lost_packets / (self.valid_packets + self.lost_packets)) * 100
            print(f"Packet loss rate:        {loss_rate:.2f}%")
        
        if self.start_time:
            elapsed = time.time() - self.start_time
            print(f"Total time:              {elapsed:.1f} seconds")
            if elapsed > 0:
                avg_rate = self.valid_packets / elapsed
                print(f"Average packet rate:     {avg_rate:.2f} packets/s")
                print(f"Average throughput:      {avg_rate * PACKET_SIZE:.0f} bytes/s")
        
        print("=" * 70)
    
    def close(self):
        """关闭串口"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[INFO] Serial port closed")

# ==================== 主程序 ====================
def main():
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = SERIAL_PORT
    
    print("=" * 70)
    print("Baseline Data Receiver - IMX6ULL Project")
    print("=" * 70)
    print(f"Port: {port}")
    print(f"Baud: {BAUD_RATE}")
    print("-" * 70)
    
    receiver = PacketReceiver(port, BAUD_RATE)
    
    if receiver.connect():
        try:
            receiver.receive()
        finally:
            receiver.close()
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()
