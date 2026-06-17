/*
 * OPAMP.c
 *
 *  Created on: Dec 28, 2022
 *      Author: Stanislav
 *
 *      OPAMP1 In+ - PA0
 *      OPAMP1 In- - PA1
 *      OPAMP1 Out - PA3
 *
 *      OPAMP2 In+ - PA6
 *      OPAMP2 In- - PA7
 *      OPAMP2 Out - PB0
 *
 */
#include "main.h"
#include "RTC.h"
#include "GPIO.h"

/*
 * We will use the OPAMP1 for buffering a fraction of the high-voltage, which is used for the main op amps.
 * This buffered fraction of high voltage will be used by ADC for monitoring "health" of our boost converter.
 * The output of OPAMP1 is internally connected to ADC channel 8, which means that OPAMP1 in+ is fed to ADC ch8
 * The amp is configured as a non-inverting follower.
 *
 * We will also use the second opamp to monitor if the electrodes are disconnected. It is also internally connected the
 * one of the ADC channels (channel 15)
 */

enum EnumAmps {AMP1, AMP2};
static OPAMP_TypeDef * OPAMPS[2] = {OPAMP1, OPAMP2};	//OPAMP addresses. Since both amps need calibration, we will use one calibration routine for both

void calibrate(unsigned char whichAmp){
	static int i = 0;
	OPAMPS[whichAmp]->CSR &= ~(OPAMP_CSR_HSM);	//Switching off high speed mode.
	OPAMPS[whichAmp]->CSR |= (OPAMP_CSR_CALON + OPAMP_CSR_USERTRIM);	//Calibration is on, using user trimming values

	OPAMPS[whichAmp]->CSR &= ~OPAMP_CSR_OPAEN;	//^Switch the amp on
	OPAMPS[whichAmp]->CSR |= OPAMP_CSR_OPAEN;	//^Switch the amp on
	OPAMPS[whichAmp]->CSR &= ~(OPAMP_CSR_OPALPM + OPAMP_CSR_CALSEL);	//Normal mode & high side calibration
	OPAMPS[whichAmp]->OTR &= ~(OPAMP_OTR_TRIMOFFSETN_Msk);	//Preparing the trimming field
	RTC_wait(32);		//Wait before accessing calibration results
	while(!(OPAMPS[whichAmp]->CSR & OPAMP_CSR_CALOUT)){	//Keep incrementing offset values until the CALOUT bit sets to 1
		OPAMPS[whichAmp]->OTR &= ~(OPAMP_OTR_TRIMOFFSETN_Msk);
		OPAMPS[whichAmp]->OTR |= (++i << OPAMP_OTR_TRIMOFFSETN_Pos);	//Increment the offset value
		RTC_wait(32);		//Wait before accessing calibration results
	}
	OPAMPS[whichAmp]->CSR |= OPAMP_CSR_CALSEL;	//Normal mode & low side calibration
	i = 0;
	OPAMPS[whichAmp]->OTR &= ~(OPAMP_OTR_TRIMOFFSETP_Msk);	//Preparing the trimming field
	RTC_wait(32);		//Wait before accessing calibration results
	while(!(OPAMPS[whichAmp]->CSR & OPAMP_CSR_CALOUT)){	//Keep incrementing offset values until the CALOUT bit sets to 1
		OPAMPS[whichAmp]->OTR &= ~(OPAMP_OTR_TRIMOFFSETP_Msk);
		OPAMPS[whichAmp]->OTR |= (++i << OPAMP_OTR_TRIMOFFSETP_Pos);	//Increment the offset value
		RTC_wait(32);		//Wait before accessing calibration results
	}
	OPAMPS[whichAmp]->CSR &= ~(OPAMP_CSR_CALSEL);	//^Low power mode & high side calibration
	OPAMPS[whichAmp]->CSR &= ~OPAMP_CSR_OPAEN;	//^Switch the amp off before turning on the LP mode
	OPAMPS[whichAmp]->CSR |= OPAMP_CSR_OPALPM;
	OPAMPS[whichAmp]->CSR |= OPAMP_CSR_OPAEN;	//Switch the amp on
	i = 0;
	OPAMPS[whichAmp]->LPOTR &= ~(OPAMP_LPOTR_TRIMLPOFFSETN_Msk);	//Preparing the trimming field
	RTC_wait(32);		//Wait before accessing calibration results
	while(!(OPAMPS[whichAmp]->CSR & OPAMP_CSR_CALOUT)){	//Keep incrementing offset values until the CALOUT bit sets to 1
		OPAMPS[whichAmp]->LPOTR &= ~(OPAMP_LPOTR_TRIMLPOFFSETN_Msk);
		OPAMPS[whichAmp]->LPOTR |= (++i << OPAMP_LPOTR_TRIMLPOFFSETN_Pos);	//Increment the offset value
		RTC_wait(32);		//Wait before accessing calibration results
	}
	OPAMPS[whichAmp]->CSR |= OPAMP_CSR_CALSEL;	//^Low power mode & low side calibration
	i = 0;
	OPAMPS[whichAmp]->LPOTR &= ~(OPAMP_LPOTR_TRIMLPOFFSETP_Msk);	//Preparing the trimming field
	RTC_wait(32);		//Wait before accessing calibration results
	while(!(OPAMPS[whichAmp]->CSR & OPAMP_CSR_CALOUT)){	//Keep incrementing offset values until the CALOUT bit sets to 1
		OPAMPS[whichAmp]->LPOTR &= ~(OPAMP_LPOTR_TRIMLPOFFSETP_Msk);
		OPAMPS[whichAmp]->LPOTR |= (++i << OPAMP_LPOTR_TRIMLPOFFSETP_Pos);	//Increment the offset value
		RTC_wait(32);		//Wait before accessing calibration results
	}
	i = 0;
	OPAMPS[whichAmp]->CSR &= ~OPAMP_CSR_OPAEN;	//Switch the amp off
	OPAMPS[whichAmp]->CSR &= ~OPAMP_CSR_CALON;	//Calibration done
	OPAMPS[whichAmp]->CSR &= ~OPAMP_CSR_OPALPM;	//Disable low power mode
}

//Gain table: 1 << (number + 1)
void OPAMP_init(void){
	//!!configure ports if need be
	enum EnumAmps currentAmp = AMP1;
	GPIOa_powerUp();
	GPIOb_powerUp();
	RCC->APB3ENR |= RCC_APB3ENR_OPAMPEN;	//^Powering up OPAMPs
	OPAMPS[currentAmp]->CSR |= OPAMP_CSR_OPARANGE; //This bit must be set before enabling the OPAMP and this bit affects all OPAMP instances.
	calibrate(currentAmp);
	//Negative input not connected, amp is configured as a non-inverting internal buffer, high speed selected for better precision
	OPAMPS[currentAmp]->CSR |= ((3 << OPAMP_CSR_OPAMODE_Pos) + OPAMP_CSR_VM_SEL_1 + OPAMP_CSR_HSM);
	OPAMPS[currentAmp]->CSR &= ~(OPAMP_CSR_VP_SEL);	//Positive input is connected to the PA0 GPIO pin
	OPAMPS[currentAmp]->CSR |= OPAMP_CSR_OPARANGE;
	OPAMPS[currentAmp]->CSR |= OPAMP_CSR_OPAEN;	//Switch the amp on
	currentAmp = AMP2;
	OPAMPS[currentAmp]->CSR |= OPAMP_CSR_OPARANGE;
	calibrate(currentAmp);
	//Negative input not connected, amp is configured as a non-inverting internal buffer, high speed selected for better precision
	OPAMPS[currentAmp]->CSR |= ((3 << OPAMP_CSR_OPAMODE_Pos) + OPAMP_CSR_VM_SEL_1 + OPAMP_CSR_HSM);
	OPAMPS[currentAmp]->CSR &= ~(OPAMP_CSR_VP_SEL);	//Positive input is connected to the PA6 GPIO pin
	OPAMPS[currentAmp]->CSR |= OPAMP_CSR_OPARANGE;
	OPAMPS[currentAmp]->CSR |= OPAMP_CSR_OPAEN;	//Switch the amp on
}

int OPAMP_getAmp1Gain(void){
	return (1 << ((((OPAMP1->CSR) & OPAMP_CSR_PGA_GAIN_Msk) >> OPAMP_CSR_PGA_GAIN_Pos) + 1));
}

int OPAMP_getAmp2Gain(void){
	return (1 << ((((OPAMP2->CSR) & OPAMP_CSR_PGA_GAIN_Msk) >> OPAMP_CSR_PGA_GAIN_Pos) + 1));
}
