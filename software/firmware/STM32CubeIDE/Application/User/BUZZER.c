/*
 * BUZZER.c
 *
 *  Created on: Nov 13, 2025
 *      Author: stanislav
 */
#include "main.h"
#include "GPIO.h"
#include "RTC.h"

//Buzzer pin - PA2
void BUZZER_Init(void){
	GPIOa_powerUp();
	GPIOA->MODER &= ~(3 << GPIO_MODER_MODE2_Pos);
	GPIOA->MODER |= (1 << GPIO_MODER_MODE2_Pos);	//PB10 is a digital output
	GPIOA->OTYPER &= ~(1 << 2);	//Push/pull output
	GPIOA->BSRR |= (1 << 2);	//Switching the buzzer off
}

void BUZZER_Shut(void){
	GPIOA->BSRR |= (1 << 2);	//Switching the buzzer off
}

void BUZZER_Buzz(void){
	GPIOA->BRR |= (1 << 2);	//Switching the buzzer on (Buzzer is turned on by low voltage)
}

void BUZZER_StartBuzz(void){
	for(int i = 0; i < 3; i++){
		BUZZER_Buzz();
		RTC_wait(900);
		BUZZER_Shut();
		RTC_wait(6000);
	}
	RTC_wait(46000);
	BUZZER_Buzz();
	RTC_wait(6000);
	BUZZER_Shut();
}


