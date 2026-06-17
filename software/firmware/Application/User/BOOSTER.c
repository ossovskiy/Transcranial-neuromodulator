/*
 * BOOSTER.c
 *
 *  Created on: Aug 30, 2025
 *      Author: stanislav
 */

#include "GPIO.h"
#include "ADC.h"
#include "main.h"
#include "RTC.h"

#define boostEnPin (1 << 10)	//Boost converter enable pin PB10

static unsigned char isBoosterOn = 0;

void BOOSTER_init(void){
	//Now, let's configure the "on switch for the boost voltage converter and turn the converter on
	//PB10 is connected to the EN inputs of the converter
	GPIOb_powerUp();
	//Before selecting PB10 as an output, we must clear the relevant selection field
	//as by default all of the pins are analog function and each selection field consists of 2 bits
	GPIOB->MODER &= ~(3 << GPIO_MODER_MODE10_Pos);
	GPIOB->MODER |= (1 << GPIO_MODER_MODE10_Pos);	//PB10 is a digital output
	GPIOB->OTYPER &= ~(boostEnPin);	//Push/pull output
	GPIOB->BSRR |= boostEnPin;	//Switching the booster on
	RTC_wait(16384);
	isBoosterOn = 1;
	RTC_wait(16384);		//Giving it time to start
}

void BOOSTER_off(void){
	GPIOB->BRR |= boostEnPin;
	isBoosterOn = 0;
}

unsigned char BOOSTER_isOn(void){
	return isBoosterOn;
}
