/*
 * ADC.h
 *
 *  Created on: Nov 25, 2022
 *      Author: Stanislav
 */
#define NO_OF_ADC1_CHANNELS 7

/*
 * ADC channels are as follows:
 * 0 - Battery
 * 1 - Core temperature
 * 2 - Amplitude pot
 * 3 - Frequency pot
 * 4 - Auto-off timer
 * 5 - Boost converter voltage health check
 * 6 - Output check for the electrodes disconnect detection
 */

typedef enum AdcChs{
	 Battery,
	 Coretemp,
	 Amplitude,
	 Freq,
	 OffTimer,
	 BoostV,
	 Output
} adcChannels;


void ADC_init(void);
unsigned int* ADC_getADC1Data(void);
void ADC_calibrate(void);
void ADC_stop(void);
