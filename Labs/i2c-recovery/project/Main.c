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

int main()
{
    imx6u_clkinit();
    clk_Enable();
    uart_Init();
    delay_Init();
    // I2C init
    ap3216c_init();

    uint8_t value = 0;
    int iter = 0;
    int ok = 1, attempts = 0, pulses = 0;
    char fault[8] = {"No fault"};
    int td_ms = 0, tr_ms = 0;
    while(1)
    {
        value = ap3216c_readonebyte(AP3216C_ADDR, AP3216C_SYSTEMCONG);
        printf("key=%#x iter= %d fault=%s td_ms=%d tr_ms=%d ok=%d attempts=%d pulses=%d\r\n",
            value, iter, fault, td_ms, tr_ms, ok, attempts, pulses);
        iter++;
        delayms(500);
    }
    
    return 0;
}