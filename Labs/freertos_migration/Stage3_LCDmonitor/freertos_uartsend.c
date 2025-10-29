#include "freertos_uartsend.h"
#include "../bsp/uart/bsp_uart_async.h"  // Async UART

SemaphoreHandle_t timer_semaphore; 
QueueHandle_t uart_queue;
static uint32_t g_last_send_time = 0;  // Global variable: last async send start time

static inline uint32_t get_high_precision_tick(void)
{
    return GPT2->CNT;
}

void sensor_timer_irq_handler(unsigned int giccIar, void *param)
{
    /* Clear interrupt flag */
    GPT2->SR = 1 << 0;
    
    /* Update next compare value (FreeRun mode) */
    GPT2->OCR[0] = GPT2->CNT + 32250;  // 645kHz * 50ms = 32250 ticks
    
    // Give semaphore and check if immediate context switch needed
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(timer_semaphore, &xHigherPriorityTaskWoken);
    // Yield based on return value
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * Initialize sensor sampling timer (GPT2, 50ms)
 */
void sensor_timer_init(void)
{
    /* 1. Disable GPT2 */
    GPT2->CR = 0;
    
    /* 2. Set prescaler */
    GPT2->PR = 65;  // 66MHz / 66 = 1MHz
    
    /* 3. Set output compare register: 50ms */
    GPT2->OCR[0] = 32250;  // 32250
    
    /* 4. Clear all status flags */
    GPT2->SR = 0x3F;
    
    /* 5. Enable output compare interrupt */
    GPT2->IR = 1 << 0;
    
    /* 6. Configure control register */
    GPT2->CR = (1 << 9) | (1 << 6) | (1 << 1);  // FreeRun + IPG_CLK
    
    /* 7. Register interrupt handler */
    system_register_irqhandler(GPT2_IRQn, 
                              (system_irq_handler_t)sensor_timer_irq_handler, 
                              NULL);
    
    /* 8. Set interrupt priority (must be >= configMAX_API_CALL_INTERRUPT_PRIORITY) */
    // Note: GIC_SetPriority() already does left shift internally, pass value directly
    GIC_SetPriority(GPT2_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY);
    
    /* 9. Enable GIC interrupt */
    GIC_EnableIRQ(GPT2_IRQn);

    printf("[Sensor Timer] GPT2 initialized (not started yet)\r\n");
}


void sensor_timer_start(void)
{
    GPT2->CR |= (1 << 0);
    printf("[Sensor Timer] GPT2 started: 50ms period\r\n");
}

void freertos_test2_loop(void)
{
    // init async UART
    uart_async_init();
    printf("[FreeRTOS] Async UART initialized\r\n");
    
    // Create semaphore
    timer_semaphore = xSemaphoreCreateBinary();
    if (timer_semaphore == NULL) 
    {
        printf("[ERROR] Failed to create semaphore!\r\n");
        while(1);
    }
    
    // Create queue
    uart_queue = xQueueCreate(16, sizeof(sensor_packet_t));
    if (uart_queue == NULL) {
        printf("[ERROR] Failed to create queue!\r\n");
        while(1);
    }
    
    // Create tasks
    xTaskCreate(sensor_task2, "Sensor", 512, NULL, 3, NULL);  // Priority 3 (highest)
    xTaskCreate(uart_task2, "UART", 256, NULL, 2, NULL);      // Priority 2
    xTaskCreate(led_task2, "LED", 128, NULL, 1, NULL);        // Priority 1
    xTaskCreate(stats_task2, "Stats", 512, NULL, 0, NULL);    // Priority 0

    
    // init GPT2 timer (but don't start yet)
    sensor_timer_init();
    
    // 5. Start scheduler
    printf("[FreeRTOS] Starting scheduler...\r\n");
    vTaskStartScheduler();  // Never returns
}

void led_task2(void *param)
{
    printf("[LED Task] Started\r\n");
    
    while(1) {
        led0_switch();
        vTaskDelay(pdMS_TO_TICKS(500));  // 500ms blink
    }
}

void sensor_task2(void *param)
{
    sensor_packet_t packet;
    static uint16_t seq_num = 0;
    
    printf("[Sensor Task] Started, starting GPT2 timer...\r\n");
    
    // Start GPT2 after scheduler is running (safe)
    sensor_timer_start();
    
    printf("[Sensor Task] Waiting for timer signal...\r\n");
    
    while(1) 
    {
        // ===== Block waiting for semaphore =====
        xSemaphoreTake(timer_semaphore, portMAX_DELAY);
        
        // ===== Execute after receiving signal =====
        /* Fill packet header */
        packet.header[0] = 0xAA;
        packet.header[1] = 0x55;
        packet.seq_num = seq_num++;
        packet.timestamp = get_high_precision_tick();

        // Read sensor data && time
        uint32_t read_start = get_high_precision_tick();
        icm20608_read_data(&packet.accel_x, &packet.accel_y, &packet.accel_z,
                          &packet.gyro_x, &packet.gyro_y, &packet.gyro_z);
        uint32_t read_end = get_high_precision_tick();
        
        // Fill processing time and send time
        packet.process_time_us = read_end - read_start;  // Sensor read time
        packet.send_time_us = g_last_send_time;          // Last async send start time
        packet.padding = 0;
        
        // Calculate checksum
        packet.checksum = calculate_checksum(&packet);
        
        // Send to UART queue (non-blocking, returns immediately)
        xQueueSend(uart_queue, &packet, 0);
    }
}

void uart_task2(void *param)
{
    sensor_packet_t packet;
    
    printf("[UART Task] Started, waiting for data from queue...\r\n");
    
    while(1) 
    {
        // Receive data from queue
        // Block and wait until queue has data
        if (xQueueReceive(uart_queue, &packet, portMAX_DELAY) == pdPASS) 
        {
            // Wait for previous send to complete (if still in progress)
            // This yields CPU, won't block other tasks
            while (uart_async_is_busy()) {
                vTaskDelay(1);  // Check every 1ms
            }
            
            // Start async send (returns immediately)
            uint32_t send_start = get_high_precision_tick();
            int ret = uart_async_send((uint8_t*)&packet, sizeof(sensor_packet_t));
            uint32_t send_end = get_high_precision_tick();
            
            if (ret == 0) 
            {
                // Successfully started, record start time
                g_last_send_time = send_end - send_start;
            }
        }
    }
}

// Global buffers used by stats task (avoid using task stack space)
static char g_stats_buffer[512];
static char g_stats_buffer2[512];  // Second buffer to avoid overwrite

// display stats on LCD
static void lcd_display_stats(void)
{
    char line_buffer[80];
    uint16_t y = 10;  // Starting Y coordinate
    
    // Force clear screen (fill entire screen with black)
    uint32_t *fb = (uint32_t *)tftlcd_dev.framebuffer;
    int i = 0;
    for(; i < tftlcd_dev.width * tftlcd_dev.height; i++) {
        fb[i] = 0x00000000;  // RGB888: Pure black
    }
    
    // Set foreground and background colors
    tftlcd_dev.forecolor = 0x00FFFFFF;  // White
    tftlcd_dev.backcolor = 0x00000000;  // Black
    
    // Title (white, large font 24)
    lcd_show_string(30, y, 750, 35, 24, "FreeRTOS Monitor");
    y += 40;
    
    // Get task list
    vTaskList(g_stats_buffer);
    
    // Task List title
    lcd_show_string(30, y, 750, 30, 24, "==Task List==");
    y += 35;
    
    // Simplify: replace \r and \t with spaces for display
    int j;
    for(j = 0; g_stats_buffer[j] != '\0'; j++) {
        if(g_stats_buffer[j] == '\r' || g_stats_buffer[j] == '\t') {
            g_stats_buffer[j] = ' ';  // Replace tabs with spaces too
        }
    }
    
    // Compress extra spaces (convert consecutive spaces to single space)
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
    
    // Display all tasks line by line
    char *line_start = g_stats_buffer;
    char *line_end;
    int line_count = 0;
    
    while((line_end = strchr(line_start, '\n')) != NULL && line_count < 8) {
        // Skip leading spaces
        char *content_start = line_start;
        while(*content_start == ' ' && content_start < line_end) {
            content_start++;
        }
        
        int len = line_end - content_start;
        if(len > 0 && len < 70) {
            strncpy(line_buffer, content_start, len);
            line_buffer[len] = '\0';
            lcd_show_string(30, y, 750, 50, 24, line_buffer);  // Font 24
            y += 30;
            line_count++;
        }
        line_start = line_end + 1;
    }
    
    y += 20;
    
    // Runtime Stats title
    lcd_show_string(30, y, 750, 30, 24, "==CPU Usage==");
    y += 35;
    
    // Get runtime statistics (use second buffer)
    vTaskGetRunTimeStats(g_stats_buffer2);
    
    // Replace \r and \t with spaces
    i = 0;
    for(; g_stats_buffer2[i] != '\0'; i++) {
        if(g_stats_buffer2[i] == '\r' || g_stats_buffer2[i] == '\t') {
            g_stats_buffer2[i] = ' ';
        }
    }
    
    // Compress extra spaces (reuse previous variables)
    read_ptr = g_stats_buffer2;
    write_ptr = g_stats_buffer2;
    last_was_space = 0;
    
    while(*read_ptr != '\0') {
        if(*read_ptr == ' ') {
            if(!last_was_space) {
                *write_ptr++ = ' ';
                last_was_space = 1;
            }
        } else if(*read_ptr == '%') {
            // Replace percent sign with friendlier representation
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
    *write_ptr = '\0';
    
    // Display CPU usage line by line
    line_start = g_stats_buffer2;
    line_count = 0;
    
    while((line_end = strchr(line_start, '\n')) != NULL && line_count < 8) {
        // Skip leading spaces
        char *content_start = line_start;
        while(*content_start == ' ' && content_start < line_end) {
            content_start++;
        }
        
        int len = line_end - content_start;
        if(len > 0 && len < 70) {
            strncpy(line_buffer, content_start, len);
            line_buffer[len] = '\0';
            lcd_show_string(30, y, 750, 50, 24, line_buffer);  // Font 24
            y += 30;
            line_count++;
        }
        line_start = line_end + 1;
    }
}

// Stats task (low priority, doesn't affect real-time performance)
void stats_task2()
{
    printf("[Stats Task] Started (LCD Display Mode)\r\n");
    
    // Start GPT2 timer (for runtime statistics counter only)
    // Note: No need to start interrupt, just need counter running
    if((GPT2->CR & 0x01) == 0) {  // Check if already started
        GPT2->CR |= (1 << 0);  // Start GPT2
        printf("[Stats Task] GPT2 timer started for runtime statistics\r\n");
    }
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(2000));  // Update LCD every 2 seconds
        
        // Display stats on LCD
        lcd_display_stats();
    }
}