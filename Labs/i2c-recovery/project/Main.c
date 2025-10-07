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
#include "../bsp/i2c/bsp_i2c.h"
#include "../bsp/ap3216c/bsp_ap3216c.h"

extern volatile int g_i2c_blocked;

int main()
{
    imx6u_clkinit();
    clk_Enable();
    uart_Init();
    delay_Init();
    rtc_init();
    // I2C init
    ap3216c_init();

    while(1)
    {
        uint32_t now = systick_ms();
        fault_auto_process(now);
        rec_evt_pump();
        delayms(50);
    }
    
    return 0;
}