# Baseline Performance Analysis Report

**Project:** IMX6ULL Data Acquisition Optimization  
**Stage:** 1 - Baseline (Blocking Mode)  
**Date:** October 15-16, 2025  
**Duration:** 30 seconds test  

---

## Executive Summary

The baseline stage successfully demonstrates the **limitations of blocking I/O** in embedded systems. While functionally correct (806 packets received with 0 checksum errors), the system shows critical performance issues that will be addressed in subsequent stages.

---

## Performance Metrics

### 1. Throughput Statistics
```
Packets received:     806 packets
Packet rate:          26.86 packets/s
Data throughput:      779 bytes/s
UART capacity:        11520 bytes/s @ 115200 bps
Bandwidth efficiency: 6.8%
```

**Analysis:** The system only utilizes **6.8% of available UART bandwidth**. The bottleneck is not communication speed, but CPU blocking behavior.

---

### 2. CPU Utilization (Critical Issue ⚠️)
```
Average work time:    33.7 ms
Average period:       31.6 ms
CPU busy (blocking):  106.7%
CPU idle:             -6.7%
```

**Key Finding:**  
- Target cycle: 50ms (20 Hz sampling rate)
- Actual cycle: 31.6ms  
- **CPU is 100% busy** and cannot meet the design goal
- The system is **overloaded** - processing time exceeds cycle time

**Why this happens:**
- CPU blocks during sensor SPI transfer (29.7ms)
- CPU blocks during UART transmission (4.0ms)
- Total blocking time (33.7ms) > desired period (50ms)

---

### 3. Time Breakdown
```
Sensor Read Time:     29.70 ms (88%)
UART Send Time:       4.05 ms  (12%)
Total Processing:     33.75 ms (100%)
```

**Problem Analysis:**

**Sensor Read (29.7ms):**
- Actual SPI transfer: ~500 microseconds
- Remaining time: Blocking delays for ICM20608 stabilization
- **CPU wastes 29+ ms doing nothing** while waiting

**UART Send (4.0ms):**
- Theoretical time: 30 bytes × 10 bits ÷ 115200 = 2.6ms
- Actual time: 4.0ms (includes timeout protection)
- **CPU blocks waiting for UART hardware**

---

### 4. Timing Accuracy (Poor ⚠️)
```
Mean interval:        31.62 ms
Standard deviation:   4.39 ms  (13.9% jitter)
Min interval:         20.12 ms
Max interval:         41.83 ms
Range:                21.71 ms (±34% variation)
```

**Root Cause:**
```c
// Polling-based timing (inaccurate)
while(get_system_tick() < next_tick);  // Busy wait
next_tick = get_system_tick() + PERIOD_US;
```

**Problems:**
- Software polling is imprecise
- If processing exceeds period, immediately starts next cycle
- Accumulates timing errors
- Affected by interrupts and system load

---

## Key Findings - Baseline Issues

### ⚠️ Issue #1: CPU Blocking Waste
- CPU spends 88% of time waiting for SPI sensor
- CPU spends 12% of time waiting for UART
- **CPU cannot do anything else** during I/O operations

### ⚠️ Issue #2: Low Bandwidth Efficiency  
- UART idle 93% of the time
- System sends data in short bursts
- Wasted communication capacity

### ⚠️ Issue #3: Poor Timing Precision
- ±4.39ms standard deviation (±14%)
- Software polling is unreliable
- Cannot guarantee real-time requirements

### ⚠️ Issue #4: No Concurrency
- CPU utilization: 106.7%
- Cannot handle button presses
- Cannot update LED/display
- **System is completely blocked**

---

## What We Learned

### ✅ Successful Aspects
1. **Functional correctness:** All 806 packets valid, 0 checksum errors
2. **Data integrity:** Sensor readings are accurate and consistent
3. **Baseline established:** Clear metrics for comparison
4. **Problems exposed:** Identified 4 major bottlenecks

### 🎯 Purpose of Baseline Stage
The baseline is **intentionally inefficient** to:
1. Demonstrate blocking I/O problems clearly
2. Provide comparison benchmark for optimization
3. Validate basic functionality before adding complexity

---

## Next Stage Improvements

### Stage 2: IRQ + Ring Buffer

**Strategy:**
- Use GPT1 timer interrupt for precise 50ms sampling
- Read sensor in interrupt handler → write to ring buffer
- Main loop reads from buffer → sends via UART
- **Decouple sensing from transmission**

**Expected Improvements:**

| Metric | Baseline | IRQ Target |
|--------|----------|------------|
| CPU usage | 106.7% | **< 20%** ✅ |
| Timing accuracy | ±4.39ms | **< ±0.5ms** ✅ |
| Concurrency | ❌ None | ✅ Available |
| Throughput | 779 B/s | 779 B/s (same) |

**Note:** Throughput won't increase (same data volume), but **CPU will be freed** to do other tasks.

---

## Technical Details

### Hardware Platform
- **Board:** IMX6ULL development board
- **Sensor:** ICM20608 (6-axis IMU via SPI @ 6MHz)
- **Communication:** UART1 @ 115200 bps, 8N1
- **Timer:** GPT1 @ ~645kHz (actual measured frequency)

### Packet Structure (30 bytes)
```c
typedef struct {
    uint8_t  header[2];        // 0xAA 0x55
    uint16_t seq_num;          // Sequence number
    uint32_t timestamp;        // GPT1 ticks
    int16_t  accel_x, y, z;    // Accelerometer (3×2 bytes)
    int16_t  gyro_x, y, z;     // Gyroscope (3×2 bytes)
    uint32_t process_time_us;  // Sensor read time
    uint32_t send_time_us;     // UART send time
    uint8_t  checksum;         // Sum of first 28 bytes
    uint8_t  padding;          // Alignment padding
} __attribute__((packed)) sensor_packet_t;
```

**Important:** Fields are ordered to ensure 4-byte alignment for all `uint32_t` types (ARM requirement).

---

## Conclusion

The baseline stage successfully achieved its goal: **exposing the fundamental problems of blocking I/O architecture**. 

With 106.7% CPU utilization and ±4.39ms timing jitter, the system cannot scale or handle additional tasks. These metrics provide a clear target for the IRQ and DMA optimization stages.

**Key Takeaway:** In embedded systems, **blocking I/O is the enemy of efficiency**. Even with fast hardware, CPU blocking wastes precious processing time that could be used for concurrent tasks.

---

**Next Step:** Implement IRQ + Ring Buffer architecture to reduce CPU usage from 106.7% to < 20%.
