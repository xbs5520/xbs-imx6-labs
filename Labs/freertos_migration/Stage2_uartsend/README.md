# Stage 2: FreeRTOS with UART Send

**Goal**: Migrate bare metal IRQ+RingBuffer architecture to FreeRTOS, learn task scheduling, semaphores, and queues

---

## Overview

### Core Objectives
Migrate Day 3's bare metal IRQ+RingBuffer architecture to FreeRTOS environment, learn RTOS task scheduling, semaphore, and queue mechanisms.

---

## Architecture Evolution

### Bare Metal Architecture (Day 3)
```
GPT1 Interrupt → Read sensor directly → Write RingBuffer
Main Loop       → Read from RingBuffer → UART send
```

### FreeRTOS Architecture (Day 5 Stage 2)
```
GPT2 Interrupt → xSemaphoreGiveFromISR() → Send signal
           ↓
Sensor Task (Priority 3) → xSemaphoreTake() blocks and waits
                         → Read sensor
                         → xQueueSend() send data packet
           ↓
UART Task (Priority 2)   → xQueueReceive() blocks and waits
                         → uart_async_send() async send
           ↓
LED Task (Priority 1)    → vTaskDelay() periodic blink
```

**Key Features**:
- **3 tasks** + 1 semaphore + 1 queue
- **Dual timer architecture**: GPT1 (FreeRTOS Tick 1ms) + GPT2 (Application timer 50ms)
- **Async UART**: Send doesn't block CPU, interrupt automatically completes transmission

---

## Key Technical Points

### 1. FreeRTOS Port Basics

#### Port Layer Implementation
Only need to implement 2-3 functions (FreeRTOS provides other 8 core functions):
- `vConfigureTickInterrupt()`: Configure GPT1 as Tick timer
- `freertos_gpt1_irq_handler()`: Tick interrupt handler
- Optional: `vClearTickInterrupt()`: Clear interrupt flag (some hardware requires)

#### Dual Timer Architecture
```c
// GPT1: FreeRTOS Tick (1ms)
void freertos_gpt1_irq_handler(unsigned int giccIar, void *param) {
    GPT1->SR = 1 << 0;
    GPT1->OCR[0] = GPT1->CNT + 1000;  // Next 1ms
    if (xTaskIncrementTick() != pdFALSE) {
        portYIELD();  // Task switch needed
    }
}

// GPT2: Application timer (50ms)
void sensor_timer_irq_handler(unsigned int giccIar, void *param) {
    GPT2->SR = 1 << 0;
    GPT2->OCR[0] = GPT2->CNT + 32250;  // Next 50ms
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(timer_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // Immediate high priority task scheduling
}
```

#### Interrupt Priority Configuration
```c
// Must be >= configMAX_API_CALL_INTERRUPT_PRIORITY (20)
GIC_SetPriority(GPT1_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY);
GIC_SetPriority(GPT2_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY);

// Note: GIC_SetPriority() already does left shift internally, don't manually shift!
// Wrong: GIC_SetPriority(GPT1_IRQn, 20 << 3);  // Double shift → wrong priority
```

---

### 2. Inter-Task Communication

#### Binary Semaphore: ISR → Task Event Notification
```c
// Send signal in ISR
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xSemaphoreGiveFromISR(timer_semaphore, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // Immediate scheduling

// Wait for signal in task
xSemaphoreTake(timer_semaphore, portMAX_DELAY);  // Block and wait
```

**Advantages**:
- Replaces polling (`while (flag == 0)`), CPU enters low power when idle
- Automatic task scheduling, high priority tasks respond immediately

#### Queue: Task → Task Data Passing
```c
// Create queue (16 data packets)
uart_queue = xQueueCreate(16, sizeof(sensor_packet_t));

// Producer (Sensor Task)
xQueueSend(uart_queue, &packet, 0);  // Non-blocking

// Consumer (UART Task)
xQueueReceive(uart_queue, &packet, portMAX_DELAY);  // Block and wait
```

**Advantages**:
- Replaces RingBuffer, FreeRTOS handles synchronization internally
- Automatic blocking/waking, no need for manual `vTaskDelay()`

---

### 3. Async UART Optimization

#### Blocking (Initial Version)
```c
// UART Task
uart_send_blocking((uint8_t*)&packet, sizeof(packet));  // Blocks ~4ms
// CPU wastes time waiting
```

#### Async (Optimized)
```c
// UART Task
while (uart_async_is_busy()) {
    vTaskDelay(1);  // Yield CPU to other tasks!
}
uart_async_send((uint8_t*)&packet, sizeof(packet));  // Start send ~0.06ms
// ← UART interrupt completes send automatically, CPU can run other tasks
```

**Improvement Principle**:
- `uart_async_send()` only starts DMA/interrupt, doesn't wait for completion
- Calls `vTaskDelay(1)` to yield CPU while waiting
- UART interrupt sends byte by byte, CPU processes other tasks

---

## Performance Comparison

| Metric | Bare Metal IRQ+RingBuffer | FreeRTOS Blocking UART | FreeRTOS Async UART |
|--------|---------------------------|------------------------|---------------------|
| **Sampling Interval** | 50.00ms ± 0.05ms | 50.007ms ± 1.353ms | 50.005ms ± 1.101ms |
| **Jitter** | **±0.05ms** (0.1%) | ±1.353ms (2.7%) | ±1.101ms (2.2%) |
| **UART Send** | 4.05ms (blocking) | 4.04ms (blocking) | **0.06ms** (start) |
| **CPU Usage** | ~67% | ~67.5% | **59.5%** |
| **Task Count** | N/A (single thread) | 3 | 3 |
| **Code Complexity** | Medium (manual mgmt) | High (tasks+queues) | High (tasks+queues) |

### Key Findings

#### ⚠️ RTOS Introduces Scheduling Delay
- **Jitter increases 20x**: ±0.05ms → ±1.1ms
- **Reasons**:
  1. Task context switch overhead (save/restore registers)
  2. FreeRTOS internal scheduling algorithm processing time
  3. May be briefly preempted by other tasks at same priority
- **Impact**: Only 2.2% for 50ms period, **acceptable**

#### ✅ Async Optimization Effective
- **CPU usage reduced**: 67.5% → 59.5%
- **Send time reduced**: 4.04ms → 0.06ms (startup time)
- **Reason**: UART task yields CPU while waiting, interrupt completes send automatically

#### ✅ Real-Time Performance Acceptable
- ±1.1ms jitter sufficient for most sensor applications (≥10ms period)
- If need **≤0.1ms jitter**, still need bare metal IRQ solution

---

## Debugging Experience

### Problem 1: Interrupt Priority Configuration Error
**Issue**: `configASSERT( portICCRPR_RUNNING_PRIORITY_REGISTER >= ... )` fails

**Cause**:
```c
// Wrong: Double left shift
GIC_SetPriority(GPT1_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT);
// GIC_SetPriority() already shifts internally, manual shift causes:
// 20 << 3 << 3 = 1280 → overflow wraps to 0 (highest priority) → violates FreeRTOS rules
```

**Solution**:
```c
// Correct: Pass priority value directly
GIC_SetPriority(GPT1_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY);  // 20
```

---

### Problem 2: Timer Initialization Timing Error
**Issue**: `configASSERT` fails at system startup

**Cause**:
```c
void freertos_test2_loop(void) {
    sensor_timer_init();
    sensor_timer_start();  // ← GPT2 triggers interrupt immediately
    vTaskStartScheduler();  // ← Scheduler not ready yet!
}
```

**Solution**:
```c
void freertos_test2_loop(void) {
    sensor_timer_init();  // Only initialize, don't start
    vTaskStartScheduler();  // Start scheduler
}

void sensor_task2(void *param) {
    sensor_timer_start();  // Start in task (scheduler is ready)
}
```

---

### Problem 3: Timestamp Precision Insufficient
**Issue**: `process_time_us` always shows 0 or 1000

**Cause**:
```c
uint32_t read_start = xTaskGetTickCount();  // 1ms precision
icm20608_read_data(...);  // Actually takes 100μs
uint32_t read_end = xTaskGetTickCount();
// read_end - read_start = 0 (less than 1ms)
```

**Solution**:
```c
static inline uint32_t get_high_precision_tick(void) {
    return GPT2->CNT;  // 1MHz = 1μs precision
}

uint32_t read_start = get_high_precision_tick();
icm20608_read_data(...);
uint32_t read_end = get_high_precision_tick();
// Can measure microsecond operations
```

---

## Key Takeaways

### 1. RTOS ≠ Higher Performance
**Introduces scheduling overhead, real-time performance slightly degraded**:
- Jitter increases from ±0.05ms to ±1.1ms (**20x**)
- Task switching, context saving, scheduling algorithm all need time
- **Suitable for**: Applications without stringent real-time requirements (jitter <5% acceptable)

### 2. RTOS = Better Architecture
**Task isolation, priority management, code maintainability improved**:
- Code separated by function into different tasks (Sensor, UART, LED)
- Adding new features only requires creating new tasks, doesn't affect existing code
- FreeRTOS provides rich synchronization mechanisms (semaphores, queues, mutexes, etc.)

### 3. Trade-off Balance
**Sacrifice a little real-time performance for system scalability and development efficiency**:
- **Bare metal**: Suitable for simple, high real-time requirement scenarios
- **RTOS**: Suitable for complex, multi-function, priority management scenarios
- **This experiment**: 50ms period ±1.1ms jitter = 2.2% error, **fully acceptable**

---

## Code Structure

```
Stage2_uartsend/
├── freertos_uartsend.c      # Main implementation (semaphore+queue+async UART)
└── freertos_uartsend.h      # Header file
```

---

## Summary

**FreeRTOS Stage 2 successfully completed!**

- ✅ Migrated bare metal IRQ+RingBuffer to FreeRTOS
- ✅ Learned semaphores, queues, and task scheduling
- ✅ Implemented async UART optimization
- ✅ Performance metrics excellent (50ms ±1.1ms, CPU 59.5%)
- ✅ System stable and reliable

This is a professional multi-task system implementation suitable for complex embedded applications!
