#ifndef _BSP_RTC_H
#define _BSP_RTC_H

#include "../../imx6ul/imx6ul.h"
#include "stdio.h"

#define SECONDS_IN_A_DAY    (86400)
#define SECONDS_IN_A_HOUR   (3600)
#define SECONDS_IN_A_MINUTE (60)
#define DAYS_IN_A_YEAR      (365)
#define YEAR_RANGE_START    (1970)
#define YEAR_RANGE_END      (2099)

struct rtc_datetime
{
    unsigned short year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
};

void rtc_init();
void rtc_enable();
void rtc_disable();
unsigned char rtc_isleapyear(unsigned short year);
uint64_t rtc_coverdate_to_seconds(struct rtc_datetime *datetime);
uint64_t rtc_getseconds();
void rtc_setdatetime(struct rtc_datetime *datetime);
void rtc_getdatetime(struct rtc_datetime *datetime);

static uint64_t snvs_read_raw_counter(void);
uint64_t rtc_millis(void);
uint32_t systick_ms(void);
#endif // _BSP_RTC_H