/*
 * UART.c
 *
 *  Created on: Nov 1, 2022
 *      Author: Stanislav
 */
#include "main.h"
#include "DMA.h"
#include "GPIO.h"
#include "UART.h"

static Data_list tx_list;
static Data_list rx_list;

//PA9 - Tx, PA10 - Rx
void UART_init(void){
	//**FIFO pointers init
	tx_list.write_pointer = 0;
	rx_list.write_pointer = 0;
	tx_list.read_pointer = 0;
	rx_list.read_pointer = 0;
	//**
	NVIC_SetPriority(USART1_IRQn, 2);	//Initializing the UART interrupt
	NVIC_EnableIRQ(USART1_IRQn);
	GPIOa_powerUp();
	GPIOA->AFR[1] &= ~((15 << GPIO_AFRH_AFSEL9_Pos) + (15 << GPIO_AFRH_AFSEL10_Pos));	//^Connecting GPIOA pins 9 & 10 to USART1
	GPIOA->AFR[1] |= ((7 << GPIO_AFRH_AFSEL9_Pos) + (7 << GPIO_AFRH_AFSEL10_Pos));
	GPIOA->MODER &= ~(GPIO_MODER_MODE9 + GPIO_MODER_MODE10);	//^ Connecting GPIOA pins 9 and 10 as alternative function
	GPIOA->MODER |= (GPIO_MODER_MODE9_1 + GPIO_MODER_MODE10_1);
	RCC->APB2ENR |= (RCC_APB2ENR_USART1EN);	//UART clock on
	__NOP();
	__NOP();
	USART1->BRR = 2777;	//Baud rate = 160Mhz/2778~57600
	USART1->CR3 |= (USART_CR3_DMAT);	//Activating Tx DMA on UART
	DMA_uartFuncInit();
	USART1->ICR |= (USART_ICR_TCCF);	//Clear the "Transmission complete" flag
	//**Enable transmitter, receiver, activation in low power mode, enable FIFO, idle interrupt
	USART1->CR1 = (USART_CR1_TE + USART_CR1_RE + USART_CR1_UESM + USART_CR1_FIFOEN /*+ USART_CR1_IDLEIE*/);
	//**
	USART1->CR1 |= (USART_CR1_UE);	//Activating UART
	UART_send("Puink-puink~~ UART initialized", 30);
}

Data_list* UART_getTxFifo(void){
	return &tx_list;
}

void UART_send(unsigned char data[], int how_much){
	int next = (tx_list.write_pointer + 1);
	if(next >= TX_BUFFER_SIZE){	//Keep FIFO within boundaries
		next = 0;
	}
	//**If the tx fifo is full, wait
	while(next == tx_list.read_pointer){}
	//**Write data
	tx_list.data_pointer[tx_list.write_pointer] = data;
	tx_list.how_much_data[tx_list.write_pointer] = how_much;
	tx_list.write_pointer = next;
	DMA_startUartTransfer();
}

void USART1_IRQHandler(void){
}
