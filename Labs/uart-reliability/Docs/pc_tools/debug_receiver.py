#!/usr/bin/env python3
"""
调试版接收器 - 显示所有接收到的原始字节
"""

import serial
import sys
import time

SERIAL_PORT = 'COM5'
BAUD_RATE = 115200

def main():
    print("=" * 70)
    print("DEBUG Receiver - Shows all raw bytes")
    print("=" * 70)
    
    try:
        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            timeout=1.0
        )
        print(f"✓ Connected to {SERIAL_PORT} @ {BAUD_RATE}")
        print("-" * 70)
        print("Waiting for data...")
        print()
        
        byte_count = 0
        last_print = time.time()
        
        while True:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                byte_count += len(data)
                
                # 显示原始字节（16进制）
                hex_str = ' '.join(f'{b:02X}' for b in data)
                print(f"Received {len(data):3d} bytes: {hex_str[:100]}{'...' if len(hex_str) > 100 else ''}")
                
                # 查找帧头
                for i in range(len(data) - 1):
                    if data[i] == 0xAA and data[i+1] == 0x55:
                        print(f"  → Found header at byte {i}!")
                
                # 显示可打印字符（如果有）
                try:
                    text = data.decode('ascii', errors='ignore')
                    if text.strip():
                        print(f"  Text: {text.strip()}")
                except:
                    pass
                
                print()
            
            # 每5秒打印统计
            if time.time() - last_print >= 5.0:
                print(f"[STATS] Total bytes received: {byte_count}")
                last_print = time.time()
            
            time.sleep(0.01)
    
    except KeyboardInterrupt:
        print("\nStopped by user")
    except serial.SerialException as e:
        print(f"Error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    main()
