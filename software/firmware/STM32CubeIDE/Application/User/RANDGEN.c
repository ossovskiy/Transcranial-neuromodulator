/*
 * RANDGEN.c
 *
 *  Created on: Jan 10, 2026
 *      Author: stanislav
 */
#include "stm32u575xx.h"
#include "RTC.h"

void RANDGEN_init(void){
    // 1. Enable RNG Clock in AHB2ENR1 (AHB2 peripheral bus)
	RCC->AHB2ENR1 |= RCC_AHB2ENR1_RNGEN;
	RTC_wait(200);
	// 2. Reset to ensure a clean state
	RNG->CR &= ~(RNG_CR_RNGEN);
	RTC_wait(100);
	RNG->CR |= RNG_CR_CONDRST;
	RNG->CR &= ~RNG_CR_CONDRST;
	RTC_wait(200);
	// 3. Enable the RNG
	RNG->CR |= RNG_CR_RNGEN;
}

float RANDGEN_getRandFloat(void){
	//In case of an error reset RNG
	if (RNG->SR & (RNG_SR_CECS | RNG_SR_SECS)) {
		RNG->SR &= ~(RNG_SR_CECS | RNG_SR_SECS); // Clear flags
		RNG->CR |= RNG_CR_CONDRST;
		RNG->CR &= ~RNG_CR_CONDRST;
	}
	return (float)RNG->DR / (float)0xffffffff;
}


int RANDGEN_getRandInt(void){
	//In case of an error reset RNG
	if (RNG->SR & (RNG_SR_CECS | RNG_SR_SECS)) {
		RNG->SR &= ~(RNG_SR_CECS | RNG_SR_SECS); // Clear flags
		RNG->CR |= RNG_CR_CONDRST;
		RNG->CR &= ~RNG_CR_CONDRST;
	}
	return RNG->DR;
}
