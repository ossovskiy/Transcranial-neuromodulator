/*
 * RTC.c
 *
 *  Created on: Dec 18, 2022
 *      Author: Stanislav
 */
#include "main.h"

static unsigned int counter_start_time;	//Here we will keep the initial time stamp. Then we will subtract it from the result

void RTC_init(void){
	RCC->APB3ENR |= (RCC_APB3ENR_RTCAPBEN);		//Power the RTC up
	PWR->DBPR  |= PWR_DBPR_DBP;		//Disable backup domain write protection
	RCC->BDCR |= RCC_BDCR_RTCSEL_0; //Selecting LSE as RTC clock source
	RTC->WPR = 0xCA;	//^Unlock RTC registers
	RTC->WPR = 0x53;
	RTC->ICSR |= RTC_ICSR_INIT;	//Stop RTC and unlock registers
	while(!(RTC->ICSR & RTC_ICSR_INITF)){}	//Wait for entering the init mode
	RTC->ICSR |= (RTC_ICSR_BIN_1 +  (7 << RTC_ICSR_BCDU_Pos));	//Mixed mode. 32 bit SSR counter and calendar.Calendar increments every 32768 tacts of the counter
	RTC->PRER = 0x0;	//Setting PREDIV_A to 0, so that to have maximum resolution of the RTC (SSR will run the fastest way)
	RTC->ICSR &= ~(RTC_ICSR_INIT);	//Finish initialization and start the RTC
	RTC->WPR = 0x1;	//Write protect RTC registers
}

void RTC_armCounter(void){
	while(!(RTC->ICSR & RTC_ICSR_RSF)){};	//Ensure it is safe to read the subsecond register
	counter_start_time = RTC->SSR;	//Marking the beginning of the count-down.
}

//Make sure you arm the counter before checking! Otherwise the results will be incorrect
//This free-running RTC timer counts backwards
unsigned int RTC_checkElapsedTime(void){
	while(!(RTC->ICSR & RTC_ICSR_RSF)){};	//Ensure it is safe to read the subsecond register
	unsigned int temp = RTC->SSR;
	if(temp >= counter_start_time){	//If counter roll-over happened during counting, account for that.
		return (counter_start_time + (0xffffffff - temp));
	}else{
		return (counter_start_time - temp);
	}
}

//This function virtually sets an alarm RTC_clock_cycles ahead and waits for it
void RTC_wait(unsigned int RTC_clock_cycles){
	RTC->WPR = 0xCA;	//^Unlock RTC registers
	RTC->WPR = 0x53;
	RTC->CR &= ~(RTC_CR_ALRAE);	//Disable alarm A to allow changes
	RTC->ALRMAR |= (RTC_ALRMAR_MSK1 + RTC_ALRMAR_MSK2 + RTC_ALRMAR_MSK3 + RTC_ALRMAR_MSK4);	//Mask alarm trigger for any DD:HH:MM:SS and activate trigger on subsecond values (32 bit)
	RTC->ALRMASSR |= (32 << RTC_ALRMASSR_MASKSS_Pos);
	while(!(RTC->ICSR & RTC_ICSR_RSF)){};	//Ensure it is safe to read the subsecond register
	RTC->ALRABINR = (RTC->SSR - RTC_clock_cycles);	//Setting the alarm
	int bogus = RTC->DR;	//After SSR is read, calendar registers won't increment until this register is read also
	RTC->CR |= RTC_CR_ALRAE;	//Arm the alarm
	while(!(RTC->SR & RTC_SR_ALRAF)){}	//Wait for the alarm
	RTC->SCR |= RTC_SCR_CALRAF;	//Clearing the alarm flag
	RTC->CR &= ~(RTC_CR_ALRAE);	//Disable alarm A
	RTC->WPR = 0x1;	//Lock registers
}
