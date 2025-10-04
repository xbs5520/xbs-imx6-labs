#include "bsp_uart.h"

#if UART_BASELINE_METRICS
static volatile uint32_t uart_bytes_received = 0;
static volatile uint32_t uart_overrun_events = 0;
static volatile uint32_t uart_total_lost = 0;
static volatile uint32_t uart_max_burst = 0;
static volatile uint32_t uart_current_burst = 0;
static volatile uint8_t  uart_seq_expected = 0;
static volatile uint32_t uart_idle_counter = 0;
// Optional: calibration value (counts per ms when system is otherwise idle)
static volatile uint32_t uart_idle_calib_per_ms = 0;
static volatile uint8_t  uart_synced = 0; // 0 until pre-sync pattern achieved
static volatile uint8_t  uart_presync_prev = 0;
static volatile uint8_t  uart_presync_have_prev = 0;
static volatile uint8_t  uart_presync_consec = 0; // number of consecutive +1 steps
#endif

void uart_Init()
{
    // init io
    uart_io_Init();
    // init uart
    uart_disable(UART1);
    uart_softreset(UART1);

    //clear
    UART1->UCR1 = 0; // clear UCR1
    UART1->UCR1 &= ~(1 << 14); // disable check baud rate

    // set UCR2
    // bit 14   1  忽略 RTS
    // bit 8    0  关闭奇偶校验
    // bit 6    0  1位停止位
    // bit 5    1  8位数据位
    // bit 2    1  使能发送
    // bit 1    1  使能接收
    UART1->UCR2 |= (1 << 14) | (1 << 5) | (1 << 2) | (1 << 1);

    UART1->UCR3 |= 1 << 2; //bit2 must 1

    // set baud rate
    // UFCR 9:7 101 1分频
    // UBMR 3124  
    // UBIR 71
    // baud rate 115200
    UART1->UFCR = 5 << 7;
    UART1->UBIR = 71;
    UART1->UBMR = 3124;
    //可以通过这个函数直接设置
    //uart_setbaudrate(UART1, 115200, 80000000);
    uart_enable(UART1);
}

//IOinit
void uart_io_Init()
{
    IOMUXC_SetPinMux(IOMUXC_UART1_TX_DATA_UART1_TX, 0);
    IOMUXC_SetPinMux(IOMUXC_UART1_RX_DATA_UART1_RX, 0);
    IOMUXC_SetPinConfig(IOMUXC_UART1_TX_DATA_UART1_TX, 0x10B0);
    IOMUXC_SetPinConfig(IOMUXC_UART1_RX_DATA_UART1_RX, 0x10B0);
}


void uart_disable(UART_Type *uart)
{
    uart->UCR1 &= ~(1 << 0); // disable UART
}
void uart_enable(UART_Type *uart)
{
    uart->UCR1 |= (1 << 0); // enable UART
}
void uart_softreset(UART_Type *uart)
{
    uart->UCR2 &= ~(1 << 0); // reset
    while((uart->UCR2 & 0x1) == 0);// wait for reset to complete
}
// Forward declaration for unified accounting helper
#if UART_BASELINE_METRICS
static void uart_account_byte(uint8_t b);
#endif

void putc(unsigned char c)
{
    // 在等待 TX 完成期间主动服务 RX，避免打印造成的 FIFO 积压导致丢失
    while(((UART1->USR2 >> 3) & 0x01) == 0) {
#if UART_BASELINE_METRICS
        uint8_t rb;
        while (uart_try_read_byte(&rb)) {
            uart_account_byte(rb);
        }
#endif
    }
    UART1->UTXD = c & 0XFF; // send data
}

void puts(char *str)
{
    char *p = str;
    while(*p)
    {
        putc(*p);
        p++;
    }
}

int uart_try_read_byte(uint8_t *out) 
{
    if ((UART1->USR2 & 0x1) == 0) return 0;
    *out = (uint8_t)UART1->URXD;
    return 1;
}

uint8_t uart_read_blocking() 
{
    while((UART1->USR2 & 0x1) == 0) {
#if UART_BASELINE_METRICS
        uart_idle_counter++;
#endif
    }
    return (uint8_t)UART1->URXD;
}

void uart_read_seq_and_account(void) 
{
    uint8_t b = uart_read_blocking();
#if UART_BASELINE_METRICS
    uart_account_byte(b);
#endif
}

#if UART_BASELINE_METRICS
// Unified accounting (used by blocking read, drain, and opportunistic service during TX)
static void uart_account_byte(uint8_t b) {
    if (!uart_synced) {
        if (!uart_presync_have_prev) {
            uart_presync_prev = b;
            uart_presync_have_prev = 1;
            uart_presync_consec = 1;
            return; // not yet synced
        } else {
            uint8_t expected = (uint8_t)(uart_presync_prev + 1);
            if (b == expected) {
                uart_presync_consec++;
                uart_presync_prev = b;
                if (uart_presync_consec >= 4) {
                    uart_seq_expected = (uint8_t)(uart_presync_prev + 1);
                    uart_synced = 1;
                    uart_bytes_received = 0;
                    uart_overrun_events = 0;
                    uart_total_lost = 0;
                    uart_max_burst = 0;
                    uart_current_burst = 0;
                }
            } else {
                uart_presync_prev = b;
                uart_presync_consec = 1;
            }
            return; // still pre-sync
        }
    }

    // Synced path
    uint8_t delta = (uint8_t)(b - uart_seq_expected);
    if (delta == 0) {
        uart_current_burst++;
        if (uart_current_burst > uart_max_burst) uart_max_burst = uart_current_burst;
        uart_seq_expected++;
    } else {
        uart_overrun_events++;
        uart_total_lost += delta;
        uart_current_burst = 1;
        uart_seq_expected = (uint8_t)(b + 1);
    }
    uart_bytes_received++;
}

void uart_drain_nonblocking(void) {
    uint8_t b;
    while (uart_try_read_byte(&b)) {
        uart_account_byte(b);
    }
}
#endif

#if UART_BASELINE_METRICS
void uart_get_metrics(uint32_t *bytes, uint32_t *overruns,
                      uint32_t *lost, uint32_t *max_burst) {
    if (bytes) *bytes = uart_bytes_received;
    if (overruns) *overruns = uart_overrun_events;
    if (lost) *lost = uart_total_lost;
    if (max_burst) *max_burst = uart_max_burst;
}

uint32_t uart_get_idle_counter(void) { return uart_idle_counter; }
#endif

#if UART_BASELINE_METRICS
void uart_reset_metrics(void) {
    uart_bytes_received = 0;
    uart_overrun_events = 0;
    uart_total_lost = 0;
    uart_max_burst = 0;
    uart_current_burst = 0;
    uart_seq_expected = 0;
    uart_idle_counter = 0; // optional to leave intact, but we reset for fresh calibration if desired
    uart_idle_calib_per_ms = 0;
    uart_synced = 0;
    uart_presync_prev = 0;
    uart_presync_have_prev = 0;
    uart_presync_consec = 0;
}
#endif

unsigned char getc()
{
    while((UART1->USR2 & 0x1) == 0); // wait for last receive
    return UART1->URXD;
}

void raise(int sig_nr)
{
}