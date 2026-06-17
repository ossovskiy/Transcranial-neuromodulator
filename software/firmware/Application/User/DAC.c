
#include "main.h"
#include "GPIO.h"
#include "RTC.h"
#include "TIMER.h"
#include "DMA.h"
#include "helper.h"

//This internal DAC isn't used for now

void DAC_calibrate(void){
	unsigned short calibVal = 0;
	DAC1->CR &= ~(DAC_CR_EN1);		//Disable the DAC channel 1
	DAC1->CR &= ~(DAC_CR_EN2);		//Disable the DAC channel 2
	DAC1->CR |= DAC_CR_CEN1;		//Starting the offset calibration for channel 1
	//Calibrating channel 1. Keep increasing calibrating value until the calibration flag sets
	while(!(DAC1->SR & DAC_SR_CAL_FLAG1)){
		DAC1->CCR = calibVal++;
		RTC_wait(2);	//Wait for about 60us to let the circuit settle
	}
	//Since we will calibrate channel 2 also and both channel share the same calibration register
	//we will use the "=" operator for making the procedure easier, to it is necessary to save the calibration value for channel 1
	unsigned short channel1CalibVal = calibVal - 1;		//Decrement it back
	DAC1->CR &= ~(DAC_CR_CEN1);		//Stopping the offset calibration for channel 1
	DAC1->CR |= DAC_CR_CEN2;		//Starting the offset calibration for channel 2
	calibVal = 0;
	while(!(DAC1->SR & DAC_SR_CAL_FLAG2)){
		DAC1->CCR = ((calibVal++) << 16);
		RTC_wait(2);	//Wait for about 60us to let the circuit settle
	}
	DAC1->CCR |= channel1CalibVal;	//Writing back the correct calibration value for the channel 1
	DAC1->CR &= ~(DAC_CR_CEN2);		//Stopping the offset calibration
	DAC1->CR |= DAC_CR_EN1;			//Enabling the DAC channel 1
	DAC1->CR |= DAC_CR_EN2;			//Enabling the DAC channel 2
}

void DAC_init(void){
	//DAC outputs (two DACs are combined into a one with higher resolution) - PA4 (lower bits), PA5 (higher bits)
	//The DAC will be clocked by HCLK (default) in order to be precisely triggered by the timer
	GPIOa_powerUp();
	PWR->SVMCR |= (PWR_SVMCR_ASV);	//Removing Analog supply (VDDA) isolation, necessary in order to power DAC1
	__NOP();
	__NOP();
	RCC->AHB3ENR |= RCC_AHB3ENR_DAC1EN;		//Connecting DAC to the bus
	//RCC->SRDAMR |= RCC_SRDAMR_DAC1AMEN;		//DAC autonomous mode enable
	//RCC->CCIPR3 |= RCC_CCIPR3_DAC1SEL;		//LSE is selected as the clock source for the DAC sample and hold mode.
	DAC1->CR &= ~(DAC_CR_EN1);		//Disable the DAC before applying settings
	DAC1->CR &= ~(DAC_CR_EN2);		//Disable the DAC channel 2
	//DAC will be connected to the external pin, buffer enabled (manual p. 1164)
	DAC1->MCR |= DAC_MCR_HFSEL_0;	//Must set this bit, since AHB frequency is >80Mhz
	DAC_calibrate();
	DAC1->CR &= ~(DAC_CR_EN1);		//Disable the DAC before applying settings (the calibrator enables
	DAC1->CR &= ~(DAC_CR_EN2);		//Disable the DAC channel 2
	//Switching off the buffer (for lower distortion?)
	DAC1->MCR |= (DAC_MCR_SINFORMAT2 /*+ (2 << DAC_MCR_MODE1_Pos) + (2 << DAC_MCR_MODE2_Pos)*//*+ DAC_MCR_SINFORMAT1*/);	//DAC now accepts signed numbers (ch 2 only, ch 1 is for the lower bits, so it must be unsigned)
	TIMER_dac_init();	//Setting up the timer for triggering DAC
	DAC1->DHR12LD = 0;	//The guide states that the first DMA request is generated after the first write, that should be done manually. However, works without it. Left here just in case.
	DAC1->CR |= (DAC_CR_TEN1 + (5 << DAC_CR_TSEL1_Pos) + DAC_CR_DMAEN1);		//DAC channel1 will be hardware triggered by the timer 6, DMA request enabled
	DMA_dacFuncInit();
	DAC1->CR |= DAC_CR_DMAEN1;	//Enable DMA for the channel 1
	DAC1->CR |= DAC_CR_EN1;		//Enabling DAC channel1
	DAC1->CR |= DAC_CR_EN2;		//Enabling DAC channel2
}

