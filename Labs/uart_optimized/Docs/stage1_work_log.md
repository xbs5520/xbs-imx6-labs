# Baseline Stage Implementation

**Project:** IMX6ULL Data Acquisition Optimization (3-Stage Performance Comparison)  
**Stage Completed:** Stage 1 - Baseline (Blocking Mode)  

## Overview

Successfully implemented and tested the baseline stage of a 3-stage embedded system optimization project. The goal is to demonstrate performance improvements from **Blocking I/O → IRQ + Ring Buffer → DMA Double Buffer**.

---

## Project Planning & Architecture Design

### 1. Three-Stage Architecture Defined

**Stage 1: Baseline (Blocking)**

- Blocking sensor read via SPI
- Blocking UART transmission
- Software polling for timing
- **Goal:** Expose problems of blocking I/O

**Stage 2: IRQ + Ring Buffer**

- Timer interrupt triggers sampling
- Sensor data stored in ring buffer
- Main loop handles transmission
- **Goal:** Decouple I/O, reduce CPU usage

**Stage 3: DMA Double Buffer**
- DMA handles UART transmission
- Double buffering for continuous operation
- Zero-copy data transfer
- **Goal:** Minimize CPU involvement

### 2. Data Packet Design (30 bytes)
```c
struct sensor_packet_t {
    uint8_t  header[2];         // Frame sync: 0xAA 0x55
    uint16_t seq_num;           // Sequence number
    uint32_t timestamp;         // GPT1 timer ticks
    int16_t  accel[3];          // Accelerometer x,y,z
    int16_t  gyro[3];           // Gyroscope x,y,z
    uint32_t process_time_us;   // Sensor read time
    uint32_t send_time_us;      // UART send time
    uint8_t  checksum;          // Data integrity
    uint8_t  padding;           // Alignment
};
```

### 3. Performance Metrics Identified
- CPU utilization
- Timing accuracy
- Throughput
- Concurrency capability

---

## Critical Bug Discovery & Resolution

### Problem: Board Hangs on Structure Access

**Symptom:**
- Board prints: `[TEST1] Loop iteration 0`
- Then completely freezes
- No further output or data transmission

**Debugging Process:**

1. add debug print
   
2. **Narrowed down:** Structure field access
   
   ```c
   packet.header[0] = 0xAA;     // ✅ OK
   packet.header[1] = 0x55;     // ✅ OK  
   packet.timestamp = seq;      // ❌ HANGS HERE!
   ```
   
3. **Root cause identified:** **Unaligned memory access**

**Technical Details:**

**Original Structure (WRONG):**

```c
typedef struct {
    uint8_t  header[2];      // Offset: 0 (2 bytes)
    uint32_t timestamp;      // Offset: 2 ← NOT 4-byte aligned!
    uint16_t seq_num;        // Offset: 6
    ...
} __attribute__((packed)) sensor_packet_t;
```

**Problem:**
- ARM Cortex-A7 requires 32-bit data to be 4-byte aligned
- `timestamp` starts at offset 2 (not divisible by 4)
- Accessing unaligned `uint32_t` triggers hardware exception
- With `__attribute__((packed))`, compiler doesn't insert padding

**Solution: Reorder Fields**
```c
typedef struct {
    uint8_t  header[2];      // Offset: 0 (2 bytes)
    uint16_t seq_num;        // Offset: 2 (2 bytes)
    uint32_t timestamp;      // Offset: 4 ← NOW ALIGNED!
    ...
    uint8_t  padding;        // Fill to 30 bytes
} __attribute__((packed)) sensor_packet_t;
```

**Result:** ✅ All `uint32_t` fields now properly aligned!

---

## Implementation Details

### Firmware Side (C)

**Files Created/Modified:**
- `project/baseline.c` - Main baseline loop
- `project/baseline.h` - Data structures and function declarations
- `project/baseline_test_simple.c` - Simplified test version
- `project/Main.c` - Entry point
- `bsp/uart/bsp_uart.c` - UART blocking send function
- `bsp/icm20608/bsp_icm20608.c` - Sensor read wrapper

**Key Functions:**
```c
void baseline_loop(void);                      // Main loop
uint8_t calculate_checksum(sensor_packet_t*);  // Checksum calculation
void icm20608_read_data(sensor_data_t*);      // Sensor wrapper
void uart_send_blocking(uint8_t*, uint32_t);  // Blocking UART send
uint32_t get_system_tick(void);               // GPT1 timer read
```

**Baseline Loop Logic:**
```c
while(1) {
    uint32_t next_tick = get_system_tick() + PERIOD_TICKS;
    
    // Read sensor (blocking)
    uint32_t read_start = get_system_tick();
    icm20608_read_data(&sensor_data);
    packet.process_time_us = get_system_tick() - read_start;
    
    // Fill packet
    packet.timestamp = get_system_tick();
    packet.seq_num = seq++;
    // ... copy sensor data ...
    
    // Send via UART (blocking)
    uint32_t send_start = get_system_tick();
    uart_send_blocking((uint8_t*)&packet, sizeof(packet));
    packet.send_time_us = get_system_tick() - send_start;
    
    // Wait for next cycle (polling)
    while(get_system_tick() < next_tick);
}
```

### PC Side (Python)

**Tools Created:**

Raw byte hex dump
- Frame header detection
- Binary data visualization

---

## Clock Calibration Issue

**Discovery:**
- Configured GPT1 for 1 MHz (66 MHz IPG ÷ 66)
- Actual measured frequency: ~645 kHz
- Root cause: IPG clock is ~42.6 MHz (not 66 MHz as documented)

**Decision:**
- **Keep current configuration** (no fix needed)
- Reason: We need relative performance comparison, not absolute timing
- PC side uses measured 645 kHz for time conversion
- All 3 stages will use same clock source → fair comparison

---

## Test Results

### Functional Validation ✅
```
Total packets:       806
Valid packets:       806
Checksum errors:     0
Success rate:        100%
```

### Performance Baseline Established
```
CPU utilization:     106.7% (overloaded)
Average cycle:       31.6 ms (vs 50ms target)
Timing jitter:       ±4.39 ms (±14%)
UART efficiency:     6.8% (93% idle)
Sensor read time:    29.7 ms
UART send time:      4.0 ms
```

### Sensor Data Quality ✅
```
Static (board at rest):
  Accel: (-2, 7, 2054)  ← Z-axis shows gravity (~2048 LSB = 1g)
  Gyro:  (10, 11, 5)    ← Near zero (stable)

Motion (board moved):
  Accel: (526, 240, 1994)  ← Clear acceleration change
  Gyro:  (-74, 88, 44)     ← Angular velocity detected
```

---

## Debugging Tools & Workflow

### Build System
```bash
./re.bash  # Clean → Compile → Link → Generate .bin → Write to SD card
```

### Debugging Flow
1. Edit source code
2. Run `./re.bash` to build and flash
3. Swap SD card to board
4. Monitor serial output (CRT terminal)
5. Run Python receiver scripts
6. Analyze performance data

## Lessons Learned

### Technical Insights

1. **Memory Alignment Matters**
   - ARM requires natural alignment for 32-bit data
   - `__attribute__((packed))` disables automatic padding
   - Must manually arrange fields for alignment
3. **Software Timing is Imprecise**
   - Polling loops have ±14% jitter
   - Hardware timer interrupts needed for precision
   - Real-time systems require hardware-assisted timing
4. **Performance Measurement is Critical**
   - Embedding timestamps in data packets enables analysis
   - Quantitative metrics drive optimization decisions
   - Baseline data is essential for comparison

### Project Management

1. **Start Simple:** Baseline stage validates basic functionality
2. **Measure First:** Established clear performance metrics
3. **Expose Problems:** Baseline intentionally shows issues
4. **Document Everything:** Logs provide comparison benchmark

---

## Deliverables

### Code
- ✅ Baseline firmware implementation (C)
- ✅ PC-side analysis tools (Python)
- ✅ Complete build system (Makefile + scripts)

### Documentation
- ✅ Performance analysis report (baseline_analysis.md)
- ✅ Raw performance log (baseline.log)
- ✅ Work log (this file)

### Data
- ✅ 806 validated data packets
- ✅ Performance metrics captured
- ✅ Baseline benchmark established

---

**Status:** ✅ Baseline Stage Complete  
**Next:** Stage 2 - IRQ + Ring Buffer Implementation
