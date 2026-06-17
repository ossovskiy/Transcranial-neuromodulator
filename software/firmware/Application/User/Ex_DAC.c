/*
 * Ex_DAC.c
 *
 *  Created on: Jul 24, 2025
 *      Author: stanislav
 */

#include "SPI.h"
#include "DMA.h"
#include "RTC.h"
#include "TIMER.h"
#include "CORDIC.h"
#include "ADC.h"
#include "main.h"
#include "BOOSTER.h"
#include "helper.h"

/*DAC80501 data width == 24 bits. 4 zero bits + 4 address bits + command
 * After each data piece the SS pin needs to be re-triggered.
 */
typedef struct {
		const unsigned int reset;	//A reserved soft-reset sequence
		const unsigned int pwrDown;
		const unsigned int zeroOutput;	//Not used. Default the DAC outputs voltage == 1/2 of the range, but we can set it to zero.
		const unsigned int refDivOutGain;	//Dividing internal reference by two, since the power is 3.3V, otherwise the DAC will not have enough headroom
											//for the reference to work properly and will shut down completely. Then we compensate for a lower reference by setting the internal buffer gain to x2
} commands;
commands exDacCommands = {0x5000A, 0x30101, 0x80000, 0x40101};

///The ADC must be properly initialized before calling this function. SPI also.
void Ex_DAC_Init(void){
	RTC_wait(10);	//Waiting for > 250uS to give the DAC time to startup
	SPI_commit((char*)&exDacCommands.reset, 0, sizeof(exDacCommands)/4);	//resetting the device
	RTC_wait(10);	//Waiting for > 250uS to give the DAC time to startup
	SPI_commit((char*)&exDacCommands.refDivOutGain, 0, sizeof(exDacCommands)/4);	//Dividing the reference,
	CORDIC_sineInit();		//Turn on the CORDIC sine function
	//Safety measure. Check, if the amplitude is initially not set to about zero - pause and wait until it is set to zero.
	//Alternatively, if the amplitude isn't zero, but we still want to proceed without changing it, wait until we explicitly press the button
	//Since ADC data is provided by DMA autonomously, we keep polling the DMA "transfer complete" status to read the
	//amplitude slider only after its value has been read.
	unsigned int* adc_data = (unsigned int*)ADC_getADC1Data();
	while(!(DMA_ADC1_port->CSR & DMA_CSR_TCF)){}
	//We will be lingering here until either the button is pressed, or, if the amplitude was high, until the slider sets it to the minimum
	while(adc_data[Amplitude] <= MINAMPLITUDE){
		if(main_getErrorCode() != AllOkay) return;
		if(HELPER_buttonIsPressed()) {
			break;	//Stop waiting if the button is pressed and proceed on with the current amplitude setting
		}
		__WFI();	//Sleep until DMA prepares the next ADC reading
	}
	while(adc_data[Amplitude] >= MINAMPLITUDE - 100){	//making the number a bit lower to accommodate for noise.
		if(main_getErrorCode() != AllOkay) return;
		BSP_LED_On(LED3);	//Indicate, if the initial amplitude is higher than the threshold
		if(HELPER_buttonIsPressed()) {
			break;	//Stop waiting if the button is pressed and proceed on with the current amplitude setting
		}
		__WFI();	//Sleep until DMA prepares the next ADC reading
	}
	BOOSTER_init();	//Turning on high voltage
	BSP_LED_Off(LED3);
	if(main_getErrorCode() > 0) return;	//Don't continue, if the boosters failed, or whatever else happened
	TIMER_dac_init();	//Setting up the timer for triggering DAC
}

void Ex_DAC_PowerDown(){
	SPI_commit((char*)&exDacCommands.pwrDown, 0, sizeof(exDacCommands)/4);
}

void Ex_DAC_Send(unsigned short data){
	static unsigned int sample;
	sample = (exDacCommands.zeroOutput + data);	//Adding sample data to the address of the output register on the DAC
	SPI_commit((char*)&sample, 0, 4);
}
