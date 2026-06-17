/*
 * DMA.h
 *
 *  Created on: Nov 11, 2022
 *      Author: Stanislav
 */
#include "stdbool.h"

#define DMA_UART_port GPDMA1_Channel12
#define DMA_ADC1_port GPDMA1_Channel13
#define DMA_DAC_port GPDMA1_Channel10
#define DMA_SPI_Tx_port GPDMA1_Channel9
#define DMA_SPI_Rx_port GPDMA1_Channel8

//ADC threshold for starting tracking disconnected electrodes detection - about a half of the voltage
//when the DAC sine sample is scaled down from 16bits to 14bits (ADC resolution)
#define OFFTHRESH 1000

//Minimal amplitude, because outputs are symmetrical, without any AC no way to tell if the electrodes are off
#define MINAMPLITUDE 750

//Turn the machine off if the resistance is above this threshold
#define RESTHRESH 25000

#define DATA_WIDTH_8	0
#define DATA_WIDTH_16	1
#define DATA_WIDTH_32	2

void DMA_uartFuncInit(void);
void DMA_adc1FuncInit(void);
void DMA_dacFuncInit(void);
void DMA_spiFuncInit(unsigned char sourceDataWidth, unsigned char destDataWidth);
void DMA_init(void);
unsigned int DMA_getADC1Data(void);
void DMA_startUartTransfer(void);
void DMA_startSpiTransfer(void);
void DMA_prepareADC1Transfer(unsigned int* destination);
void DMA_resumeUartTransmission(void);
void DMA_suspendUartTransmission(void);
char DMA_UARTisBusy(void);
void DMA_initiateSpiTransmission(char* data, int how_much);
void DMA_initiateSpiReception(char* where, int how_much);
bool DMA_isADCHealthy(void);
void DMA_acknwledgeADCHealthy(void);


