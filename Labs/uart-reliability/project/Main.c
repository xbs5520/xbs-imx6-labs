#ifdef __GNUC__
// removed <unistd.h> for bare-metal build; no POSIX layer needed
#endif
#include "../imx6ul/imx6ul.h"
#include "../bsp/led/bsp_led.h"
#include "../bsp/delay/bsp_delay.h"
#include "../bsp/clk/bsp_clk.h"
#include "../bsp/beep/bsp_beep.h"
#include "../bsp/key/bsp_key.h"
#include "../bsp/int/bsp_int.h"
#include "../bsp/exit/bsp_exit.h"
#include "../bsp/epittimer/bsp_epittimer.h"
#include "../bsp/key_filter/bsp_keyfilter.h"
#include "../bsp/uart/bsp_uart.h"
#include "../stdio/include/stdio.h"
#include "../bsp/lcd/bsp_lcd.h"
#include "../bsp/lcd/bsp_lcdapi.h"
#include "../bsp/rtc/bsp_rtc.h"

extern uint32_t uart_get_idle_counter(void);

static uint32_t calibrate_idle_ms(uint32_t ms) {
    volatile uint32_t start_cnt = uart_get_idle_counter();
    uint32_t start_ms = systick_ms();
    while (systick_ms() - start_ms < ms) {

    }
    uint32_t delta = uart_get_idle_counter() - start_cnt;
    return delta / ms; // per ms
}


int main()
{
    imx6u_clkinit();
    clk_Enable();
    uart_Init();
    rtc_init();

    // 通知主机：READY -> host 可以在看到这一行后再启动真正发送 (或脚本 --wait-ready)
    printf("[UART_BASELINE] READY\n");

    // 1) 校准空转：阻塞等待区间里 idle 计数器每循环 +1，用 200ms 求出每 ms 基线
    uint32_t idle_per_ms = calibrate_idle_ms(200); // baseline idle count per ms with no UART bytes
    if (idle_per_ms == 0) idle_per_ms = 1; // 防止除零

    printf("[UART_BASELINE] idle_per_ms=%lu (calibration window 200ms)\n", (unsigned long)idle_per_ms);
    printf("[UART_BASELINE] Starting blocking receive stream measurement...\n");
    printf("[UART_BASELINE] Columns: t(s) bytes lost overruns max_burst busy%% peak_busy%%\n");
    // Reset metrics: 进入预同步阶段 (等待4个连续递增字节才开始统计)
    uart_reset_metrics();
    // 预防在 calibration 期间串口已收到少量字节：主动 drain 一次（不会计入，因为还未 sync）
    uart_drain_nonblocking();
    
    // 2) 进入基线循环：持续阻塞式读取 + 统计，每秒打印一次
    uint32_t last_print_ms = systick_ms();
    uint32_t start_ms = last_print_ms;
    uint32_t last_idle_snapshot = uart_get_idle_counter();
    uint32_t peak_busy_pct = 0;

    while (1) {
    // 阻塞式读取 + 统计（内部会在等待期间累加 idle 计数 / 若未 sync 则进行预同步）
    uart_read_seq_and_account();

        uint32_t now_ms = systick_ms();
        if (now_ms - last_print_ms >= 1000) { // 每秒一次
            uint32_t idle_now = uart_get_idle_counter();
            uint32_t idle_delta = idle_now - last_idle_snapshot;
            uint32_t elapsed_ms = now_ms - last_print_ms; // ≈1000
            // 理论最大 idle 计数 = idle_per_ms * elapsed_ms
            uint64_t theoretical_idle = (uint64_t)idle_per_ms * (uint64_t)elapsed_ms;
            uint32_t busy_pct = 0;
            if (theoretical_idle > 0 && idle_delta <= theoretical_idle) {
                // busy = 1 - (实际 idle / 理论空闲)
                uint64_t active_ratio_times100 = (theoretical_idle - idle_delta) * 100ULL / theoretical_idle;
                busy_pct = (uint32_t)active_ratio_times100;
            } else if (theoretical_idle > 0 && idle_delta > theoretical_idle) {
                // 若因为测量偏差 idle_delta 超过理论值，视作 0% busy
                busy_pct = 0;
            }
            if (busy_pct > peak_busy_pct) peak_busy_pct = busy_pct;

            // 在打印前快速抓取可能积压的字节
            uart_drain_nonblocking();
            uint32_t bytes, overruns, lost, max_burst;
            uart_get_metrics(&bytes, &overruns, &lost, &max_burst);
            uint32_t t_sec = (now_ms - start_ms) / 1000;
            // 构建一整行字符串，逐字符发送，每个字符间隙快速 drain，降低打印阻塞导致的 RX FIFO 堆积
            char line[128];
            int len = 0;
            // 使用简单格式化（假设 sprintf 可用）
            len = sprintf(line, "[UART_BASELINE] %lus %lu %lu %lu %lu %u %u\n",
                          (unsigned long)t_sec,
                          (unsigned long)bytes,
                          (unsigned long)lost,
                          (unsigned long)overruns,
                          (unsigned long)max_burst,
                          (unsigned)busy_pct,
                          (unsigned)peak_busy_pct);
            if (len < 0) len = 0;
            int i; // C90 style loop variable declaration (toolchain not set to C99)
            for (i = 0; i < len; ++i) {
                putc((unsigned char)line[i]);
                // 每发送几个字符就试着清一次 RX（2 次尝试足够轻量）
                if ((i & 3) == 3) { // 每4个字符试一次
                    uart_drain_nonblocking();
                }
            }
            // 打印完成后再做一次彻底 drain
            uart_drain_nonblocking();

            last_print_ms = now_ms;
            last_idle_snapshot = idle_now;
        }
        // 可选：运行固定时长后退出，比如 60 秒
        // if ((systick_ms() - start_ms) > 60000) break;
    }

    return 0;
}