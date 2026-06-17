/*
 * UART.h
 *
 *  Created on: Nov 1, 2022
 *      Author: Stanislav
 */

#define TX_BUFFER_SIZE 16

//**This list will hold links to data to be sent/received by UART
typedef struct{
	unsigned char* data_pointer[TX_BUFFER_SIZE];
	unsigned int how_much_data[TX_BUFFER_SIZE];
	int write_pointer;
	int read_pointer;
}Data_list;

void UART_init(void);
void UART_send(unsigned char data[], int how_much);
Data_list* UART_getTxFifo(void);


