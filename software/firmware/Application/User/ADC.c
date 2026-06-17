/*
 * ADC.c
 *
 *  Created on: Nov 25, 2022
 *      Author: Stanislav
 */

#include "main.h"
#include "UART.h"
#include "GPIO.h"
#include "ADC.h"
#include "DMA.h"
#include "BOOSTER.h"
#include "helper.h"

/*
 * ADC array indices are as follows:
 * 0 - Battery
 * 1 - Core temperature
 * 2 - Amplitude pot
 * 3 - Frequency pot
 * 4 - Auto-off timer
 * 5 - Boost converter voltage health check
 * 6 - Output check for the electrodes disconnect detection
 */
unsigned int adc_data[NO_OF_ADC1_CHANNELS];

//BEWARE that opamp may amplify the input signal so the ADC may saturate at low voltages
//Check the opamp amplification settings!
//ADC channel 8, which is used for the booster voltage monitoring is internally connect to OpAmp1 output
//so effectively the input to ADC channel 8 comes from PA0 - OpAmp1 non-inverting input.
//The same story for the electrodes disconnect checking. ADC channel 15 is internally connected to the onboard opamp,
//so the input signal comes from PA6.
/*
 * Pins, used for the ADC channels:
 *
 * Battery voltage - Vbat
 * Core temperature - internal
 * Output amplitude pot - PA5
 * Sine frequency control pot - PA7
 * Auto off-timer control pot - PA4
 * Booster voltage check - PA0
 * Electrode disconnect - PA6
 *
 *
 */
void ADC_init(void){
	PWR->SVMCR |= (PWR_SVMCR_ASV);	//Removing Analog supply (VDDA) isolation, necessary in order to power ADC1 LDO up
	__NOP();
	__NOP();
	GPIOa_powerUp();
	GPIOb_powerUp();
	//**Internal reference buffer init
	RCC->APB3ENR |= (RCC_APB3ENR_VREFEN);	//Power the reference buffer
	__NOP();
	__NOP();
	RCC->SRDAMR |= (RCC_SRDAMR_VREFAMEN);	//Enable reference buffer during sleep mode
	//!!!__!!! Unsolder SB3 bridge on the board
	//Setting up the reference buffer and reference output pin
	VREFBUF->CSR = 0;	//Reference out pin is now connected to the reference buffer and the buffer is disabled
	VREFBUF->CSR |= (3 << VREFBUF_CSR_VRS_Pos);	//2.5V reference selected
	VREFBUF->CSR |= VREFBUF_CSR_ENVR;	//Reference buffer enabled
	while(!(VREFBUF->CSR & VREFBUF_CSR_VRR)){}	//Waiting for the reference voltage to stabilize
	//**Powering up ADC1
	RCC->AHB2ENR1 |= (RCC_AHB2ENR1_ADC12EN);	//Connecting ADC1
	//!!!__!!! MAKE ADC1SMEN: ADC1 clocks enable during Sleep and Stop modes
	ADC12_COMMON->CCR = ((2 << ADC_CCR_PRESC_Pos) + ADC_CCR_VBATEN + ADC_CCR_VREFEN + ADC_CCR_VSENSEEN);	//ADC1 clock/4 (ADC1 clock must be less than 55MHZ), Vbat sensing enabled, Vref sensing enabled, temperature sensor
	ADC1->CR &= ~(ADC_CR_DEEPPWD);	//Exit ADC1 power down mode
	ADC1->CR |= ADC_CR_ADVREGEN;	//Enable ADC1 LDO
	while(!(ADC1->ISR & ADC_ISR_LDORDY)){}	//Wait for the LDO to get ready
	ADC_calibrate();	//Extended calibration
	//!!!__!!! perhaps voltage booster may increase noise !!!__!!!
	SYSCFG->CFGR1 |= SYSCFG_CFGR1_BOOSTEN; //Enable voltage boost on I/O analog switches in order to speed up conversions
	//Preselect 18 (Vbat), 19 (temp), 15 (output monitoring for electrode disconnect detection), 8 (booster voltage monitoring), 10, 12, 9 for pots, which we will use to control things
	ADC1->PCSEL |= (ADC_PCSEL_PCSEL_18 + ADC_PCSEL_PCSEL_19 + ADC_PCSEL_PCSEL_15 + ADC_PCSEL_PCSEL_8 + ADC_PCSEL_PCSEL_10 + ADC_PCSEL_PCSEL_12 + ADC_PCSEL_PCSEL_9);
	//Sampling the electrode off detection channel for 391 cycles and all the channels for 68 clk cycles
	ADC1->SMPR1 = 0;
	ADC1->SMPR2 = 0;
	ADC1->SMPR1 |= ((5 << ADC_SMPR1_SMP8_Pos) + (5 << ADC_SMPR1_SMP9_Pos));
	ADC1->SMPR2 |= ((5 << ADC_SMPR2_SMP12_Pos) + (5 << ADC_SMPR2_SMP15_Pos) + (5 << ADC_SMPR2_SMP18_Pos) + (5 << ADC_SMPR2_SMP19_Pos) + (5 << ADC_SMPR2_SMP10_Pos));
	ADC1->SQR1 |= (((NO_OF_ADC1_CHANNELS - 1) << ADC_SQR1_L_Pos /*total number of conversions - 1*/)  + \
			(18 << ADC_SQR1_SQ1_Pos) + (19 << ADC_SQR1_SQ2_Pos) + (10 << ADC_SQR1_SQ3_Pos) + (12 << ADC_SQR1_SQ4_Pos));	//We convert 7 channels at a time (SQR1 and 2 registers are responsible for that)
	ADC1->SQR2 |=  ((9 << ADC_SQR2_SQ5_Pos) + (8 << ADC_SQR2_SQ6_Pos) + (15 << ADC_SQR2_SQ7_Pos));
	ADC1->CFGR1 = 0;	//Making sure we are not starting from some residual values, such as default low resolution, etc.
	ADC1->CFGR2 = 0;
	ADC1->CFGR1 |= (ADC_CFGR1_AUTDLY + ADC_CFGR1_CONT + (3 << ADC_CFGR1_DMNGT_Pos));	//New conversion is possible only after all the old conversion have been read, continuous conversion, DMA circular mode selected, 14-bit resolution
	//**Oversampling = 1024, Injection restarts regular channel sampling from the first sample, oversampled data is 10 bits right shifted to get a 14 bit-wide result
	ADC1->CFGR2 |= ADC_CFGR2_LFTRIG;	//Preserve converted data from corruption, if we trigger conversions too seldom
	ADC1->CFGR2 |= (ADC_CFGR2_ROVSE + ADC_CFGR2_ROVSM + (1023 << ADC_CFGR2_OVSR_Pos) + (10 << ADC_CFGR2_OVSS_Pos));
	ADC1->ISR &= ~(ADC_ISR_ADRDY + ADC_ISR_OVR);	//Clear the ADC1 ready flag and OVR in order to permit DMA transfers and see later that the ADC1 gets ready really
	ADC1->CR |= ADC_CR_ADEN;	//Enabling ADC1
	while(!(ADC1->ISR & ADC_ISR_ADRDY)){}	//Waiting for the ADC1 to get ready
	DMA_adc1FuncInit();		//Starting DMA on ADC1
	ADC1->CR |= ADC_CR_ADSTART;
}

void ADC_stop(void){
	ADC1->CR &= ~ADC_CR_ADEN;
	NVIC_DisableIRQ(GPDMA1_Channel13_IRQn);
}

void ADC_calibrate(void){
	ADC1->CR |= ADC_CR_ADCALLIN;	//Set the linear calibration bit - both offset and linear calibration will be performed
	ADC1->CALFACT &= ~(ADC_CALFACT_CAPTURE_COEF + ADC_CALFACT_LATCH_COEF);	//Making sure no previous calibration values are locked
	//Extended calibration isn't supported on the "x" revision of the MCU
	/*ADC1->CR |= ADC_CR_ADEN;	//Enabling ADC1
	while(!(ADC1->ISR & ADC_ISR_ADRDY)){}	//Waiting for the ADC1 to get ready
	ADC1->CR |= (0b1001 << ADC_CR_CALINDEX0_Pos); //^Some magic numbers, necessary for initiating the extended calibration
	ADC1->CALFACT2 = 0x00020000;
	ADC1->CALFACT |= ADC_CALFACT_LATCH_COEF;	//^More magic, just following the necessary steps
	ADC1->CALFACT &= ~(ADC_CALFACT_LATCH_COEF);*/
	ADC1->CR |= ADC_CR_ADCAL;	//Starting extended calibration
	while(ADC1->CR & ADC_CR_ADCAL){}	//Waiting for the calibration to finish
	ADC1->CR &= ~(ADC_CR_ADCALLIN);		//No need for linear calibration anymore
}

unsigned int* ADC_getADC1Data(void){
	return (unsigned int *)adc_data;
}

