# Stage 3: FreeRTOS Runtime Statistics with LCD Monitor

**Date**: 2025-10-25  
**Goal**: Implement FreeRTOS runtime statistics and display task status and CPU usage on LCD in real-time

---

## Overview

### Goals Achieved
✅ Configure FreeRTOS runtime statistics  
✅ Use GPT2 timer as 1MHz high-precision counter  
✅ LCD real-time display of task list and CPU usage  
✅ Upgrade to heap_4 memory manager  
✅ Solve LCD character rendering issues  
✅ 6 tasks running stably with excellent performance  

---

## Core Technical Implementation

### 1. Runtime Statistics Configuration

#### FreeRTOSConfig.h Settings
```c
#define configGENERATE_RUN_TIME_STATS            1
#define configUSE_TRACE_FACILITY                 1
#define configUSE_STATS_FORMATTING_FUNCTIONS     1

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() /* Empty, GPT2 already initialized */
#define portGET_RUN_TIME_COUNTER_VALUE()         GPT2->CNT
```

#### GPT2 Timer Configuration
- **Clock Source**: IPG_CLK (66MHz)
- **Prescaler**: 65 (66MHz / 66 = 1MHz)
- **Count Mode**: FreeRun
- **Precision**: 1μs
- **Purpose**: Counter only, no interrupts

#### Stats Task Starts GPT2
```c
void stats_task2(void *param)
{
    // Start GPT2 timer (for runtime statistics counter only)
    if((GPT2->CR & 0x01) == 0) {
        GPT2->CR |= (1 << 0);
        printf("[Stats Task] GPT2 timer started for runtime statistics\r\n");
    }
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        lcd_display_stats();
    }
}
```

---

### 2. Memory Management Upgrade

#### Problem: heap_1 Heap Exhausted
- **Issue**: Stats task creation failed, xTaskCreate() returns NULL
- **Cause**: 
  - heap_1.c doesn't support free(), only allocation
  - Stats task stack 512 words (2KB)
  - Existing tasks + new task exceeds 8KB heap size

#### Solution
```c
// Makefile modification
OBJS += freeRTOS/portable/MemMang/heap_4.o  # Replace heap_1.o

// FreeRTOSConfig.h
#define configTOTAL_HEAP_SIZE  (12 * 1024)  // 8KB → 12KB
```

#### heap_4 Features
- ✅ Supports malloc/free
- ✅ Memory fragment merging
- ✅ Best fit algorithm
- ✅ Thread safe

---

### 3. LCD Real-Time Monitor System

#### Display Content
1. **Task List** (vTaskList)
   - Task Name
   - State: R=Running, B=Blocked, X=Executing
   - Priority
   - Remaining Stack
   - Task Number

2. **CPU Usage** (vTaskGetRunTimeStats)
   - Task Name
   - Runtime
   - CPU Percentage

#### Display Parameters
- **Refresh Period**: 2 seconds
- **Font Size**: 24 (title), 20-24 (content)
- **Color Scheme**: Black background, white text (0x00000000 / 0x00FFFFFF)
- **Screen Resolution**: 800x480

---

## Key Problem Solutions

### Problem 1: LCD Data Not Displaying

#### Issue
- LCD displays title and separators
- vTaskList() data not displaying
- Debug shows buffer length > 0, has data

#### Root Cause
vTaskList() output format uses **tab characters (\t)** for alignment:
```
Stats\t\t\tX\t0\t424\t2\n
\t\t\t\tIDLE\t\t\tR\t0\t100\t3\n
```

Terminal can interpret \t correctly, but **lcd_show_string() cannot render tabs**.

#### Solution

**Step 1: Replace Special Characters**
```c
// Replace \r and \t with spaces
for(j = 0; g_stats_buffer[j] != '\0'; j++) {
    if(g_stats_buffer[j] == '\r' || g_stats_buffer[j] == '\t') {
        g_stats_buffer[j] = ' ';
    }
}
```

**Step 2: Compress Consecutive Spaces**
```c
// Convert consecutive spaces to single space
char *read_ptr = g_stats_buffer;
char *write_ptr = g_stats_buffer;
int last_was_space = 0;

while(*read_ptr != '\0') {
    if(*read_ptr == ' ') {
        if(!last_was_space) {
            *write_ptr++ = ' ';
            last_was_space = 1;
        }
    } else {
        *write_ptr++ = *read_ptr;
        last_was_space = 0;
    }
    read_ptr++;
}
*write_ptr = '\0';
```

**Step 3: Skip Leading Spaces**
```c
while((line_end = strchr(line_start, '\n')) != NULL) {
    // Skip leading spaces in each line
    char *content_start = line_start;
    while(*content_start == ' ' && content_start < line_end) {
        content_start++;
    }
    
    int len = line_end - content_start;
    if(len > 0) {
        strncpy(line_buffer, content_start, len);
        line_buffer[len] = '\0';
        lcd_show_string(30, y, 750, 50, 24, line_buffer);
        y += 30;
    }
    line_start = line_end + 1;
}
```

---

### Problem 2: Percent Sign Display Error

#### Issue
- CPU usage '%' character displays incorrectly on LCD
- May show as garbled text or blank

#### Solution
Replace '%' with 'pct' (percent abbreviation):

```c
while(*read_ptr != '\0') {
    if(*read_ptr == '%') {
        *write_ptr++ = 'p';
        *write_ptr++ = 'c';
        *write_ptr++ = 't';
        last_was_space = 0;
    } else {
        *write_ptr++ = *read_ptr;
        last_was_space = 0;
    }
    read_ptr++;
}
```

Display effect:
```
Sensor   56pct
IDLE     30pct
```

---

### Problem 3: CPU Usage Data Empty

#### Issue
- vTaskGetRunTimeStats() returns empty string
- Buffer length is 0

#### Cause
- Sensor/UART tasks commented out during debug
- GPT2 timer not started
- portGET_RUN_TIME_COUNTER_VALUE() reads GPT2->CNT
- Counter not running, cannot measure runtime

#### Solution
Check and start GPT2 when stats_task starts:

```c
// Start GPT2 timer (for runtime statistics counter only)
if((GPT2->CR & 0x01) == 0) {  // Check if already started
    GPT2->CR |= (1 << 0);      // Start GPT2
    printf("[Stats Task] GPT2 timer started\r\n");
}
```

---

## Final Performance Metrics

### LCD Display Effect
```
FreeRTOS Monitor

==Task List==
Stats    X  0  374  2
Sensor   B  3  XXX  5
UART     B  2  XXX  6
IDLE     R  0  100  3
Tmr Svc  B  1  220  4
LED      B  1  98   1

==CPU Usage==
Sensor   56pct
IDLE     30pct
Stats    5pct
LED      1pct
TMR      1pct
```

### Python Script Validation Results

```
【Test Info】
  Time:       2025-10-24T19:46:30
  Duration:   30.0 seconds
  Packets:    875
  Errors:     0
  Packet Rate: 29.16 pkt/s
  Throughput: 874.93 B/s

【Timing Accuracy】
  Sampling Interval: 49.998 ms (std dev 0.735 ms)
  Jitter:     ±1.044 ms
  Range:      [49.460, 51.042] ms
  CV:         1.47%

【Performance】
  Sensor Read: 29.70 ms
  UART Send:  0.06 ms
  CPU Usage:  59.5% (estimated)
```

### Performance Analysis

#### 1. Timing Accuracy (Excellent)
- **Target**: 50ms sampling period
- **Actual**: 49.998ms
- **Error**: 0.002ms (0.004%)
- **Jitter**: ±1.044ms (2.1%)
- **Rating**: Very high precision, minimal jitter

#### 2. CPU Usage Distribution
| Task | LCD Display | Description |
|------|-------------|-------------|
| Sensor | 56% | ICM20608 read takes 29.7ms |
| IDLE | 30% | System idle |
| Stats | 5% | LCD update (2s period) |
| LED | 1% | 500ms blink |
| TMR | 1% | Software timer service |
| UART | <1% | Async send, minimal CPU |

**Total CPU Usage**: 70% (high load operation)

#### 3. Data Transmission Quality
- **Packet Rate**: 29.16 pkt/s (about 34ms/packet)
- **Error Rate**: 0/875 = 0%
- **Stability**: 30 seconds, no packet loss

---

## System Architecture (6 Tasks)

| Task | Priority | Stack | Function | CPU Usage | State |
|------|----------|-------|----------|-----------|-------|
| Sensor | 3 | 512w | 50ms sample ICM20608 | 56% | B (blocked waiting semaphore) |
| UART | 2 | 256w | Async send data | <1% | B (blocked waiting queue) |
| LED | 1 | 128w | 500ms blink | 1% | B (vTaskDelay) |
| Stats | 0 | 512w | 2s update LCD | 5% | X (executing) |
| IDLE | 0 | 100w | Idle task | 30% | R (ready) |
| Timer Svc | 1 | 220w | Software timer service | 1% | B (waiting timer) |

### Inter-Task Communication
```
GPT2 Interrupt → [Binary Semaphore] → Sensor Task
Sensor Task → [Queue 16x30B] → UART Task
```

### Memory Usage
- **Heap Size**: 12KB (heap_4)
- **Task Stacks Total**: ~6KB (512+256+128+512+100+220 words = 7.3KB)
- **Queue/Semaphores**: ~500B
- **Remaining Available**: ~4KB

---

## Technical Points Summary

### 1. FreeRTOS Runtime Statistics
- **Configuration Macros**: configGENERATE_RUN_TIME_STATS and 2 others
- **Counter Requirements**: 10-100x faster than tick frequency (1MHz vs 100Hz)
- **API Usage**:
  - `vTaskList()` - Get task status list
  - `vTaskGetRunTimeStats()` - Get CPU usage

### 2. Memory Management
- **heap_1**: Allocation only, no free (suitable for static systems)
- **heap_4**: Supports malloc/free, memory merging (recommended)
- **Heap Size**: Task stacks + kernel objects + 20% margin

### 3. LCD String Processing
- **Tab Character Issue**: \t cannot render, needs replacement
- **Space Compression**: Consecutive spaces affect layout
- **Leading Spaces**: Need to skip for left alignment
- **Special Characters**: '%' etc may not be supported, need replacement

### 4. Dual Buffer Technique
```c
static char g_stats_buffer[512];   // Task list
static char g_stats_buffer2[512];  // CPU usage
```
Prevents vTaskList and vTaskGetRunTimeStats from overwriting each other's data.

---

## Learning Outcomes

### Skills Mastered
✅ FreeRTOS runtime statistics configuration and usage  
✅ heap_4 memory manager principles and applications  
✅ LCD display system character rendering limitations  
✅ String processing algorithms (compression, replacement, parsing)  
✅ Multi-buffer technique to avoid data races  
✅ Real-time system performance analysis and tuning  

### Demonstrable Results
- ✅ LCD real-time monitor interface (800x480, 2s refresh)
- ✅ 6 tasks running cooperatively (Sensor/UART/LED/Stats/IDLE/Timer)
- ✅ High precision timing (49.998ms, error 0.004%)
- ✅ Zero error data transmission (29+ pkt/s)
- ✅ Detailed CPU usage statistics

---

## Future Optimization Directions
**Task Notifications** - Use ulTaskNotifyTake to replace binary semaphore
**Add AP3216C** - Light/proximity sensor acquisition
**LCD Graphs** - Display CPU usage history curves

---

## Code Structure

```
Stage3_LCDmonitor/
├── freertos_uartsend.c      # Main task implementation with LCD display
└── freertos_uartsend.h      # Header file
```

---

## Summary

**FreeRTOS Stage 3 completed successfully!**

- ✅ Implemented complete runtime statistics functionality
- ✅ LCD real-time display clear and beautiful
- ✅ Solved multiple technical challenges
- ✅ All performance metrics excellent
- ✅ System stable and reliable

**This is a professional-grade FreeRTOS multitasking system implementation!**
