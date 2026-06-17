/*
 * helper.c
 *
 *  Created on: Oct 20, 2022
 *      Author: Stanislav
 */
#include <math.h>
#include "helper.h"
#include "main.h"
#include "OPAMP.h"
#include "RTC.h"
#include "ADC.h"
#include "CLOCKING.h"
#include "TIMER.h"
#include "BOOSTER.h"
#include "Ex_DAC.h"
#include "DMA.h"
#include "BUZZER.h"

//Temp sensor calibration values
#define TS_CAL1 (*(volatile unsigned short*) 0x0BFA0710)
#define TS_CAL2 (*(volatile unsigned short*) 0x0BFA0742)

//Voltage reference sensor calibration addresses
#define VREFINT_CAL (*(unsigned volatile short *) 0x0BFA07A5)

static int cordic_m = 0;	//A multiplier for our sinewave generator (signed, from -1 to 1)

//ADC temperature sensor temperature calculation variables
static float ts_cal1;
static float ts_cal2;
static float voltagePerDegree;
static float lowestTempPossible;

/*
 * We need to represent each digit as a separate character. Computers store most significant numbers "on the right",
 * while people-read numbers are stored the opposite way. So we need to read a number one digit at a time (four bits, id est from 0 to 15)
 * and store the most significant digits at the beginning of the resultant array and the least significant - at the end.
 */
void HELPER_numToASCII(unsigned int num, unsigned int num_length, char* result){
	for(int i = (num_length * 2) - 1; i >= 0; i--){	//processing the most significant digits first, going backwards
		int t = (num >> (i * 4)) & 0x0000000f;	//Mask everything, except the needed digit
		if(t <= 9){	//Store the result in "reverse order", since people read most significant digits first
			result[(num_length * 2 - 1) - i] = t + 0x30;	//ASCII number offset
		}else{
			result[(num_length * 2 - 1) - i] = t + 0x37;	//ASCII character offset
		}
	}
}

void HELPER_icacheInit(void){
	ICACHE->CR |= ICACHE_CR_CACHEINV;		//Invalidating old cache (if something left after reset)
	while (ICACHE->SR & ICACHE_SR_BUSYF){}		//Wait for the ICACHE system to become available
	ICACHE->CR |= ICACHE_CR_EN;
	//DCACHE1->CR |= DCACHE_CR_EN;
}

//It seems that the temperature sensor has about 140 degrees bias (!!)
float temp_calculator(unsigned short adcTemperature){
	float temperature = (adcTemperature / voltagePerDegree) + lowestTempPossible;
	return temperature;
}

//This function establishes values, necessary for later calculation of the core temperature
 void set_temperature_values(void){
	 //Scaling for the 2.5V reference voltage
	 ts_cal1 = TS_CAL1 * (3.0/2.5);
	 ts_cal2 = TS_CAL2 * (3.0/2.5);
	 voltagePerDegree = (ts_cal2 - ts_cal1) / 100;
	 lowestTempPossible = 30 - (unsigned short)(ts_cal1 / voltagePerDegree);	//We know that TS_CAL1 value is the voltage at 30 degrees and TS_CAL2 at 130c, we know how much voltage is one degree, so we find the lowest measured temperature possible at 0.0V
 }

float HELPER_Get_Temperature(void){
	//Before calculating temperature we should initialize all the equation parameters, so no need to do it every time, let's do it once
	static char paramInitFlag = 0;
	if(!paramInitFlag){
		set_temperature_values();
		paramInitFlag++;
	}
	unsigned int* adc_array = (unsigned int*)ADC_getADC1Data();
	int t = temp_calculator(adc_array[Coretemp]); 	//Pointing at the temperature part of the ADC data
	return t;
}

float HELPER_Get_Vbat(void){
	unsigned int* adc_array = (unsigned int*)ADC_getADC1Data();
	unsigned short adcVbat = adc_array[Battery];
	float vbat = (2.5 * adcVbat / 16384 ) * 4;	//The last term is added, because voltage monitor is internally divided by 4
	return vbat;
}


/*
 * The following two functions set and return the "cordic_m" - the multiplier (amplitude) for the generated sine function
 */
int HELPER_Get_Cordic_m(){
	return cordic_m;
}

//Second parameter for the CORDIC (multiplier, signed, from -1 to 1).
//Multiplication constant, since we use CORDIC in 16-bit mode, the lowest 16 bits are the angle value, and this multiplier is the highest 16 bits
void HELPER_Set_Cordic_m(int newCordic_m){
	if(newCordic_m < MINAMPLITUDE) newCordic_m = MINAMPLITUDE;
	cordic_m = newCordic_m << 17;
}

/*
 * This function checks if there is an error. It will slow down the system clock and turn on various LED indications in a such case.
 * Otherwise, it will just exit.
 *
 * Error indicators:
 * Negative booster failure - red led blinking, beeping
 * Positive booster failure - blue led blinking, beeping
 * Both boosters failure - red and blue les blinking, beeping
 * Overheat - red, blue and green leds blinking, beeping
 * Electrodes off - red and green leds blinking, beeping
 * Dead battery - fast red and blue leds blinking with some pause in between
 * Slider fault - red led on, beeping
 */
void HELPER_isThereError(unsigned char errorCode){
	unsigned short blinkingRate;
	if(errorCode == AllOkay) return;
	//Reset all the lights before indicating anything
	BSP_LED_Off(LED1);
	BSP_LED_Off(LED2);
	BSP_LED_Off(LED3);
	int i = 0;
	int j = 0;	//This number determines the rhythmic pattern
	if(errorCode == NegativeBoosterFailure){
		blinkingRate = 9000;
		while(1){
			BSP_LED_Toggle(LED3);
			if(i < 12){
				if(!(j & 1)){
					BUZZER_Buzz();
					RTC_wait(500);
					BUZZER_Shut();
					i++;
				}
				j++;
			}
			RTC_wait(blinkingRate);
		}
	}else if(errorCode == PositiveBoosterFailure){
		blinkingRate = 4096;
		while(1){
			BSP_LED_Toggle(LED2);
			if(i < 30){
				if(!(j & 0)){
					BUZZER_Buzz();
					RTC_wait(500);
					BUZZER_Shut();
					i++;
				}
				j++;
			}
			RTC_wait(blinkingRate);
		}
	}else if(errorCode == BothBoostersFailure){
		blinkingRate = 14096;
		while(1){
			BSP_LED_Toggle(LED3);
			BSP_LED_Toggle(LED2);
			if(i < 15){
				if(!(j & 0)){
					BUZZER_Buzz();
					RTC_wait(500);
					BUZZER_Shut();
					i++;
				}
				j++;
			}
			RTC_wait(blinkingRate);
		}


	}else if(errorCode == Overheat){
		blinkingRate = 4096;
		while(1){
			BSP_LED_Toggle(LED3);
			BSP_LED_Toggle(LED2);
			BSP_LED_Toggle(LED1);
			if(i < 12){
				if(!(j & 1)){
					BUZZER_Buzz();
					RTC_wait(500);
					BUZZER_Shut();
					i++;
				}
				j++;
			}
			RTC_wait(blinkingRate);
		}
	}else if(errorCode == ElectrodesOff){
		blinkingRate = 4096;
		while(1){
			BSP_LED_Toggle(LED1);
			BSP_LED_Toggle(LED3);
			//The buzzer will buzz for 6 times
			if(i < 12){
				if(!(j & 13)){ //The number 13 gives a good rhythm due to its bits combination
					BUZZER_Buzz();
					RTC_wait(500);
					BUZZER_Shut();
					i++;
				}
				j++;
			}
			RTC_wait(blinkingRate);
		}
	}
	else if(errorCode == DeadBattery){
		blinkingRate = 1100;
		int k = 0;
		while(k++ < 40){	//Couldn't use the "for" loop due to some weird compiler bug
			BSP_LED_Toggle(LED2);
			BSP_LED_Toggle(LED3);
			RTC_wait(blinkingRate);
		}
		BSP_LED_Off(LED2);
		BSP_LED_Off(LED3);
		RTC_wait(10000);
	}else if(errorCode == SliderFault){
		while(1){
			BSP_LED_On(LED3);
			//The buzzer will buzz for 6 times
			if(i < 32){
				BUZZER_Buzz();
				RTC_wait(700);
				BUZZER_Shut();
				RTC_wait(1200);
				i++;
			}
		}
	}
}

void HELPER_systemHalt(void){
	if(!TIMER_isSineRunning()){
		ADC_stop();
		BOOSTER_off();
		//Ex_DAC_PowerDown();
		CLOCKING_slowCpuDown();
	}else TIMER_enoughPlease();
}

//This function will check if the button is pressed.
//If the button is pressed briefly, then just start the generation
//If the it is pressed and held for over 0.75s, request automatic modulation upon start, blink the red LED three times,
//wait for the button to be released, wait for the button contacts to stop bouncing and exit
unsigned int HELPER_buttonIsPressed(void){
	if((GPIOC->IDR & (1 << 13))){
		RTC_wait(2000);
		if(!(GPIOC->IDR & (1 << 13))) return (unsigned int)1;
		RTC_wait(16000);
		if((GPIOC->IDR & (1 << 13))){
			TIMER_isModulationRequested() ? TIMER_clearModulationRequest() : TIMER_setModulationRequest();
			BUZZER_Buzz();
			for(unsigned short i = 6; i > 0; i--){
				if(!TIMER_isModulationRequested()){
					if(i < 6) BUZZER_Shut();	//Smaller value == longer beep
				}else{
					if(i < 3) BUZZER_Shut();
				}
				BSP_LED_Toggle(LED2);
				RTC_wait(3000);
			}
			//Account for the contact bounce. If we only want the modulation without starting everything right away, we need to wait while the button is released, otherwise this function
			//will read a false press, while called again (it will be called until either the button gets a short press, or until the amplitude slider is moved to the minimum)
			while((GPIOC->IDR & (1 << 13)));
			RTC_wait(1000);
			return (unsigned int)0;
		}else return (unsigned int)1;		//If the button is pressed longer, but not long enough, don't request modulation
	}
	return (unsigned int)0;
}

