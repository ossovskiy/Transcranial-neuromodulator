/*
 * GPIO.c
 *
 *  Created on: Nov 28, 2022
 *      Author: Stanislav
 */
#include "main.h"

void GPIOa_powerUp(void){
	RCC->AHB2ENR1 |= (RCC_AHB2ENR1_GPIOAEN);	//Switching on GPIOA clock
	__NOP();
	__NOP();
}

void GPIOb_powerUp(void){
	RCC->AHB2ENR1 |= (RCC_AHB2ENR1_GPIOBEN);	//Switching on GPIOB clock
	__NOP();
	__NOP();
}

void GPIOc_powerUp(void){
	RCC->AHB2ENR1 |= (RCC_AHB2ENR1_GPIOCEN);	//Switching on GPIOC clock
	__NOP();
	__NOP();
}

//Making PC13 (button) into a digital input
void GPIOc_buttonInit(void){
	GPIOc_powerUp();
	GPIOC->MODER &= ~(3 << GPIO_MODER_MODE13_Pos); //PC13 is a digital input
	GPIOC->PUPDR |= (2 >> GPIO_PUPDR_PUPD13_Pos);	//Pull the button input down
}
