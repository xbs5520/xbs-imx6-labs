# PC 端接收工具使用说明

## 文件说明

### 1. baseline_receiver.py - 实时监控
**功能**：实时接收并显示传感器数据，每秒打印一次

**使用方法**：
```bash
# Linux
python3 baseline_receiver.py /dev/ttyUSB0

# Windows
python baseline_receiver.py COM3
```

**输出示例**：
```
[Seq  1234] Accel:(  -6,   4,2039) Gyro:( 11, 11,  4) | Process: 29.7ms Send:  4.0ms Total: 33.7ms | Rate:20.1pkt/s  583B/s
```

**说明**：
- `Seq`: 数据包序号
- `Accel/Gyro`: 传感器原始ADC值
- `Process`: 传感器读取时间
- `Send`: UART发送时间
- `Rate`: 实时吞吐率

---

### 2. performance_analyzer.py - 性能分析
**功能**：收集指定时长的数据并进行详细统计分析

**使用方法**：
```bash
# 收集30秒数据
python3 performance_analyzer.py /dev/ttyUSB0 30

# 收集60秒数据
python3 performance_analyzer.py /dev/ttyUSB0 60
```

**输出示例**：
```
📈 PERFORMANCE ANALYSIS - BASELINE STAGE
===============================================================================

【Basic Statistics】
  Total packets received:    612
  Valid packets:             612
  Lost packets:              0
  Packet loss rate:          0.00%
  Collection time:           30.5 seconds

【Throughput】
  Average packet rate:       20.07 packets/s
  Average throughput:        582 bytes/s
  Expected @ 115200 bps:     ~11520 bytes/s
  Efficiency:                5.1%

【Processing Time】(Sensor Read)
  Mean:     29.72 ms
  Median:   29.70 ms
  Min:      29.50 ms
  Max:      30.10 ms
  StdDev:    0.15 ms

【Send Time】(UART Transmit)
  Mean:      4.03 ms
  Median:    4.00 ms
  Min:       3.95 ms
  Max:       4.20 ms
  StdDev:    0.08 ms

【Total Time】(Process + Send)
  Mean:     33.75 ms
  Median:   33.70 ms
  Min:      33.50 ms
  Max:      34.10 ms
  StdDev:    0.18 ms

【Sampling Interval】(PC Side)
  Mean:     49.88 ms
  Median:   49.85 ms
  Min:      49.20 ms
  Max:      50.50 ms
  StdDev:    0.25 ms

【CPU Usage Estimation】
  Average work time:         33.8 ms
  Average period:            49.9 ms
  CPU busy (blocking):       67.7% ⚠️
  CPU idle (wasted):         32.3%
  → This is the problem Baseline exposes!

【Key Findings - Baseline Issues】
  ⚠️  Blocking read: CPU waits during sensor SPI transfer
  ⚠️  Blocking send: CPU waits during UART transmission
  ⚠️  No concurrency: Cannot do anything else while waiting
  ⚠️  Low efficiency: 32% CPU time wasted in idle loop

【Next Stage Improvement】
  ✓ IRQ + Ring Buffer: CPU can do other tasks during I/O
  ✓ Expected CPU usage: < 20%
  ✓ Expected throughput: Same, but with concurrency
```

---

## 配置说明

在脚本开头修改配置：

```python
# 串口配置
SERIAL_PORT = '/dev/ttyUSB0'  # Linux
# SERIAL_PORT = 'COM3'          # Windows
BAUD_RATE = 115200

# GPT1 时钟频率（根据你的测试结果调整）
GPT1_FREQ_HZ = 645000  # 约 645 kHz
```

### 如何确定 GPT1_FREQ_HZ？

运行板子上的 `test_gpt1_clock()` 函数，查看输出：
```
GPT1 actual frequency: 645203 Hz
```

将这个值填入 `GPT1_FREQ_HZ`。

---

## 依赖安装

```bash
# 安装 pyserial
pip3 install pyserial

# 或者使用系统包管理器（Linux）
sudo apt-get install python3-serial
```

---

## 故障排除

### 1. 找不到串口设备
```bash
# Linux: 查看可用串口
ls /dev/ttyUSB* /dev/ttyACM*

# Linux: 添加用户到串口组
sudo usermod -a -G dialout $USER
# 注销后重新登录

# Windows: 在设备管理器中查看COM端口号
```

### 2. 权限问题（Linux）
```bash
sudo chmod 666 /dev/ttyUSB0
# 或者
sudo usermod -a -G dialout $USER
```

### 3. 校验和错误
如果经常出现校验和错误：
- 检查串口连接是否稳定
- 降低波特率测试（9600）
- 检查板子端的校验和计算是否正确

### 4. 数据解析错误
- 确认板子端数据包格式与脚本一致
- 确认是小端格式（Little Endian）
- 检查 `PACKET_SIZE = 29` 是否正确

---

## 性能基准参考

### Baseline 阶段预期性能：
- **吞吐率**: ~580 bytes/s (20 packets/s)
- **处理时间**: 29-30 ms (传感器读取)
- **发送时间**: 4-5 ms (UART)
- **总时间**: 33-35 ms
- **采样率**: ~20 Hz (50ms 周期)
- **CPU占用**: 67-70% (Blocking)

### 后续阶段对比目标：
- **IRQ + Ring Buffer**: CPU 占用 < 20%
- **DMA Double Buffer**: CPU 占用 < 5%

---

## 数据保存（可选）

如果需要保存原始数据用于后续分析：

```python
# 在 process_packet() 中添加：
with open('baseline_data.csv', 'a') as f:
    f.write(f"{packet['seq_num']},{packet['process_ms']},{packet['send_ms']}\n")
```

---

## 常见问题

**Q: 为什么吞吐率只有 ~580 bytes/s，而波特率是 115200？**

A: 因为：
1. 每个字节需要 10 位（1起始位 + 8数据位 + 1停止位）
2. 理论最大 = 115200 / 10 = 11520 bytes/s
3. 实际 = 29 bytes × 20 Hz = 580 bytes/s
4. 效率 = 580 / 11520 = 5% （其余时间在处理数据和等待）

**Q: 时间换算是否准确？**

A: 取决于 `GPT1_FREQ_HZ` 的准确性：
- 如果 GPT1 实际是 645 kHz，则时间换算准确
- 即使有 ±10% 误差，相对对比仍然有效
- 重要的是三个阶段用同一标准

**Q: send_time 为什么在下一个包才显示？**

A: 因为发送时间是在发送完成后才测量的，所以记录在 `last_send_time` 中，在下一个包的数据中发送出去。第一个包的 `send_time = 0` 是正常的。

---

## 使用建议

1. **先运行 baseline_receiver.py** - 确认数据接收正常
2. **再运行 performance_analyzer.py** - 收集30-60秒数据做详细分析
3. **保存分析结果** - 截图或保存输出，用于项目报告
4. **对比三个阶段** - Baseline → IRQ → DMA 的性能提升

---

## 技术支持

如有问题，检查：
1. 板子端代码是否正确编译和烧录
2. 串口连接是否正常
3. `GPT1_FREQ_HZ` 配置是否与实际一致
4. 数据包格式是否匹配
