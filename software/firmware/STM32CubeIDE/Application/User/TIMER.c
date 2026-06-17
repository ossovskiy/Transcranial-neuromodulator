/*
 * TIMER.c
 *
 *  Created on: Apr 9, 2023
 *      Author: Stanislav
 */
#include "main.h"
#include "RTC.h"
#include "GPIO.h"
#include "helper.h"
#include "Ex_DAC.h"
#include "ADC.h"
#include "WATCHDOG.h"
#include "CLOCKING.h"
#include "BOOSTER.h"
#include "DMA.h"
#include "BUZZER.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "RANDGEN.h"

//A 1hz value for the initial timer setting
#define TICKS_PER_HZ     2440.0f

static bool timer6Healthy = true;
static bool timer7Healthy = true;

//A sample of the sine wave, which the DMA will send to the DAC, initiated at the half voltage  - "zero"
static short sineSample = 0x8000;

//Time value for the final fade out stage duration (only valid if modulation is off, for the modulation program has its own set value)
//Will be proportional to 1/10 of the session time and set once the watchdog timer marks that it is time to finish the session
static float fadeOutTime = 0.0f;

//A sample devoid of the DC component for calculating disconnected electrodes later in the ADC data processing interrupt routine (DMA.c file)
static short sineSampleAC = 0;

//A flag if the auto modulation program is requested (yes by default)
static short modulationRequest = 1;

//Generation running flag
static unsigned short sineRunning = 0;

//Basically precision, how much we increment the angle
static unsigned short angleInc = 1;

//A minimum timer value (no need to make it shorter, because the MCU cannot run it faster anyway)
static unsigned const MINIMUM_TIMER_PERIOD = 800;

//Auto off countdown parameter
static unsigned short timeToStop = 0;

static unsigned short angleVal = 0;

//Variable for samples averaging, if such is required (not used for now)
static unsigned int accumulatedSample = 0;
static unsigned short samplesCounter = 1; 	//For a proper averaging we must know how many samples have been taken. The value "one" is given to avoid zero division.

//This is an "interface" function, which will flag the sine generator (timer Interrupt routine down here)
//that it is time to stop generating a sine wave. This function is used by the watchdog timer, which also serves as the auto-off timer.
//So when the time is up, the watchdog will set this flag and the sine generator (timer), on seeing it, will stop.
void TIMER_enoughPlease(void){
	timeToStop = 1;
}

void TIMER_acknowledgeTimer6Healthy(void){
	timer6Healthy = false;
}

void TIMER_acknowledgeTimer7Healthy(void){
	timer7Healthy = false;
}

bool TIMER_isTimer6Healthy(void){
	return timer6Healthy;
}

bool TIMER_isTimer7Healthy(void){
	return timer7Healthy;
}

//An "interface" function, which permits other modules to figure out if the sine generation is online
unsigned int TIMER_isSineRunning(void){
	return sineRunning;
}

unsigned int TIMER_isFadeoutRunning(void){
	return (sineRunning & timeToStop);	//Returns "yes" only when the generator is running in the last, amplitude gradual fade out mode.
}

unsigned short TIMER_isModulationRequested(void){
	return modulationRequest;
}

/*
 * Controlling the "frequency" (the timer interval, at which samples are calculated).
 */
void TIMER_setSineFreq(unsigned int newFrequency){
	if(modulationRequest) TIM6->ARR = newFrequency; 	//No need to add slider non-linearity during automatic modulation
	else TIM6->ARR = MINIMUM_TIMER_PERIOD + (unsigned short)(newFrequency * 1.3) * (1 + (newFrequency >> 7));
}

short TIMER_getCurrentSample(void){
	return sineSampleAC;
}

void TIMER_setAndCalculateFadeOutTime(unsigned int time){
	fadeOutTime = (float)(time / 13);	//For a 65 min long session the fade out time will be five minutes
}

//This function will initialize periodicity at which this timer will generate (via CORDIC) a sine wave sample
//and through it to the external DAC.
//The automatic frequency modulation timer is also initialized here.
void TIMER_dac_init(void){
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
	TIM6->CR1 &= ~(TIM_CR1_CEN);	//Stopping the timer
	TIM6->ARR = (unsigned short)TICKS_PER_HZ;	//Timer will generate the update signal. Here, any value will do, because it will be changed after the next ADC update
	NVIC_SetPriority(TIM6_IRQn, 3);	//^Enabling timer 6 interrupt
	NVIC_EnableIRQ(TIM6_IRQn);
	TIM6->DIER |= TIM_DIER_UIE;	//Update interrupt enabled
	RTC_armCounter();	//Record the moment generator start to calculate elapsed time later
	//Indicate that everything is fine and the sine generation has started
	BSP_LED_On(LED2);
	BUZZER_StartBuzz();
	RTC_wait(48200); //Just wait for a bit after giving the start sound
	sineRunning = 1;	//Marking for the watchdog that the generation is in progress
	//Set up the timer that will take care of both automatic frequency modulation and final amplitude fade out
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM7EN;
	TIM7->CR1 &= ~(TIM_CR1_CEN);	//Stopping the timer
	TIM7->PSC |= 31;	//Slowing down the timer x32 times (value + 1)
	TIM7->ARR = 49999;	//The timer will update the frequency about 100 times per a second (value - 1)
	NVIC_SetPriority(TIM7_IRQn, 6);	//Enabling timer 7 interrupt
	NVIC_EnableIRQ(TIM7_IRQn);
	TIM7->DIER |= TIM_DIER_UIE;	//Interrupt enabled
	TIM7->CR1 |= TIM_CR1_CEN;	//Starting the timer
	unsigned int* ADC_results = (unsigned int*)ADC_getADC1Data();
	//Set initial amplitude and frequency (if auto modulation is requested, the modulator will set it automatically)
	if(!modulationRequest) TIMER_setSineFreq(ADC_results[Freq] >> 4);
	HELPER_Set_Cordic_m(ADC_results[Amplitude]);
	TIM6->CR1 |= TIM_CR1_CEN;	//Starting the timer
}

//Not used for now
unsigned short TIMER_getAveragedSample(void){
	static unsigned short averagedSample = 0;
	averagedSample = accumulatedSample / samplesCounter;
	accumulatedSample = 0;
	samplesCounter = 1;
	return averagedSample;
}

void TIMER_clearModulationRequest(void){
	modulationRequest = 0;
}

void TIMER_setModulationRequest(void){
	modulationRequest = 1;
}

//Making a sine sample
//Incrementing angle, calculating its sine value and throwing it into the Ex DAC
//Also checking if the auto-off timer has triggered and we need to turn the sine generation off
void TIM6_IRQHandler(void){
	TIM6->SR &= ~(TIM_SR_UIF);	//Acknowledging interrupt
	timer6Healthy = true;
	//When everything is done and it is time to stop the generator, do so at sine zero crossing.
	if(timeToStop && ((angleVal == 0) || (angleVal == 32768))){ 	//Zero crossing happens at the beginning and the half of the angle range
		//***
		//This part hits when either an error is detected, or the amplitude decrease has been done.
		//Just turning everything off
		//***
		if((HELPER_Get_Cordic_m() <= (MINAMPLITUDE << 17)) || (main_getErrorCode() > 0)){
			//Turning off the timer, which means no more sample generation
			TIM6->CR1 &= ~(TIM_CR1_CEN);
			TIM6->SR &= ~(TIM_SR_UIF);	//Acknowledging interrupt just in case
			//Making sure that the modulation timer is off also
			TIM7->CR1 &= ~(TIM_CR1_CEN);
			NVIC_DisableIRQ(TIM6_IRQn);
			NVIC_DisableIRQ(TIM7_IRQn);
			ADC_stop();
			BOOSTER_off();
			BSP_LED_Off(LED1);
			if(main_getErrorCode() == AllOkay) BSP_LED_On(LED2); //Leave just the blue LED on
			BSP_LED_Off(LED3);
			//Since the whole thing is finished and for a new session we need to restart the device, no need to keep the core at high-speed mode.
			//Switch off the PLL and clock the core from MSIS
			sineRunning = 0;	//Marking for the watchdog that the sine isn't being generated
			CLOCKING_slowCpuDown();
			return;
		}
	}
	sineSampleAC = CORDIC->RDATA;	//!!!___!!! TRY WITHOUT BIASING Converting signed CORDIC data into unsigned data, eatable by the ExDAC
	sineSample = sineSampleAC + 0x8000;	//The DAC understands data from 0 to Vref, not from -1 to 1, like CORDIC, so we shift it here.
	//accumulatedSample += (unsigned int)sineSample;	//Accumulating samples for subsequent averaging
	//samplesCounter ++;	//How many samples to average?
	Ex_DAC_Send((unsigned short)sineSample);
	CORDIC->WDATA = (angleVal + HELPER_Get_Cordic_m());	//Initiate calculating sine of a sample via CORDIC (will be ready by the next call of this routine)
	angleVal += angleInc;	//The CORDIC 16-bit range is the fraction value from -1 to 1. So the smaller the increment, the slower the frequency
}

/*
 * Frequency Modulation program starts here
 * There will be several phases:
 * 1. 1.5hz
 * 2. Logarithmic transition down to 0.7hz
 * 3. Logarithmic transition down to 0.5hz
 * 4. 0.5hz
 * 5. Logarithmic transition down to 0.22hz //not used
 * 6. 0.22hz	//not used
 * 7. Logarithmic fade out
 *
 * Jitter is added to make things look more natural
 *
 * The function will also perform the final amplitude fade out process in the manual frequency set mode
 */

//!! The 0.22hz phase can be commented out (not sure it is useful)

// --- Protocol Timing (seconds) ---
#define STAGE1_DUR      300.0f
#define STAGE2_DUR     	900.0f
#define STAGE3_DUR     	600.0f
#define STAGE4_DUR     1200.0f
//#define STAGE5_DUR      300.0f
#define STAGE5_DUR      600.0f	//no 0.22 phase
#define STAGE6_DUR      300.0f
#define STAGE7_DUR      300.0f

// --- Frequencies ---
#define FREQ_START  	1.7f
#define FREQ_MID1   	0.85f
#define FREQ_MID2  		0.6f
#define FREQ_END   		0.22f

// --- Jitter ---
#define JITTER_MAX       0.06f
#define JITTER_SPEED     0.003f

// --- Calculation period (how often this timer is routine runs per second) ---
#define CALC_PERIOD		 0.01f; // If this timer runs at 100Hz

//Timer tracker of the automatic modulation program
static float elapsed_time = 0.0f;

//This will be used as an emergency in order to jump right to the last phase (soft amplitude fade out) in case of a dead battery
void TIMER_jumpToEndOfModulation(void){
	elapsed_time = STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR /*+ STAGE5_DUR + STAGE6_DUR*/; //uncomment for no 0.22 phase
	//elapsed_time = STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR + STAGE5_DUR + STAGE6_DUR;
}

/*
 * This timer will control both the automatic frequency modulation program and (if the manual frequency set mode is active)
 * will control the last stage of the sine generation session - gradual amplitude fade out.
 */
void TIM7_IRQHandler(void){
	TIM7->SR &= ~(TIM_SR_UIF);	//Acknowledging interrupt
	timer7Healthy = true;
	// --- Global State ---
	static float current_jitter = 0.0f;
	static float target_jitter = 0.0f;
	//
	static float base_freq = FREQ_START;
	static float final_freq = 0;
	static float amplitudeForFadeOut = -1.0f;	//Amplitude can't be below zero, so this way we are marking that the initial amplitude from which we will fade to zero hasn't been grabbed yet.
	static float calculatedFadeOutAmplitude = 0.0f;
	unsigned int* ADC_results = (unsigned int*)ADC_getADC1Data();
	static int i = 0; 	//A counter for blinking the LED during fade out not to be too fast
	//
	//The routine
	if(modulationRequest){
		// --- 1. Frequency State Machine ---
		if (elapsed_time < STAGE1_DUR) {
			base_freq = FREQ_START;
		}
		else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR)) {
			float t = (elapsed_time - STAGE1_DUR) / STAGE2_DUR;
			base_freq = FREQ_START * powf((FREQ_MID1 / FREQ_START), t);
		}
		else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR + STAGE3_DUR)) {
			float t = (elapsed_time - STAGE1_DUR - STAGE2_DUR) / STAGE3_DUR;
			base_freq = FREQ_MID1 * powf((FREQ_MID2 / FREQ_MID1), t);
		}
		else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR)) {
			base_freq = FREQ_MID2;
		}
		//Comment from here
		/*
		else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR + STAGE5_DUR)) {
			float t = (elapsed_time - STAGE1_DUR - STAGE2_DUR - STAGE3_DUR - STAGE4_DUR) / STAGE5_DUR;
			base_freq = FREQ_MID2 * powf((FREQ_END / FREQ_MID2), t);
		}
		else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR + STAGE5_DUR + STAGE6_DUR)) {
			base_freq = FREQ_END;
		}
		*/
		//To here for no 0.22 phase !!_!!Dont forget to change the "jump to end of modulation" function also

		else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR + STAGE5_DUR /*+ STAGE6_DUR + STAGE7_DUR*/)) { //uncomment for No 0.22 phase
		//else if (elapsed_time < (STAGE1_DUR + STAGE2_DUR + STAGE3_DUR + STAGE4_DUR + STAGE5_DUR + STAGE6_DUR + STAGE7_DUR)) {
			if(!timeToStop) TIMER_enoughPlease();
			if(amplitudeForFadeOut < 0.0f) amplitudeForFadeOut = (float)(HELPER_Get_Cordic_m() >> 17);	//Cordic_m value is shifted left, so we shift it back for the processing, the setter will shift it back automatically
			float t = (elapsed_time - STAGE1_DUR - STAGE2_DUR - STAGE3_DUR - STAGE4_DUR /*- STAGE5_DUR - STAGE6_DUR*/) / STAGE5_DUR;//STAGE7_DUR; //uncomment for No 0.22 phase
			//float t = (elapsed_time - STAGE1_DUR - STAGE2_DUR - STAGE3_DUR - STAGE4_DUR - STAGE5_DUR - STAGE6_DUR) / STAGE7_DUR;
			calculatedFadeOutAmplitude = amplitudeForFadeOut * powf((((float)MINAMPLITUDE) / amplitudeForFadeOut), t);
			//Allowing to move the Amplitude slider down (reducing the amplitude), but not up.
			//Re-calculate initial frequency if the slider has moved
			unsigned int currentAmplitude = ADC_results[Amplitude];
			if((int)calculatedFadeOutAmplitude > currentAmplitude) calculatedFadeOutAmplitude = amplitudeForFadeOut = (float)currentAmplitude;
			HELPER_Set_Cordic_m((int)calculatedFadeOutAmplitude);
			if(!(++i & 15)) BSP_LED_Toggle(LED2);	//Blink the blue LED
		}
		// --- 2. Stochastic Jitter Logic ---
		if ((RANDGEN_getRandInt() % 1000) == 0) { // Adjusted probability for 100Hz timer
			target_jitter = ((RANDGEN_getRandFloat() * 2.0f * JITTER_MAX) - JITTER_MAX) * base_freq; 	//Making the jitter fraction constant at all frequencies
		}
		current_jitter += (target_jitter - current_jitter) * JITTER_SPEED;
		final_freq = base_freq + current_jitter;
		//
		if (final_freq < 0.01f) final_freq = 0.01f;
		unsigned short newTimerValue = (unsigned short)(TICKS_PER_HZ / final_freq);
		TIMER_setSineFreq(newTimerValue);
		elapsed_time += CALC_PERIOD;
	}else{
		//This section is just for the gradual amplitude fade out in the manual frequency set mode
		if(timeToStop){	//In the manual frequency set mode, the watchdog timer will control, when the session time is up.
			if(elapsed_time <= fadeOutTime){
				if(amplitudeForFadeOut < 0.0f) amplitudeForFadeOut = (float)(HELPER_Get_Cordic_m() >> 17);	//Cordic_m value is shifted left, so we shift it back for the processing, the setter will shift it back automatically
				float t = elapsed_time / fadeOutTime;
				calculatedFadeOutAmplitude = amplitudeForFadeOut * powf((((float)MINAMPLITUDE) / amplitudeForFadeOut), t);
				//Allowing to move the Amplitude slider down (reducing the amplitude), but not up.
				//Re-calculate initial frequency if the slider has moved
				unsigned int currentAmplitude = ADC_results[Amplitude];
				if((int)calculatedFadeOutAmplitude > currentAmplitude) calculatedFadeOutAmplitude = amplitudeForFadeOut = (float)currentAmplitude;
				HELPER_Set_Cordic_m((int)calculatedFadeOutAmplitude);
			}else HELPER_Set_Cordic_m(MINAMPLITUDE);	//At the end of the fadeout cycle, the final amplitude, due to float to int conversion, may end up being bigger than the minimum amplitude value and as the result, the TIMER 6 will never turn the device off, so we account for this here
			elapsed_time += CALC_PERIOD;
			if(!(++i & 15)) BSP_LED_Toggle(LED2);	//Blink the blue LED
		}
	}
}

