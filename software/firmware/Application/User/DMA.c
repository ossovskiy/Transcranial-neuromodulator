/*
 * DMA.c
 *
 *  Created on: Nov 11, 2022
 *      Author: Stanislav
 */
#include "main.h"
#include "helper.h"
#include "ADC.h"
#include "DMA.h"
#include "UART.h"
#include "SPI.h"
#include "CORDIC.h"
#include "TIMER.h"
#include "BOOSTER.h"
#include "Ex_DAC.h"
#include "stdlib.h"
#include "stdbool.h"
#include "math.h"

#define PADS_RES 6000	//Electrodes intrinsic resistance
#define DIODE_DROP 252	//One diode drop in ADC terms

#define THRESH 1000 //The value for detecting frozen DAC

static float const VoltPerCount = (32.5 / 16384.0);	//Calculating voltage from ADC numbers (32v is the maximum absolute voltage opamp outputs, minus one diode drop)
static float const OpAmpR = 15700.0;	//The resistance at the head (output resistors before rubber pads resistance)

static bool adcHealthy = true;

int TxInProgress = 0;
Data_list* tx_fifo;	//UART Tx fifo buffer. Described in UART.h

void PowerOn(void){
	RCC->AHB1ENR |= (RCC_AHB1ENR_GPDMA1EN);		//Enable GPDMA
	__NOP();
	__NOP();
}

void DMA_uartFuncInit(void){
	tx_fifo = UART_getTxFifo();
	PowerOn();
	DMA_UART_port->CCR &= ~(DMA_CCR_EN);	//Disable DMA channel
	DMA_UART_port->CCR |= (DMA_CCR_PRIO_1 + DMA_CCR_TCIE);	//Set DMA channel priority to "the highest of the low", transfer complete interrupt enabled
	DMA_UART_port->CFCR |= DMA_CFCR_TCF;	//Clear "transfer complete" flag
	//Destination length = to UART FIFO size, source burst length = to DMA FIFO size, destination port to AHB 1
	DMA_UART_port->CTR1 |= ((7 << DMA_CTR1_DBL_1_Pos) + (31 << DMA_CTR1_SBL_1_Pos) + DMA_CTR1_DAP);
	DMA_UART_port->CBR2 |= (1 << DMA_CBR2_BRSAO_Pos);	//After each block transfer the source address points at the next byte
	DMA_UART_port->CTR2 |= (DMA_CTR2_DREQ + (25 << DMA_CTR2_REQSEL_Pos) + DMA_CTR2_BREQ + DMA_CTR2_TCEM_0);	//Channel request is driven by destination UART Tx, UART needs Block transfer mode, transfer complete interrupt is generated only after all the repetitions of the block are completed
	DMA_UART_port->CDAR = (volatile long unsigned int)(&(USART1->TDR));		//Set UART1 Tx as the destination for GPDMA transfer
	NVIC_EnableIRQ(GPDMA1_Channel12_IRQn);	//Activating interrupts
	NVIC_SetPriority(GPDMA1_Channel12_IRQn, 2);
}

void DMA_adc1FuncInit(void){
	PowerOn();
	//**Getting the destination address. After ADC transfers data from all the channels we will point at the first element again
	static unsigned int dest_address = 0;
	dest_address = (volatile unsigned int)ADC_getADC1Data();
	int dest_address_pointer = (volatile unsigned int)&dest_address;
	PowerOn();
	DMA_ADC1_port->CCR &= ~(DMA_CCR_EN);	//Disable DMA channel
	DMA_ADC1_port->CLBAR = 0;	//^Clearing the destination address fields
	DMA_ADC1_port->CLLR &= ~(0x0000FFFF);
	DMA_ADC1_port->CLBAR |= (dest_address_pointer & 0xFFFF0000);	//Higher 16 bits of the destination address
	DMA_ADC1_port->CLLR |= ((dest_address_pointer & 0x0000FFFF) + DMA_CLLR_UDA);	//Lower 16 bits of the destination address, destination address will be updated after block transfer is completed
	DMA_ADC1_port->CCR |= ((2 << DMA_CCR_PRIO_Pos) + DMA_CCR_TCIE);		//Middle priority to ADC1 transfers, transfer complete interrupt
	//Burst length equals the length of all channels together, source/destination data width = 4 bytes, destination port = AHB1
	DMA_ADC1_port->CTR1 |= ((NO_OF_ADC1_CHANNELS*4 << DMA_CTR1_DBL_1_Pos) + (NO_OF_ADC1_CHANNELS*4 << DMA_CTR1_SBL_1_Pos) + \
			DMA_CTR1_DDW_LOG2_1 + DMA_CTR1_DAP + DMA_CTR1_SDW_LOG2_1);
	DMA_ADC1_port->CTR2 |= (DMA_CTR2_BREQ + (0 << DMA_CTR2_REQSEL_Pos));	//Block transfer request, ADC1 request selected
	DMA_ADC1_port->CSAR = (volatile unsigned int)(&ADC1->DR);	//The source is ADC1 data register
	DMA_ADC1_port->CDAR = dest_address;
	DMA_ADC1_port->CTR2 |= DMA_CTR2_TCEM_0;		//The "Transfer Complete" interrupt is generated after all the repetitions of the block
	DMA_ADC1_port->CBR2 |= (4 << DMA_CBR2_BRDAO_Pos);	//After each block transfer point at the next destination array item
	DMA_ADC1_port->CBR1 |= (4 /*transfer 4 bytes per block*/ + ((NO_OF_ADC1_CHANNELS - 1 ) << DMA_CBR1_BRC_Pos));	//Since data from all ADC channels has the same source address and appear in a circle, we take it as a repeated block transfer and change only the destination address. Then repeat over again
	NVIC_EnableIRQ(GPDMA1_Channel13_IRQn);	//Activating interrupts
	NVIC_SetPriority(GPDMA1_Channel13_IRQn, 4);
	DMA_ADC1_port->CCR |= (DMA_CCR_EN);
}

//Transmission works best when the TX data width is set to 32 bits
//Perhaps I have to do the same for the RX?
void DMA_spiFuncInit(unsigned char sourceDataWidth, unsigned char destDataWidth){
	PowerOn();
/*DMA request channels for SPI1: 6 - Rx, 7 - Tx
 * ===Tx init*/
	DMA_SPI_Tx_port->CCR &= ~(DMA_CCR_EN);	//Disable the port
	/*"Highest of the low" priority, destination data width - 32 bit, source increment, source data width - 32 bit*/
	DMA_SPI_Tx_port->CCR |= (3 << DMA_CCR_PRIO_Pos);
	DMA_SPI_Tx_port->CTR1 |= (DMA_CTR1_SINC + (sourceDataWidth << DMA_CTR1_SDW_LOG2_Pos) + (destDataWidth << DMA_CTR1_DDW_LOG2_Pos));
	//SPI drives the DMA request, SPI Tx request selected
	DMA_SPI_Tx_port->CTR2 |= DMA_CTR2_DREQ + (7 << DMA_CTR2_REQSEL_Pos);
	DMA_SPI_Tx_port->CDAR = (int)(&SPI1->TXDR);		//Setting the SPI Tx port as the destination

/*===Rx init*/
	DMA_SPI_Rx_port->CCR &= ~(DMA_CCR_EN);	//Disable the port
	/*"Highest of the low" priority, transfer complete interrupt enabled
	 * destination data width - 8bit, source data width - 8bit, Destination port AHB 1*/
	DMA_SPI_Rx_port->CCR |= ((3 << DMA_CCR_PRIO_Pos) + DMA_CCR_TCIE);
	DMA_SPI_Rx_port->CTR1 |= DMA_CTR1_DAP;
	//SPI (Source) drives the DMA request, SPI Rx request selected
	DMA_SPI_Rx_port->CTR2 |= (6 << DMA_CTR2_REQSEL_Pos);
	DMA_SPI_Rx_port->CSAR = (int)(&SPI1->RXDR);		//Setting the SPI Rx port as the destination

	NVIC_EnableIRQ(GPDMA1_Channel8_IRQn);	//Activating interrupts
	NVIC_SetPriority(GPDMA1_Channel8_IRQn, 5);
}

void DMA_initiateSpiTransmission(char* data, int how_much){
	DMA_SPI_Tx_port->CSAR = (int)data;	//Pointing at the data array
	DMA_SPI_Tx_port->CBR1 = how_much;
	DMA_SPI_Tx_port->CCR |= DMA_CCR_EN;		//Enable DMA
}

void DMA_initiateSpiReception(char* where, int how_much){
	//In order to read the correct data we need to flush Rx fifo every time when we just write
	//otherwise unused trash gets stuck there and when we indeed need to read, we get some weird stuff
	//So during writing we will simply "read" the same amount of bytes we write, to the same foo variable
	//simply in order to keep the Rx fifo empty.
	static char foo = 0;
	if(where > 0 ){
		DMA_SPI_Rx_port->CTR1 |= DMA_CTR1_DINC;
		DMA_SPI_Rx_port->CDAR = (int)where;
	}else{
		DMA_SPI_Rx_port->CTR1 &= ~DMA_CTR1_DINC;	//If reading isn't needed, turn off destination increment in order to "read" into the same foo var
		DMA_SPI_Rx_port->CDAR = (int)&foo;
	}
	DMA_SPI_Rx_port->CBR1 = how_much;
	DMA_SPI_Rx_port->CCR |= DMA_CCR_EN;		//Enable DMA
}

void DMA_loadUartData(unsigned char* data, int how_much){
	DMA_UART_port->CSAR = (volatile long unsigned int)data;	//Pointing DMA at the source data location
	DMA_UART_port->CBR1 |= (1/*transfer 1 byte per block*/ + ((how_much - 1) << DMA_CBR1_BRC_Pos));	//Figuring out how many blocks to transfer, each block = one byte
}

void DMA_startUartTransfer(){
	if(tx_fifo->write_pointer == tx_fifo->read_pointer){	//^If there is nothing to send,
		return;
	}
	//**//Check if a transmission is already going on. In this case the ISR will handle everything
	if(TxInProgress == 0){
		TxInProgress = 1;
		NVIC_SetPendingIRQ(GPDMA1_Channel12_IRQn);
	}else{
		return;
	}
}

void DMA_resumeUartTransmission(void){
	DMA_UART_port->CCR &= ~(DMA_CCR_SUSP);
}

char DMA_UARTisBusy(void){
	//If some transmission is going on (BRC field is greater than 0)?
	if((DMA_UART_port->CBR1) & DMA_CBR1_BRC){
		return 1;
	}else{
		while(!((DMA_UART_port->CSR) & DMA_CSR_IDLEF)){}	//Wait for the last byte to get transferred
		return 0;
	}
}

void DMA_suspendUartTransmission(void){
	DMA_UART_port->CCR |= (DMA_CCR_SUSP);
}

bool DMA_isADCHealthy(void){
	return adcHealthy;
}

void DMA_acknwledgeADCHealthy(void){
	adcHealthy = false;
}

//ADC1 interrupt routine
void GPDMA1_Channel13_IRQHandler(){
	DMA_ADC1_port->CFCR |= DMA_CFCR_TCF;	//Clear "Transfer complete" flag
	adcHealthy = true;
	unsigned int* ADC_results = (unsigned int*)ADC_getADC1Data();
	/* Check the the boosters are working properly.
	 * The ADC pin hangs in between the Positive and Negative boosters.
	 * So, if the ADC reading is too high - it means that the negative booster has failed.
	 * If the reading is too low - it means that the positive booster failed.
	 * If the reading is lower than normal, but higher then during the positive booster failure
	 * it means that both boosters died.
	 * Since during both, positive booster failure and both boosters failure conditions the ADC reading
	 * becomes too low, no need to check for them separately every time (just to make code run faster).
	 * Once we see that the reading is low enough to signal a failure condition, then we will check,
	 * if both boosters have failed, or just the positive one.
	 * We will need five consecutive positive triggers to consider the situation faulty, because
	 * (especially on a breadboard) glitches do happen.
	 */
	static int p = 0;
	static int n = 0;
	if(BOOSTER_isOn()){
		if(ADC_results[BoostV] > NegBoostFailThr){
			if(p >= 4){
				main_markError(NegativeBoosterFailure);
				HELPER_systemHalt();
			}else p++;
		}else if(ADC_results[BoostV] < BothBoostFailThr){
			if(n >= 4){
				(ADC_results[BoostV] > PosBoostFailThr) ? main_markError(BothBoostersFailure) : main_markError(PositiveBoosterFailure);
				HELPER_systemHalt();
			}else n++;
		}else{
			(n <= 0) ? n = 0 : n--;
			(p <= 0) ? p = 0 : p--;
		}
	}
/*
 * Here we will calculate the resistance of the electrodes. Since we know the opamp output voltage
 * and the voltage on the electrodes, we can calculate the current. And since the same current
 * passes through both, the electrodes and resistor at the opamp output, we can then calculate
 * the resistance of the electrodes (electrodes voltage / the current).
 * The issue is on small amplitudes the mismatch of resistors at the electrode voltage divider
 * and all other components become the more substantial the smaller the amplitude is.
 */
	int currentSample = (abs(TIMER_getCurrentSample()) >> 1);	//Since sample swings positive and negative, we need an absolute value. We also match it with the 14-bit ADC resolution
	static int i = 0;
	if(currentSample > OFFTHRESH){
		int output = (int)ADC_results[Output]; //Since the ADC gets signal through a diode, at low voltages the drop introduces substantial bullshit
		float resistorVoltage = VoltPerCount * (float)(currentSample - (output + DIODE_DROP));
		float current = (resistorVoltage / OpAmpR) + 0.0000001;	//Just making sure we avoid division by zero
		int skinResistance = (int)(VoltPerCount * (float)2.0f * output / current) - PADS_RES;	//Since we measure resistance relative to the ground, but in fact we have a bridge, which doubles the voltage, so in order to get the right resistance, we need to multiply it by 2
		//When the resistor drop is zero (electrodes off), ADC may "see" a bigger value at the output,
		//which may end up in the "negative" resistance. It doesn't matter, since what we want to know
		//is that there is virtually no voltage drop on the resistors, which means that the electrodes
		//are most probably off
		if(abs(skinResistance) > RESTHRESH){
			if(i >= 40){
				HELPER_systemHalt();
				main_markError(ElectrodesOff);
			}else i++;
		}else (i <= 0) ? i = 0 : i--;
	}
	static short prevTestValueAmplitude = -1;
	static short prevTestValueFrequency = -1;
	static short prevTestValueOffTimer = -1;
	static short sliderFaultThreshold = 2400;
	//Add protection from a slider fault (or sudden disconnect). Since sliders have quite short life, if its value start "jumping",
	//consequently the current, or frequency passing through the head will jump also, which is obviously not desirable.
	//So, since humans cannot move the slider too quickly, if we see that two consecutive slider readings have a difference,
	//bigger than 15% of the total range, it means the slider is faulty and produces glitches.
	if((abs(ADC_results[Amplitude] - prevTestValueAmplitude) > sliderFaultThreshold) && (prevTestValueAmplitude >= 0)){
		HELPER_systemHalt();
		main_markError(SliderFault);
	}else if((abs(ADC_results[Freq] - prevTestValueFrequency) > sliderFaultThreshold) && (prevTestValueFrequency >= 0)){
		HELPER_systemHalt();
		main_markError(SliderFault);
	}else if((abs(ADC_results[OffTimer] - prevTestValueOffTimer) > sliderFaultThreshold) && (prevTestValueOffTimer >= 0)){
		HELPER_systemHalt();
		main_markError(SliderFault);
	}else{
		prevTestValueAmplitude = ADC_results[Amplitude];
		prevTestValueFrequency = ADC_results[Freq];
		prevTestValueOffTimer = ADC_results[OffTimer];
	}
	static short prevFreq = 0;
	static short prevAmplitude = 0;
	//No need to set a new amplitude, if the user hasn't changed it.
	//We will limit both amplitude and frequency steps to 10 bits in order to improve noise immunity.
	//Yet, we will feed the full 14bit value to CORDIC
	if(!TIMER_isFadeoutRunning()){	//When the final amplitude fade out is in progress, the timer module will take care of it
		if(prevAmplitude != (ADC_results[Amplitude] >> 4)){
			HELPER_Set_Cordic_m(ADC_results[Amplitude]);
			prevAmplitude = (ADC_results[Amplitude] >> 4);
		}
	}
	//If DAC gets frozen (may happen) and starts outputting DC - reset the system
	static int k = 0;
	if(ADC_results[Output] > abs((TIMER_getCurrentSample() >> 1)) + THRESH && TIMER_isSineRunning() && !main_getErrorCode()){	//Making the sample "compatible" with the 14-bit ADC
		if(k >= 20){
			__NVIC_SystemReset();
		}else k++;
	}else (k <= 0) ? k = 0 : k--;
	//Adjust the sine frequency if no automatic modulation requested.
	//Since this is a computationally expensive procedure, we will only perform it when the slider has moved.
	//In general, the frequency calculation formula uses 10 bits value, so we will keep it that way
	if((prevFreq != (ADC_results[Freq] >> 4)) && (!TIMER_isModulationRequested())){
		TIMER_setSineFreq(ADC_results[Freq] >> 4);
		prevFreq = (ADC_results[Freq] >> 4);
	}
	static unsigned char j = 0;
	//SLow down the blinking a bit in order to be able to see it
	if(!j++)BSP_LED_Toggle(LED1);
	else if(j >= 2) j = 0;
}

//UART tx interrupt routine
void GPDMA1_Channel12_IRQHandler(){
	DMA_UART_port->CFCR |= DMA_CFCR_TCF;	//Acknowledge "transfer complete" interrupt
	if(tx_fifo->write_pointer == tx_fifo->read_pointer){	//^If there is nothing to send,
		TxInProgress = 0;
		return;
	}
	DMA_UART_port->CSAR = (volatile long unsigned int)tx_fifo->data_pointer[tx_fifo->read_pointer];	//Pointing DMA at the source data location
	DMA_UART_port->CBR1 |= (1/*transfer 1 byte per block*/ + ((tx_fifo->how_much_data[tx_fifo->read_pointer] - 1) << DMA_CBR1_BRC_Pos));	//Figuring out how many blocks to transfer, each block = one byte
	tx_fifo->read_pointer += 1;
	if(tx_fifo->read_pointer == TX_BUFFER_SIZE){	//Keep FIFO within boundaries
		tx_fifo->read_pointer = 0;
	}
	DMA_UART_port->CCR |= (DMA_CCR_EN);	//Arming DMA channel
}

//SPI Rx handler
void GPDMA1_Channel8_IRQHandler(){
	DMA_SPI_Rx_port->CFCR |= DMA_CFCR_TCF;	//Acknowledge "transfer complete" interrupt
	SPI_EotInterruptOff();	//After a reception is done, switch off DMA reception
}
