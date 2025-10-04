#include "bsp_rtc.h"

void rtc_init()
{
    SNVS->HPCOMR |= (1 << 31);

    struct rtc_datetime rtc;
    rtc.year = 2025U;
    rtc.month = 9U;
    rtc.day = 10U;
    rtc.hour = 15U;
    rtc.minute = 0U;
    rtc.second = 0U;
    rtc_setdatetime(&rtc);

    rtc_enable();
}

void rtc_enable()
{
    SNVS->LPCR |= 1 << 0;
    while(!(SNVS->LPCR & 0x01));
}

void rtc_disable(void)
{
    SNVS->LPCR &= ~(1 << 0);
    while(SNVS->LPCR & 0x01);
}

uint64_t rtc_coverdate_to_seconds(struct rtc_datetime *datetime)
{	
	unsigned short i = 0;
	uint64_t seconds = 0;
	unsigned int days = 0;
	unsigned short monthdays[] = {0U, 0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U};
	
	for(i = 1970; i < datetime->year; i++)
	{
		days += DAYS_IN_A_YEAR; 		//not leap year
		if(rtc_isleapyear(i)) days += 1;// leap year
	}

	days += monthdays[datetime->month];
	if(rtc_isleapyear(i) && (datetime->month >= 3)) days += 1; // leap year
	days += datetime->day - 1;

	seconds = days * SECONDS_IN_A_DAY + 
				datetime->hour * SECONDS_IN_A_HOUR +
				datetime->minute * SECONDS_IN_A_MINUTE +
				datetime->second;

	return seconds;	
}

void rtc_setdatetime(struct rtc_datetime *datetime)
{
    uint64_t seconds = 0;
    unsigned int tmp = SNVS->LPCR;

    rtc_disable();

    seconds = rtc_coverdate_to_seconds(datetime);

    //SNVS->LPSRTCMR = (unsigned int)(seconds >> 32);
    //SNVS->LPSRTCLR = (unsigned int)(seconds & 0XFFFFFFFF);
	SNVS->LPSRTCMR = (unsigned int)(seconds >> 17);
	SNVS->LPSRTCLR = (unsigned int)(seconds << 15);

    if(tmp & 0x01)
        rtc_enable();
}

/* 读取秒数*/
uint64_t rtc_getseconds(void)
{
    uint64_t seconds = 0;

    //seconds = ((uint64_t)((uint64_t)(SNVS->LPSRTCMR) << 32)) | (SNVS->LPSRTCLR);
    seconds = (SNVS->LPSRTCMR << 17) | (SNVS->LPSRTCLR >> 15);
    
    return seconds;
}

unsigned char rtc_isleapyear(unsigned short year)
{	
	unsigned char value=0;
	
	if(year % 400 == 0)
		value = 1;
	else 
	{
		if((year % 4 == 0) && (year % 100 != 0))
			value = 1;
		else 
			value = 0;
	}
	return value;
}

void rtc_convertseconds_to_datetime(unsigned int seconds, struct rtc_datetime *datetime)
{
    unsigned int x;
    unsigned int  secondsRemaining, days;
    unsigned short daysInYear;

    //every mouth's days
    unsigned char daysPerMonth[] = {0U, 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    secondsRemaining = seconds;
    days = secondsRemaining / SECONDS_IN_A_DAY + 1;
    secondsRemaining = secondsRemaining % SECONDS_IN_A_DAY;

	//cal hours minutes seconds
    datetime->hour = secondsRemaining / SECONDS_IN_A_HOUR;
    secondsRemaining = secondsRemaining % SECONDS_IN_A_HOUR;
    datetime->minute = secondsRemaining / 60;
    datetime->second = secondsRemaining % SECONDS_IN_A_MINUTE;

    // cal years
    daysInYear = DAYS_IN_A_YEAR;
    datetime->year = YEAR_RANGE_START;
    while(days > daysInYear)
    {
        // cal years
        days -= daysInYear;
        datetime->year++;

        if (!rtc_isleapyear(datetime->year))
            daysInYear = DAYS_IN_A_YEAR;
        else	//leap
            daysInYear = DAYS_IN_A_YEAR + 1;
    }
    if(rtc_isleapyear(datetime->year))
        daysPerMonth[2] = 29;

    for(x = 1; x <= 12; x++)
    {
        if (days <= daysPerMonth[x])
        {
            datetime->month = x;
            break;
        }
        else
        {
            days -= daysPerMonth[x];
        }
    }

    datetime->day = days;

}

void rtc_getdatetime(struct rtc_datetime *datetime)
{
    uint64_t seconds = 0;
    seconds = rtc_getseconds();
    rtc_convertseconds_to_datetime(seconds, datetime);
}


static uint64_t snvs_read_raw_counter(void)
{
    while (1) {
        uint32_t high1 = SNVS->LPSRTCMR;
        uint32_t low   = SNVS->LPSRTCLR;
        uint32_t high2 = SNVS->LPSRTCMR;
        if (high1 == high2) {
            return ((uint64_t)high1 << 32) | low;
        }
    }
}

uint64_t rtc_millis(void)
{
    uint64_t raw = snvs_read_raw_counter();   // 47-bit 
    // 32768 Hz tick
    uint64_t seconds = raw / 32768ULL;
    uint64_t sub     = raw % 32768ULL;
    uint64_t ms = seconds * 1000ULL + (sub * 1000ULL) / 32768ULL;
    return ms;
}

uint32_t systick_ms(void)
{
    return (uint32_t)rtc_millis();  // 如果你只要 32-bit
}