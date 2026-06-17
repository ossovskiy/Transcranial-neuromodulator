/*
 * TIMER.h
 *
 *  Created on: Apr 9, 2023
 *      Author: Stanislav
 */
#include "stdbool.h"

void TIMER_motor_init(void);
void TIMER_dac_init(void);
void TIMER_setSineFreq(unsigned int newFrequency);
void TIMER_enoughPlease(void);
unsigned int TIMER_isSineRunning(void);
unsigned short TIMER_isModulationRequested(void);
void TIMER_clearModulationRequest(void);
short TIMER_getCurrentSample(void);
unsigned short TIMER_getAveragedSample(void);
void TIMER_setModulationRequest(void);
unsigned int TIMER_isFadeoutRunning(void);
unsigned short TIMER_getCurrentFadeOutAmplitude(void);
void TIMER_jumpToEndOfModulation(void);
void TIMER_setAndCalculateFadeOutTime(unsigned int time);
void TIMER_acknowledgeTimer6Healthy(void);
void TIMER_acknowledgeTimer7Healthy(void);
bool TIMER_isTimer6Healthy(void);
bool TIMER_isTimer7Healthy(void);
