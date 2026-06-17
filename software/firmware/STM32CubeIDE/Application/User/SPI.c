/*
 * SPI.c
 *
 *  Created on: Jun 30, 2023
 *      Author: Stanislav
 */

#include "main.h"
#include "UART.h"
#include "helper.h"
#include "GPIO.h"
#include "DMA.h"
#include "SPI.h"

/*SPI pins: PA1 - SCK, PA11 - MISO, PA12 - MOSI, PA15 - SS (manual)
 * Every pin has SPI function assigned to the AF5
 *
 * On the NUCLEO board jumpers SB41, SB42 must be On (shorted).  Maybe: SB39 On, SB8 On
 */

static fifo spi_fifo;

void SPI_init(unsigned char dataWidth){
	GPIOa_powerUp();
	GPIOb_powerUp();
	//Connecting GPIOA pins 1, 11, 12, 15 to SPI1
	GPIOA->AFR[0] &= ~(15 << GPIO_AFRL_AFSEL1_Pos);
	GPIOA->AFR[1] &= ~((15 << GPIO_AFRH_AFSEL11_Pos) + (15 << GPIO_AFRH_AFSEL12_Pos) + (15 << GPIO_AFRH_AFSEL15_Pos));
	GPIOA->AFR[0] |= (5 << GPIO_AFRL_AFSEL1_Pos);
	GPIOA->AFR[1] |= ((5 << GPIO_AFRH_AFSEL11_Pos) + (5 << GPIO_AFRH_AFSEL12_Pos) + (5 << GPIO_AFRH_AFSEL15_Pos));
	//GPIOA pins 1, 11, 12, 15 to the alternative functions module
	GPIOA->MODER &= ~(GPIO_MODER_MODE1 + GPIO_MODER_MODE11 + GPIO_MODER_MODE12 + GPIO_MODER_MODE15);
	GPIOA->MODER |= (GPIO_MODER_MODE1_1 + GPIO_MODER_MODE11_1 + GPIO_MODER_MODE12_1 + /*GPIO_MODER_MODE15_0*/GPIO_MODER_MODE15_1);	//PA15 is connected as a GPIO output, used as CS and we trigger it manually

	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;		//Connect SPI to the bus
	//RCC->APB2SMENR |= RCC_APB2SMENR_SPI1SMEN;		//SPI clock is enabled during low power modes
	SPI1->CR1 &= ~(SPI_CR1_SPE);	//Disable SPI
	//SPI clock is 160/16 Mhz, Tx and Rx DMA requests enabled, SPI data - the required number minus 1 (datasheet requirement)
	SPI1->CFG1 = 0;	//Clearing the configuration register in case we will need to call the init function again
	SPI1->CFG1 |= (3 << SPI_CFG1_MBR_Pos) + SPI_CFG1_TXDMAEN /*+ SPI_CFG1_RXDMAEN*/ + ((dataWidth-1) << SPI_CFG1_DSIZE_Pos);
	/*SPI keeps control over pins even when disabled, data frame interleave enabled
	 * (needs to be specified by MIDI bits),
	 * idle clock polarity is low, data sampled on the second clock transition edge, SPI works in the master mode
	 * every data frame is interleaved with the SS non-active pulses (13 clock cycles wide). This is done according to the DAC datasheet p. 6, timing requirements
	 */
	SPI1->CFG2 = 0;
	SPI1->CFG2 |= SPI_CFG2_AFCNTR + SPI_CFG2_SSOE + SPI_CFG2_SSOM + (13 << SPI_CFG2_MIDI_Pos) + SPI_CFG2_CPHA + SPI_CFG2_MASTER;
	NVIC_SetPriority(SPI1_IRQn, 2);	//Initializing the SPI interrupt
	NVIC_EnableIRQ(SPI1_IRQn);
	DMA_spiFuncInit(DATA_WIDTH_32, DATA_WIDTH_32);	//Source and destination data widths for DMA bursts
}

/*
 * This function does both reading and writing on the SPI1 interface
 * both work similarly: by placing a task into FIFO
 * in case of writing, we just point at the source data and load it into the SPI via DMA.
 * We still do reading even though we don't need the data. The reason is, otherwise Rx fifo gets
 * random garbage and when we actually do need to read some data, we will have to deal with a random
 * amount of Rx fifo trash. So while writing only, we at the same time read the same amount of bytes
 * into the same foo char variable. Simply in order to preserve Rx fifo empty until we really need to read something.
 * In case of reading, we point both at the source and destination.
 *
 * Function tricks: if tx_data == 0, nothing will be read, or written. Instead the SS pin will be deactivated
 * until processing the next fifo entry.
 * if rx_data == 0, it means that we don't want to read anything, just write. So the reading will proceed into the foo variable
 * declared in the DMA module
 * Although the SS pin get low (active) automatically once a transmission starts, don't forget to deactivate it as a transmission ends
 */
//!! Since DMA always counts data by bytes, when we tell it how much data to transfer, we do so in bytes, even when we want to transfer words
void SPI_commit(char* tx_data, char* rx_data, int how_much){
	int next = spi_fifo.write_pointer + 1;
	if(next >= SPI_FIFO_SIZE){	//Keeping fifo within boundaries
		next = 0;
	}
	//Check if FIFO is full, in which case turn on SPI and wait while at least one slot becomes available
	if(next == spi_fifo.read_pointer){
		SPI1->CR1 |= SPI_CR1_SPE;	//Enable SPI and EOT/Empty interrupt
		SPI1->IER |= SPI_IER_EOTIE;
		while(next == spi_fifo.read_pointer);
	}
	spi_fifo.data[spi_fifo.write_pointer].tx_data_pointer = tx_data;
	spi_fifo.data[spi_fifo.write_pointer].rx_data_pointer = rx_data;
	spi_fifo.data[spi_fifo.write_pointer].how_much = how_much;
	spi_fifo.write_pointer = next;
	SPI1->CR1 |= SPI_CR1_SPE;	//Enable SPI and EOT/Empty interrupt
	SPI1->IER |= SPI_IER_EOTIE;
}

//The IRQ looks into the FIFO, until it is empty and decides if we will read from SPI, or write into it.
//We do one entry at a time. When the FIFO gets empty, switch off the EOT interrupt and exit.
void SPI1_IRQHandler(void){
	SPI1->IFCR = SPI_IFCR_EOTC;	//Clearing EOT interrupt flag;
	if(spi_fifo.read_pointer == spi_fifo.write_pointer){	//If fifo is empty, switch off the transmitter, interrupt and exit
		SPI1->CR1 &= ~(SPI_CR1_SPE);
		SPI1->IER &= ~(SPI_IER_EOTIE);
		return;
	}else{
		//if tx pointer is zero, just deactivate SS, no need to read or write anything
		if(spi_fifo.data[spi_fifo.read_pointer].tx_data_pointer != 0){
			//Load the address of the source data from fifo into the DMA
			DMA_initiateSpiTransmission(spi_fifo.data[spi_fifo.read_pointer].tx_data_pointer, spi_fifo.data[spi_fifo.read_pointer].how_much);
			//SPI_activate();
			SPI1->CFG1 |= SPI_CFG1_RXDMAEN;		//Enable DMA Rx transfer (To be disabled through the DMA "transfer complete" IRQ routine)
			DMA_initiateSpiReception(spi_fifo.data[spi_fifo.read_pointer].rx_data_pointer, spi_fifo.data[spi_fifo.read_pointer].how_much);
		}else{
			//SPI_deactivate();
		}
		spi_fifo.read_pointer++;
		if(spi_fifo.read_pointer >= SPI_FIFO_SIZE){	//Keep the pointer in bondaries
			spi_fifo.read_pointer = 0;
		}
		SPI1->CR1 |= SPI_CR1_CSTART;		//Start SPI transmission
	}
}

