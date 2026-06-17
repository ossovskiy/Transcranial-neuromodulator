/*
 * RTC.h
 *
 *  Created on: Dec 18, 2022
 *      Author: Stanislav
 */

void RTC_init(void);
void RTC_armCounter(void);
unsigned int RTC_checkElapsedTime(void);
void RTC_wait(unsigned int RTC_clock_cycles);
