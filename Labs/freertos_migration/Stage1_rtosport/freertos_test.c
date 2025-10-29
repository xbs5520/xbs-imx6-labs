#include "freertos_test.h"

/* Task handles */
TaskHandle_t led_task_handle;
TaskHandle_t sensor_task_handle;

// Task 1: LED Heartbeat Task
void led_task(void *param)
{
    printf("[LED Task] Started\r\n");
    
    while(1) 
    {
        led0_switch();
        vTaskDelay(pdMS_TO_TICKS(500));  // Blink every 500ms
    }
}

// Task 2: ICM20608 Sensor Reading Task
void sensor_task(void *param)
{
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    
    printf("[Sensor Task] Started\r\n");
    printf("[Sensor Task] Initializing ICM20608...\r\n");
    
    // Init ICM20608 sensor
    if (icm20608_init() != 0) 
    {
        printf("[Sensor Task] ERROR: ICM20608 init failed!\r\n");
        vTaskDelete(NULL);
        return;
    }
    
    printf("[Sensor Task] ICM20608 initialized OK\r\n");
    printf("[Sensor Task] Reading sensor data every 1 second...\r\n\r\n");
    
    while(1) 
    {
        // Read sensor data
        icm20608_read_data(&accel_x, &accel_y, &accel_z, &gyro_x, &gyro_y, &gyro_z);
        
        // Print
        printf("AX = %6d, AY = %6d, AZ = %6d\r\n", accel_x, accel_y, accel_z);
        printf("GX = %6d, GY = %6d, GZ = %6d\r\n", gyro_x, gyro_y, gyro_z);
        printf("=================================\r\n\r\n");
        
        // Delay 1000ms
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ===== Main loop entry ===== */
void freertos_test_loop(void)
{
    printf("\r\n");
    printf("   FreeRTOS + ICM20608 Sensor -- IMX6ULL \r\n");
    printf("FreeRTOS Ver:   %s\r\n", tskKERNEL_VERSION_NUMBER);
    printf("Creating tasks...\r\n");
    
    // Create Task 1: LED Heartbeat
    BaseType_t ret1 = xTaskCreate(
        led_task,               /* Task function */
        "LED",                  /* Task name */
        256,                    /* Stack size (words) */
        NULL,                   /* Parameter */
        1,                      /* Priority */
        &led_task_handle        /* Task handle */
    );
    
    if (ret1 == pdPASS) 
    {
        printf("[OK] LED task created (Priority: 1)\r\n");
    } 
    else 
    {
        printf("[ERROR] LED task creation failed!\r\n");
        while(1);
    }
    
    // Create Task 2: Sensor Reading
    BaseType_t ret2 = xTaskCreate(
        sensor_task,
        "Sensor",
        512,
        NULL,
        2,
        &sensor_task_handle
    );
    
    if (ret2 == pdPASS) 
    {
        printf("[OK] Sensor task created (Priority: 2)\r\n");
    } 
    else 
    {
        printf("[ERROR] Sensor task creation failed!\r\n");
        while(1);
    }
    
    printf("\r\nStarting FreeRTOS scheduler...\r\n");
    printf("==================================\r\n\r\n");
    
    // start FreeRTOS scheduler
    vTaskStartScheduler();
    
    // failed print
    printf("[FATAL ERROR] FreeRTOS scheduler failed to start!\r\n");

    while(1) {
        led0_switch();
        delayms(200);
    }
}
