#include <unistd.h> 
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
#include "../bsp/ap3216c/bsp_ap3216c.h"
#include "../bsp/icm20608/bsp_icm20608.h"
#include "baseline.h"
 void imx6ul_hardfpu_enable(void)
{
	uint32_t cpacr;
	uint32_t fpexc;

	/* enable NEO FPU */
	cpacr = __get_CPACR();
	cpacr = (cpacr & ~(CPACR_ASEDIS_Msk | CPACR_D32DIS_Msk))
		   |  (3UL << CPACR_cp10_Pos) | (3UL << CPACR_cp11_Pos);
	__set_CPACR(cpacr);

	fpexc = __get_FPEXC();
	fpexc |= 0x40000000UL;	
	__set_FPEXC(fpexc);
}

int main()
{
    imx6ul_hardfpu_enable();
    int_Init();
    imx6u_clkinit();
    delay_Init();
    clk_Enable();
    uart_Init();

    if(icm20608_init() != 0)
    {
        printf("ICM20608 init failed!\r\n");
        while(1);  // stop
    }
    printf("ICM20608 init OK!\r\n");
    delayms(1000);

    baseline_loop();

    return 0;
}
