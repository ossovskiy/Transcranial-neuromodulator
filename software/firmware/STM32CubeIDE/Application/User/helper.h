/*
 * helper.h
 *
 *  Created on: Oct 20, 2022
 *      Author: Stanislav
 */

typedef enum{
		AllOkay,
		PositiveBoosterFailure,
		NegativeBoosterFailure,
		BothBoostersFailure,
		Overheat,
		ElectrodesOff,
		DeadBattery,
		SliderFault
}Errors;

void HELPER_numToASCII(unsigned int num, unsigned int num_length, char* result);
void HELPER_generateAngles(int * where, int howMany);
void HELPER_icacheInit(void);
int HELPER_Get_Cordic_m(void);
float HELPER_Get_Temperature(void);
float HELPER_Get_Vbat(void);
void HELPER_Set_Cordic_m(int newCordic_m);
void HELPER_isThereError(unsigned char errorCode);
unsigned int HELPER_buttonIsPressed(void);
void HELPER_systemHalt(void);
