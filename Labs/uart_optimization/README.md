# UART Performance Optimization on IMX6ULL

A bare-metal embedded systems project demonstrating three-stage UART transmission optimization for IMU data acquisition on IMX6ULL (ARM Cortex-A7).

## Project Overview

**System**: IMX6ULL (528MHz) + ICM20608 6-axis IMU  
**Goal**: Optimize UART transmission to reduce CPU usage and improve system responsiveness  
**Sampling Rate**: 20Hz (50ms period)

## Performance Achievement

| Stage | Architecture | UART Send Time | CPU Usage | Main Loop |
|-------|--------------|----------------|-----------|-----------|
| **Stage 1** | Polling (Baseline) | 4.0 ms | ~60% | Blocked |
| **Stage 2** | IRQ + Ring Buffer | 4.0 ms | 67.5% | Available |
| **Stage 3** | IRQ + Async UART | **0.06 ms** | **59.5%** | Available |

**Key Results**:

- ✅ **98.5% reduction** in UART send time (4ms → 0.06ms)
- ✅ **8% reduction** in CPU usage (67.5% → 59.5%)
- ✅ **40x faster** system response time
- ✅ **Zero errors** in 30-second stress test (818 packets)

## 🏗️ Three-Stage Architecture

### Stage 1: Polling Baseline
```c
while(1) {
    icm20608_read_data();      // 29.7ms
    uart_send_blocking();       // 4.0ms - CPU blocked
    busy_wait_until_50ms();     // 16.3ms - wasted
}
```
**Problem**: CPU blocked on UART, cannot handle other tasks

### Stage 2: IRQ + Ring Buffer
```c
// Timer interrupt (50ms)
void gpt1_irq_handler() {
    icm20608_read_data();       // 29.7ms
    ring_buffer_write();        // ~1μs
}

// Main loop
while(1) {
    if(ring_buffer_available())
        uart_send_blocking();   // 4.0ms - still blocking
    else
        handle_other_tasks();   // LED, etc.
}
```
**Improvement**: Main loop freed from sensor read, but still blocked on UART

### Stage 3: Async UART (Interrupt-Driven TX)
```c
// Timer interrupt (50ms)
void gpt1_irq_handler() {
    icm20608_read_data();       // 29.7ms
    ring_buffer_write();        // ~1μs
}

// Main loop
while(1) {
    if(ring_buffer_available() && !uart_async_is_busy()) {
        uart_async_send();      // 0.06ms - returns immediately!
    }
    handle_other_tasks();       // 20ms available for tasks
}

// UART TX interrupt (auto-triggered 30x)
void uart1_tx_irq_handler() {
    if(uart_tx_idx < uart_tx_len)
        UART1->UTXD = uart_tx_buffer[uart_tx_idx++];  // Send 1 byte
    else {
        UART1->UCR1 &= ~(1 << 13);  // Disable interrupt
        uart_tx_busy = false;
    }
}
```
**Breakthrough**: UART transmission offloaded to interrupt handler, CPU freed

## 📁 Project Structure

```
uart_optimized/
├── Docs/
│   ├── data/
│   │   ├── result_stage1_wrong.json  # polling baseline with bug
│   │   ├── result_stage1_right.json  # fixed
│   │   ├── result_stage2             # IRQ + ringbuffer
│   │   └── result_stage3			  # DMA
│   ├── generic_receiver.py           # reciver script (create by gpt)
│   └── work_log.md					  # work log
├── Stage1 Polling Baseline /         # Stage 1: Polling
├── Stage2 IRQ + Ring Buffer /        # Stage 2: IRQ + Ring Buffer
└── Stage3 Async DMA UART             # Stage 3: Async DMA UART
```

## License

MIT License - feel free to use for learning and reference

## Acknowledgments

This project was developed as a practical embedded systems optimization exercise, demonstrating real-world problem-solving in bare-metal programming.

---

**Author**: xbs5520 
**Date**: October 2025 
**Platform**: IMX6ULL (ARM Cortex-A7, 528MHz)
