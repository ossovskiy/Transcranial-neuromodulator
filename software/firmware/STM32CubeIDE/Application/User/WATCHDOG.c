/*
 * WATCHDOG.c
 *
 *  Created on: Aug 22, 2025
 *      Author: stanislav
 */

#include "main.h"
#include "RTC.h"
#include "TIMER.h"
#include "ADC.h"
#include "helper.h"
#include "BOOSTER.h"
#include "CLOCKING.h"
#include "stdlib.h"
#include "stdbool.h"
#include "DMA.h"

//Battery weak and dead levels
static float batteryWeak = 3.59f;
static float batteryDead = 3.42f;

/*
 * 0xAAAA: reloads the RL[11:0] value into the IWDCNT down-counter (watchdog refresh),
and write-protects registers. This value must be written by software at regular intervals,
otherwise the watchdog generates a reset when the counter reaches 0.
- 0x5555: enables write-accesses to the registers.
- 0xCCCC: enables the watchdog (except if the hardware watchdog option is selected) and
write-protects registers.
- values different from 0x5555: write-protects registers.
Note that only IWDG_PR, IWDG_RLR, IWDG_EWCR and IWDG_WINR registers have a
write-protection mechanism.

We will track the elapsed time and reset the WD to keep system running. No need to do it often, or be too accurate.
Also, the WD will check if it is time to auto-off the whole thing (user off-timer), if the device is overheated and if the battery is weak.
 */
void WD_init(void){
	IWDG->KR = 0xCCCC;	//Start WD
	IWDG->KR = 0x5555; 	//Release write-protection from the WD module
	//The interrupt will trigger after (max_value - 0xbff) counts,
	//thus leaving x4 timer interrupt periods margin for the watchdog to do its job (reload etc.)
	IWDG->EWCR = (IWDG_EWCR_EWIE + (0xfff - 0x5ff));
	IWDG->KR = 1;	//Write protecting WD
	NVIC_SetPriority(IWDG_IRQn, 7); 	//The watchdog has low priority to make the system more sensitive to errors (not on time - reboot!)
	NVIC_EnableIRQ(IWDG_IRQn);
}

void IWDG_IRQHandler(void){
	IWDG->EWCR |= IWDG_EWCR_EWIC;	//Acknowledge interrupt
	IWDG->KR = 0xAAAA;	//Reload WD
	if(TIMER_isSineRunning()){
		if(TIMER_isTimer6Healthy()) TIMER_acknowledgeTimer6Healthy();
		else __NVIC_SystemReset();
		if(TIMER_isTimer7Healthy()) TIMER_acknowledgeTimer7Healthy();
		else __NVIC_SystemReset();
		if (DMA_isADCHealthy()) DMA_acknwledgeADCHealthy();
		else __NVIC_SystemReset();
	}
	unsigned int* ADC_results = (unsigned int*)ADC_getADC1Data();
	/* In case of overheat:
	 * 1. Indicate the overheat condition
	 * 2. Tell the timer to finish the business and turn off everything
	 */
	if(HELPER_Get_Temperature() > 70){
		//If the sine generator hasn't even started yet and the overheat condition happened, mark the error.
		//The Ex_DAC init routine, which will be running in such case will see the error, break and let the main routine display the error
		main_markError(Overheat);
		if(TIMER_isSineRunning()){
			TIMER_enoughPlease();
		}else{
			HELPER_systemHalt();
			return;
		}
	}
	/*Battery voltage check*/
	//Blinking red LED when the battery is low
	if((HELPER_Get_Vbat() <= batteryWeak) && (HELPER_Get_Vbat() > batteryDead) && (main_getErrorCode() == 0)){
		static short done = 0;
		if(!done){
		//Adding hysteresis to prevent erratic LED behavior
			batteryWeak = 3.65f;
			done++;
		}
		static int i = 0;
		if(!(++i & 3)) BSP_LED_Toggle(LED3); //Only blink the "low-batt" LED if no other error is present, just in order not to interfere with other error codes indications
	}
	//If the battery is all but dead, gracefully stop the session, if it is running,
	//otherwise mark a dead battery error.
	/*
	 *  !!!---!!! Make sure the watchdog interrupt priority is lower than the priority of the fade out managing timer (TIMER7). Lest the fade out timer
	 *  overwrites what we are writing in the TIMER_jumpToEndOfModulation()
	 */
	if(HELPER_Get_Vbat() <= batteryDead){
		//The following should work only once, otherwise the fade out will restart each time the watchdog interrupt triggers
		static short done = 0;
		if(TIMER_isSineRunning()){
			if(!done){
				batteryDead = 3.48f;	//Adding hysteresis to prevent erratic LED behavior
				if(!TIMER_isFadeoutRunning())
					TIMER_setAndCalculateFadeOutTime((unsigned int)(ADC_results[OffTimer] >> 2) + 32); 	//We will make the fade out part length proportional to 1/13 part of the session time. To prevent unpredicted behavior, we will use the "last seen" off timer fader value
				TIMER_isModulationRequested() ? TIMER_jumpToEndOfModulation() : TIMER_enoughPlease();
				done++;
			}
			BSP_LED_Toggle(LED3);
		}else{
			HELPER_systemHalt();
			main_markError(DeadBattery);
			return;
		}
	}
	if(HELPER_Get_Vbat() > 3.70){
		//If the only issue was the dead battery, reset the system once the battery is back to life.
		if(main_getErrorCode() == DeadBattery) NVIC_SystemReset();
		//if there is no any error
		if(main_getErrorCode() == 0) BSP_LED_Off(LED3);
	}
	if(!TIMER_isSineRunning()){
		return;	//No need to set the timer if the sine generation isn't going on
	}
	//Making the whole auto-off range about 1.1 hours (!!__!! 1.5h for now)
	//Auto off timer is ignored if the modulation program is requested, since it will take care of everything on its own.
	if(!TIMER_isModulationRequested()){
		unsigned int offMoment = ((unsigned int)(ADC_results[OffTimer] * 0.33f) + 32); //The moment when to start turning the device off. The OffTimer division value is added to limit the possible timer range to 1.5h. Also we add "32" to make sure the shortest possible time is half a minute.
		//if(((unsigned int)(ADC_results[OffTimer] >> 2) + 32) <= (unsigned int)(RTC_checkElapsedTime() >> 15)){	//Elapsed time in seconds. The OffTimer division value is added to limit the possible timer range. Also we add "32" to make sure the shortest possible time is half a minute.
		if(offMoment <= (unsigned int)(RTC_checkElapsedTime() >> 15)){	//Elapsed time in seconds.
			if(!TIMER_isFadeoutRunning())
				TIMER_setAndCalculateFadeOutTime(offMoment); 	//We will make the fade out part length proportional to 1/13 part of the session time. To prevent unpredicted behavior, we will use the "last seen" off timer fader value
			TIMER_enoughPlease();
		}
	}
}
