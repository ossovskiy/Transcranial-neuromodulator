/*
 * BOOSTER.h
 *
 *  Created on: Aug 30, 2025
 *      Author: stanislav
 */

#ifndef APPLICATION_USER_BOOSTER_H_
#define APPLICATION_USER_BOOSTER_H_

//Boost converters voltage problems thresholds
#define PosBoostFailThr 2000
#define NegBoostFailThr 8800
#define BothBoostFailThr 7200

#endif /* APPLICATION_USER_BOOSTER_H_ */
void BOOSTER_init(void);
void BOOSTER_off(void);
unsigned char BOOSTER_isOn(void);
void BOOSTER_healthCheck(void);
