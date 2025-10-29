#include "bsp_int.h"
#include "imx6ul.h"
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS Tick interrupt handler
void freertos_gpt1_irq_handler(unsigned int giccIar, void *param)
{
    /* Clear interrupt flag */
    GPT1->SR = 1 << 0;
    
    /* Update next compare value (required for FreeRun mode) */
    GPT1->OCR[0] = GPT1->CNT + 1000;  // 1000 ticks = 1ms
    
    /* Call FreeRTOS tick increment function */
    if (xTaskIncrementTick() != pdFALSE) 
    {
        /* Task switch needed */
        portYIELD();
    }
}

// Configure Tick timer
// Reference: gpt1_timer_dma_init() working configuration
void vConfigureTickInterrupt(void)
{
    /* 1. Disable GPT1 */
    GPT1->CR = 0;
    
    /* 2. Set prescaler: 66MHz / 66 = 1MHz */
    GPT1->PR = 65;  // Prescaler = 65 (divide by 66)
    
    /* 3. Set output compare register: 1MHz / 1000 = 1ms */
    GPT1->OCR[0] = 1000;  // 1000 ticks at 1MHz = 1ms
    
    /* 4. Clear all status flags */
    GPT1->SR = 0x3F;
    
    /* 5. Enable output compare interrupt */
    GPT1->IR = 1 << 0;  // Enable OCR1 interrupt
    
    /* 6. Configure control register
     * Bit 9 = 1: FreeRun mode
     * Bit 6 = 1: Clock source IPG_CLK (66MHz)
     * Bit 1 = 1: Enable mode (start counting from 0)
     * Bit 0 = 0: Don't start yet
     */
    GPT1->CR = (1 << 9) | (1 << 6) | (1 << 1);
    
    /* 7. Register interrupt handler */
    system_register_irqhandler(GPT1_IRQn, (system_irq_handler_t)freertos_gpt1_irq_handler, NULL);
    
    /* 8. Enable GIC interrupt */
    GIC_EnableIRQ(GPT1_IRQn);
    
    /* 9. Start GPT1 */
    GPT1->CR |= (1 << 0);  // Set EN bit
}


/**
 * Clear Tick interrupt flag (if FreeRTOS needs it)
 */
void vClearTickInterrupt(void)
{
    /* GPT1 interrupt flag already cleared in freertos_gpt1_irq_handler */
}

void vApplicationIdleHook(void)
{
    /* Can enter low power mode here */
}

void vApplicationTickHook(void)
{
    /* Called every Tick */
}

// Memory allocation failure Hook
void vApplicationMallocFailedHook(void)
{
    //Memory allocation failed
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    
    // Stack overflow
    taskDISABLE_INTERRUPTS();
    for (;;);
}
