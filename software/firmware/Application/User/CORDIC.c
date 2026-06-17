/*
 * CORDIC.c
 *
 *  Created on: Jul 24, 2023
 *      Author: Stanislav
 */
#include "main.h"
#include "RTC.h"

/*
 * CORDIC calculates sample for the sine wave, which we will be outputting through the external DAC
 */
void CORDIC_sineInit(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_CORDICEN;
	CORDIC->CSR &= ~((7 << CORDIC_CSR_PRECISION_Pos) + (CORDIC_CSR_RESSIZE) + (CORDIC_CSR_ARGSIZE));
	CORDIC->CSR |= ((6 << CORDIC_CSR_PRECISION_Pos) + CORDIC_CSR_RESSIZE + CORDIC_CSR_ARGSIZE);	//Since our DAC is 16 bits, we ask CORDIC to produce 16-bit wide results from 16-bit arguments
	CORDIC->CSR |= (CORDIC_CSR_FUNC_0);	//Sine function
}
