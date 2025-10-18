# Async UART Transmission Implementation

**Objective**: Implement Stage 3 - IRQ + Ring Buffer + Async UART TX (Interrupt-driven asynchronous transmission)

---

## Overview

Building upon Stage 2 (IRQ + Ring Buffer), this stage transforms UART transmission from blocking mode to asynchronous interrupt-driven mode, aiming to further reduce CPU usage and minimize main loop blocking time.

---

## Implementation Process

### 1. Initial Design and Code Creation

**Files Created**:
- `bsp/uart/bsp_uart_async.h` - Async UART API definitions
- `bsp/uart/bsp_uart_async.c` - Async UART implementation
- `project/irq_dma.h` - Stage 3 header file
- `project/irq_dma.c` - Stage 3 main loop

**Core Design Philosophy**:
```c
// Async send API
int uart_async_send(uint8_t *data, uint32_t len);  // Returns immediately, non-blocking
bool uart_async_is_busy(void);                      // Check if transmission is in progress

// Interrupt handler
void uart1_tx_irq_handler(void);  // Sends 1 byte per call
```

**Key Register Configuration**:
- `UCR1 bit 13 (TRDYEN)`: Enable TX Ready interrupt
- `USR1 bit 13 (TRDY)`: TX FIFO ready flag

---

### 2. Problems Encountered and Solutions

#### Problem: Interrupt Not Triggering (Core Issue)
**Symptom**: 

- Compilation successful, but runtime shows `interrupts=0`
- Ring Buffer overflow rapidly increasing
- Only 1 packet sent

**Debugging Journey**:

1. **First Attempt**: Verify register bit definitions
   - Confirmed UCR1 bit 13 = TRDYEN ✅
   - Confirmed USR1 bit 13 = TRDY ✅
   - Register bits are correct

2. **Second Attempt**: Add debug output
   ```c
   printf("[ASYNC] UCR1=0x%08X, USR1=0x%08X\r\n", UART1->UCR1, UART1->USR1);
   // Output: UCR1=0x00000001, USR1=0x00000240
   // Found: TRDY=0 (bit 13 is 0)
   ```

3. **Third Attempt**: Manually send first byte
   - Theory: Interrupt might be edge-triggered, needs manual start
   - Result: Still only 1 byte sent, subsequent interrupts not triggered

4. **Found Key Information in Manual**:
   ```
   "An interrupt will be issued as long as TRDYEN and TRDY are high"
   "The fill level in the TXFIFO at which an interrupt is generated 
    is controlled by TxTL bits"
   ```

5. **Root Cause Discovered**: **TX FIFO Trigger Level (TXTL) Not Configured**
   - `UFCR bits 10-15`: TXTL (TX Trigger Level)
   - Default value likely 32 (FIFO full)
   - Printf continuously occupies UART, causing FIFO to never trigger interrupt

**Final Solution**:
```c
// Configure TXTL = 2 (trigger interrupt when FIFO < 2 bytes)
uint32_t ufcr = UART1->UFCR;
ufcr &= ~(0x3F << 10);  // Clear TXTL bits
ufcr |= (2 << 10);       // Set TXTL = 2
UART1->UFCR = ufcr;
```

---

## Test Results

### Stage 3 - 50 ms (20 Hz)

```
[Test Information]
  Duration:       30.0 seconds
  Packets:        818
  Errors:         0
  Packet Rate:    27.26 pkt/s
  Throughput:     817.89 B/s

[Timing Accuracy]
  Sample Interval: 50.006 ms (std dev 0.001 ms)
  Jitter:         ±0.002 ms
  Coefficient of Variation: 0.00%

[Performance Metrics]
  Sensor Read:    29.70 ms
  UART Send:      0.06 ms (min: 0.00, max: 0.06)
  CPU Usage:      59.5%
```

**Key Improvements**:
- ✅ UART send time: 4 ms → 0.06 ms (**98.5% reduction**)
- ✅ CPU usage: 67.5% → 59.5% (**8% decrease**)
- ✅ System responsiveness: Main loop no longer blocked on UART send

---

### Stage 3 - 40 ms (25 Hz) Test

```
[Test Information]
  Duration:       30.0 seconds
  Packets:        999
  Errors:         0
  Packet Rate:    33.3 pkt/s
  Throughput:     998.91 B/s

[Timing Accuracy]
  Sample Interval: 40.006 ms (std dev 0.001 ms)
  Jitter:         ±0.006 ms
  Coefficient of Variation: 0.00%

[Performance Metrics]
  Sensor Read:    29.70 ms
  UART Send:      0.06 ms
  CPU Usage:      74.4%
```

**Configuration Changes**:
```c
#define PERIOD_MS           40
#define PERIOD_TICKS        25800   // 40ms * 645kHz

// LED blink adjustment
if ((current_count / 13) != (last_led_check / 13)) {  // 40ms × 13 ≈ 520ms
```

**Analysis**:
- ✅ Sampling rate increased 25% (20 Hz → 25 Hz)
- ⚠️ CPU usage increased 14.9% (59.5% → 74.4%)
- ⚠️ Timing jitter increased 3x (±0.002 ms → ±0.006 ms)
- ⚠️ Reduced margin: 40 ms - 29.7 ms = 10.3 ms (tight)

---

## Technical Key Points Summary

### 1. UART Async Transmission Principle

**Traditional Blocking Mode**:
```c
for (i = 0; i < len; i++) {
    while (!(UART1->USR2 & (1 << 3)));  // Wait for TX ready
    UART1->UTXD = data[i];
}
// CPU blocked for 4 ms
```

**Async Interrupt Mode**:
```c
// Main loop
memcpy(uart_tx_buffer, data, len);
UART1->UCR1 |= (1 << 13);  // Enable interrupt
return;  // Returns immediately (~1 μs)

// Interrupt handler (automatically called 30 times)
void uart1_tx_irq_handler(void) {
    if (uart_tx_idx < uart_tx_len) {
        UART1->UTXD = uart_tx_buffer[uart_tx_idx++];
    } else {
        UART1->UCR1 &= ~(1 << 13);  // Disable interrupt
        uart_tx_busy = false;
    }
}
```

**Key Advantages**:
- CPU only spends ~1 μs to initiate transmission
- Actual transmission handled automatically by hardware interrupt
- Main loop can perform other tasks

---

### 2. UART TXTL (TX Trigger Level) Configuration

**UFCR Register**:
```
bits 10-15: TXTL - TX FIFO trigger threshold
```

**Trigger Conditions**:
- TXTL = 2: When FIFO bytes < 2, TRDY=1
- TXTL = 32: When FIFO completely empty, TRDY=1

**Why Configuration is Needed**:
- If TXTL is too high (e.g., default 32)
- Printf continuously occupies UART
- FIFO never empties to trigger threshold
- Results in TRDY always being 0, interrupt not triggered

**Solution**:
- Set TXTL = 2 (lower threshold)
- Even with printf usage, FIFO can quickly drop below 2
- Interrupt can trigger normally

---

### 3. Understanding Interrupt Trigger Mechanism

**Manual Description**:
> "An interrupt will be issued as long as TRDYEN and TRDY are high"

**Actual Behavior**:
- **Level-triggered**: Continuously triggers when TRDYEN=1 AND TRDY=1
- **Not edge-triggered**: Does not require 0→1 transition
- **Prerequisites**: 
  1. Transmitter must be enabled (UCR2 bit 2 = 1)
  2. TXTL must be properly configured
  3. FIFO has sufficient space (bytes < TXTL)

---

### 4. Performance Calculation Verification

**50 ms Period**:
```
CPU Usage = (Sensor Read Time + UART Send Time) / Period
          = (29.70 + 0.06) / 50.00
          = 29.76 / 50.00
          = 59.5% ✅
```

**40 ms Period**:
```
CPU Usage = (29.70 + 0.06) / 40.00
          = 74.4% ✅
```

Measured values perfectly match theoretical calculations!

---

## Performance Comparison Table

| Metric | Stage 1 | Stage 2 | Stage 3 (50ms) | Stage 3 (40ms) |
|--------|---------|---------|----------------|----------------|
| **Architecture** | Polling | IRQ + Ring Buffer | IRQ + Async UART | IRQ + Async UART |
| **Sample Period** | 50 ms | 50 ms | 50 ms | 40 ms |
| **Sample Rate** | 20 Hz | 20 Hz | 20 Hz | 25 Hz |
| **Sensor Read** | ~30 ms | 29.70 ms | 29.70 ms | 29.70 ms |
| **UART Send** | 4 ms (blocking) | 4 ms (blocking) | 0.06 ms (async) | 0.06 ms (async) |
| **CPU Usage** | ~60% | 67.5% | **59.5%** ⭐ | 74.4% |
| **Timing Jitter** | - | ±0.002 ms | ±0.002 ms | ±0.006 ms |
| **Ring Buffer** | ❌ | ✅ | ✅ | ✅ |
| **Responsiveness** | Poor | Medium | **Excellent** ⭐ | Excellent |
| **Margin** | ~16 ms | ~16 ms | ~20 ms | ~10 ms |

---

## Key Milestones

1. ✅ Successfully implemented UART async interrupt transmission
2. ✅ Solved TXTL configuration problem (critical breakthrough)
3. ✅ UART send time reduced by 98.5% (4 ms → 0.06 ms)
4. ✅ CPU usage reduced by 8% (Stage 2's 67.5% → 59.5%)
5. ✅ Validated 40 ms (25 Hz) feasibility
6. ✅ Complete performance data comparison across all three stages

---

## Lessons Learned

### 1. Importance of Hardware Documentation
- Cannot rely on generic ARM documentation
- Must consult chip-specific reference manual
- Register names and bit definitions may differ

### 2. Interrupt Debugging Techniques
- Add debug counters (e.g., `interrupts=0` exposed the problem)
- Read status registers to confirm hardware state
- Check manual's NOTES and WARNINGS sections

### 3. Hidden FIFO Configuration Details
- Configuration items like TXTL are easily overlooked
- Default values may not suit specific applications
- Need to understand trigger conditions and timing

### 4. Performance Bottleneck Analysis
- Sensor read 29.7 ms is a hard limit
- 100 Hz is not feasible (requires < 10 ms)
- 40 ms is the practical upper limit

---

## Next Steps

1. **SPI DMA**: Convert sensor read to DMA mode
   - Goal: Reduce sensor read time
   - Feasibility: Requires SDMA script configuration (complex)

2. **Real-Time OS (RTOS)**: FreeRTOS / RT-Thread

- Better task scheduling
- Precise time management
- Multi-task concurrency

---

## Repository Status

**New Files**:
```
bsp/uart/bsp_uart_async.h
bsp/uart/bsp_uart_async.c
project/irq_dma.h
project/irq_dma.c
```

**Modified Files**:
```
bsp/uart/bsp_uart.c (TXTL related comments)
project/Main.c (calls irq_dma_loop)
```

**Documentation**:
```
Docs/day3_work_log.md
Docs/day3_work_log_en.md (this file)
Docs/stage3_analysis.md
```

---

## Conclusion

**Stage 3 (50 ms) is Currently the Optimal Solution**:
- ✅ Lowest CPU usage (59.5%)
- ✅ Sufficient system margin (~20 ms)
- ✅ Excellent timing accuracy (±0.002 ms)
- ✅ Async transmission significantly improves responsiveness

**40 ms (25 Hz) as High Sampling Rate Option**:
- ✅ 25% increase in sampling rate
- ⚠️ Higher CPU load (74.4%)
- ⚠️ Limited margin (10.3 ms)

**Technical Breakthrough**:
- ✅ Mastered UART async interrupt transmission
- ✅ Understood FIFO trigger threshold configuration
- ✅ Verified theoretical calculations match measured results

