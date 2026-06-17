/*
 * CLOCKING.c
 *
 *  Created on: Aug 31, 2025
 *      Author: stanislav
 */
#include "main.h"

void wait(unsigned int howLong){
	while(howLong > 0) howLong--;
}

void CLOCKING_clockInit(void){
	RCC->AHB3ENR |= (RCC_AHB3ENR_PWREN);	//Switching on peripheral clock power (LSE doesn't turn on without it)
	RCC->AHB2ENR1 |= (RCC_AHB2ENR1_GPIOAEN);	//Switching on GPIOA clock in order to enable MCO output
	__NOP();	//Waiting a bit for the power to turn on
	__NOP();
	RCC->BDCR |= (RCC_BDCR_LSEDRV);	//Increasing LSE drive current
	wait(10);
	PWR->DBPR |= PWR_DBPR_DBP;	//Allowing changes to LSE registers (turning LSE is prohibited without it)
	RCC->BDCR |= (RCC_BDCR_LSEON | RCC_BDCR_LSESYSEN);	//Turning on LSE
	while(!((RCC->BDCR) & RCC_BDCR_LSERDY)){};	//Waiting for LSE to stabilize

	RCC->CR &= ~(RCC_CR_PLL1ON);	//Making sure PLL1 is off before we do anything

	//RCC->PLL1CFGR |= (RCC_PLL1CFGR_PLL1MBOOST_0);	//EPOD power booster frequency is divided by 2

	RCC->CR |= (RCC_CR_MSISON);		//Turning on MSIS
	while(!((RCC->CR) & RCC_CR_MSISRDY)){};	//Wait for MSIS to get ready
	RCC->CR |= (RCC_CR_MSIKON);		//Switching MSIK on
	while(!((RCC->CR) & RCC_CR_MSIKRDY)){};	//Wait for MSIK to get ready
	RCC->CR |= (RCC_CR_MSIPLLSEL + RCC_CR_MSIPLLFAST);	//Fast PLL corrector startup, selecting PLL for MSIS
	RCC->CR |= (RCC_CR_MSIPLLEN);	//Starting PLL for MSIS
	RCC->PLL1CFGR &= ~(RCC_PLL1CFGR_PLL1SRC);	//Clearing the PLL1 source clock field
	RCC->PLL1CFGR |= (RCC_PLL1CFGR_PLL1SRC_0);	//MSIS selected as PLL1 clock entry

	PWR->VOSR |= (PWR_VOSR_VOS);	//Adjusting core voltage to allow high frequency operation
	while(!((PWR->VOSR) & PWR_VOSR_VOSRDY)){};	//Waiting for the voltage adjustments to take place
	PWR->VOSR |= (PWR_VOSR_BOOSTEN);	//Allowing core voltage boost
	while(!((PWR->VOSR) & PWR_VOSR_BOOSTRDY)){};	//Waiting for the booster to turn on
	FLASH->ACR &= ~(FLASH_ACR_LATENCY);		//Preparing Flash latency field for adjustments
	FLASH->ACR |= (FLASH_ACR_LATENCY_4WS);	//Adjusting Flash latency for 160mhz (5 cycles)

	RAMCFG_SRAM1_NS->CR &= ~(RAMCFG_CR_WSC);		//Adjusting SRAM wait states for 160Mhz (1 cycle)
	RAMCFG_SRAM2_NS->CR &= ~(RAMCFG_CR_WSC);		//Adjusting SRAM wait states for 160Mhz (1 cycle)
	RAMCFG_SRAM3_NS->CR &= ~(RAMCFG_CR_WSC);		//Adjusting SRAM wait states for 160Mhz (1 cycle)
	RAMCFG_SRAM4_NS->CR &= ~(RAMCFG_CR_WSC);		//Adjusting SRAM wait states for 160Mhz (1 cycle)
	RAMCFG_BKPRAM_NS->CR &= ~(RAMCFG_CR_WSC);		//Adjusting SRAM wait states for 160Mhz (1 cycle)

	RCC->PLL1DIVR &= ~(RCC_PLL1DIVR_PLL1N);
	RCC->PLL1DIVR |= (39 << RCC_PLL1DIVR_PLL1N_Pos);	//Multiplying 4Mhz * 39+1
	RCC->PLL1DIVR &= ~(RCC_PLL1DIVR_PLL1R);		//PLLr is divided by 1
	RCC->PLL1CFGR |= (RCC_PLL1CFGR_PLL1REN);	//Switching on PLL1r

	//___!!!___ Maybe MSIK should be stabilized, rather than MSIS

	RCC->CR |= (RCC_CR_PLL1ON);		//Turning PLL1 on
	while(!((RCC->CR) & RCC_CR_PLL1RDY)){};		//Wait for the PLL1 to stabilize
	//GPIOA->MODER &= ~(3 << 16);	//Switching GPIOA_8 to MCO function
	//GPIOA->MODER |= (2 << 16);	//^
	//GPIOA->OSPEEDR |= (3 << 16);	//Making MCO output pin "very high speed"
	//RCC->CFGR1 |= (RCC_CFGR1_MCOSEL_2 + RCC_CFGR1_MCOSEL_0 + RCC_CFGR1_MCOPRE_2);	//PLL1 output on PIN, divided by 16
	RCC->CFGR1 |= (RCC_CFGR1_SW);	//Sourcing system clock from PLL1r
	//while(!((RCC->CFGR1) & RCC_CFGR1_SWS)){};

	//__!!!__ Add CS security?
}

void CLOCKING_slowCpuDown(void){
	RCC->CFGR1 &= ~(RCC_CFGR1_SW);	//Sourcing system from MSIS
	RCC->PLL1CFGR &= ~(RCC_PLL1CFGR_PLL1REN);	//Turn PLL1r off
	RCC->CR &= ~(RCC_CR_MSIPLLEN);	//Stopping PLL
}
