/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    RCC/RCC_LSEConfig/Src/main.c
  * @author  MCD Application Team
  * @brief   This example describes how to use the RCC HAL API to configure the
  *          system clock (SYSCLK) and modify the clock settings on run time.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "UART.h"
#include "SPI.h"
#include "DMA.h"
#include "ADC.h"
#include "RTC.h"
#include "helper.h"
#include "OPAMP.h"
#include "TIMER.h"
#include "DAC.h"
#include "Ex_DAC.h"
#include "CORDIC.h"
#include "WATCHDOG.h"
#include <math.h>
#include "CLOCKING.h"
#include "GPIO.h"
#include "BUZZER.h"
#include "RANDGEN.h"

static unsigned char errorCode = 0;

void main_markError(unsigned char errCd){
	//Since we can only mark one error and each error stops the device, mark the first one.
	if(!errorCode) errorCode = errCd;
}

unsigned int main_getErrorCode(void){
	return errorCode;
}

int main(void){
  BSP_LED_Init(LED2);
  BSP_LED_Init(LED3);
  BSP_LED_Init(LED1);
  __disable_irq();
  CLOCKING_clockInit();
  RTC_init();
  HELPER_icacheInit();
  OPAMP_init();
  GPIOc_buttonInit();
  BUZZER_Init();
  RANDGEN_init();
  //UART_init();
  SPI_init(24);	//Datawidth is 24 bits
  ADC_init();
  __enable_irq();
  WD_init();
  Ex_DAC_Init();

  while (1){
	HELPER_isThereError(errorCode);
	__WFI();
  }
}
