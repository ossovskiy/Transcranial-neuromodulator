/*
 * SPI.h
 *
 *  Created on: Jun 30, 2023
 *      Author: Stanislav
 */

#define SPI_FIFO_SIZE 256
#define WRITE 0
#define READ 1
//These two control the SS pin
#define SPI_deactivate() GPIOA->BSRR |= GPIO_BSRR_BS15
#define SPI_activate() GPIOA->BRR |= GPIO_BRR_BR15
//A shortcut for SS pin deactivation (we just need to send whatever over SPI with the Tx field == 0
#define SPI_finishTransaction() SPI_commit(0, 0, 0)
//a virtual function for switching off the EOT interrupt, to be used in the DMA module
#define SPI_EotInterruptOff() SPI1->CFG1 &= ~(SPI_CFG1_RXDMAEN)


typedef struct{
	struct{
		char* tx_data_pointer;
		char* rx_data_pointer;
		int how_much;
	}data[SPI_FIFO_SIZE];
	unsigned int write_pointer;
	unsigned int read_pointer;
}fifo;

void SPI_init(unsigned char dataWidth);
void SPI_commit(char* tx_data, char* rx_data, int how_much);
