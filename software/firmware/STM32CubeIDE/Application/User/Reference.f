!!!__!!! PB3 PA14 PA13 - must be reserved for the debugger!!

**Pins:
ACC: PB4 - Vdd, On the nucleo board the jumpers SB36 and SB43 must be open
ADC: PA6 (opamp2), PA0 (opamp1), Unsolder SB3 bridge on the board, PA7 - ADC1_IN12, PA6 - ADC1_IN11, PA5 - ADC1_IN10
DAC: DAC output - PA4, PA5(not used) 
Opamp:  OPAMP1 In+ - PA0
 *      OPAMP1 In- - PA1 (not used - free)
 *      OPAMP1 Out - PA3
 *
 *      OPAMP2 In+ - PA6, 
 *      OPAMP2 In- - PA7, (not used - free)
 *      OPAMP2 Out - PB0
 SPI (used for external DAC): PA1 - SCK, PA11 - MISO, PA12 - MOSI, PA15 - SS, On the NUCLEO board jumpers SB41, SB42 must be On (shorted).  Maybe: SB39 On, SB8 On
 Timertor: PB13 - CH1N, PB14 - CH2N, PA8 - CH1, !PA9 - CH2
 USART: !PA9 - Tx, PA10 - Rx

Output connect-disconnect transistor control - PB1
Booster enable pin - PB10

Firmware logic.

The purpose is to output a high-voltage, high-resolution sine wave that should pass through the brain and potentially alter anxiety levels or sleep patterns.

1. Initialise the MCU clocking and set up all the frequencies, including the real-time LSE crystal. Main speed is 160MHz, derived by PLL, with LSE used as the frequency stabiliser. 

2. Start MCU cache block for speed improvement.

3. Initialise ADC, which will use 6 channels for various diagnostic and control parameters. 1024 oversampling, 10-bit right shift, which yields 14-bit results. The ADC conversion will happen non-stop in a circular way through all the channels.
/*
 * ADC channels are as follows:
 * 0 - Battery
 * 1 - Core temperature
 * 2 - Amplitude pot
 * 3 - Frequency pot
 * 4 - Auto-off timer
 * 5 - Boost converter voltage health check
 * 6 - Electrode detachment reading channel
 */
All the peripherals, which can use DMA will configure their corresponding DMA channel.
DMA will issue a periodic interrupt, when all the ADC data is ready.

4. UART init, not used in this project.

5. Configuring SPI for use with the external DAC.

6. Starting an onboard op amp, which is configured as a non-inverting buffer, connected to a fraction of the high voltage (50V), produced by an external boost converter. Since this fraction is acquired by a voltage divider, it has quite a high impedance, so a buffer is necessary, because we need this fraction to be digitized by the ADC, which doesn't tolerate such high impedances.

7. Watchdog timer initialization. This timer is used not only to ensure that the system is healthy, but also to check if the auto-off time has expired. In such a case, we turn the sine generator off. The watchdog also checks the temperature. In case of overheat it will turn everything off (including the booster).

8. External DAC initialization. Since the voltage that the DAC uses is 3.3V, we need to divide the reference by two and multiply the output by two to get the 2.5V range. During initialization, we switch on the boost voltage converter and firstly check (by means of an ADC channel) if the output voltage stays in some reasonable, healthy boundaries. If not, we switch the boost converter off, check if the voltage was too high or too low, and record the corresponding error code. 

9. The DAC init function then starts the timer, which will configure the MCU's on-board sine function accelerator and generate sine samples at pre-defined equal intervals, determined by the ADC-connected "frequency" slider, and then those samples will be output via the external DAC. The configurer will check if the "amplitude" slider is not at the minimum position and wait until the user sets the slider to the minimum. This is done to prevent a sudden high current passing through the brain. It will also check for the over- and undervoltage, which means the boost converter failure. It will also check if the button is pressed. The short press will just start generation, while the long press will initiate the automatic modulation from 1hz down to 0.64hz, then up to 2hz and then back to 1hz. The whole cycle will take about 45 minutes. The timer configurer also records the real-time system start moment, and lets the watchdog know the desired auto-off delay period (in seconds), which the watchdog will be checking about every 2 seconds.

After this init phase, the system will periodically output a sample of a sine wave and constantly digitize and check changes in the desired amplitude, frequency, and auto-off time period. As well, it will check the battery voltage health (needs to be implemented), core temperature, and also monitor the boost converter's output voltage. It will measure the electrodes' resistance and check if they have disconnected. In case the boost converter voltage deviates too much from 50V, the converter is turned off, and the error code (if the voltage was too low or too high) is recorded for later processing in the main routine. The core temperature is constantly monitored by the Watchdog timer. If it passes 70c the system is stopped. If the electrodes are off, the system will halt.

The main routine passes the device status (if there is some error, or not) to a helper function, which does nothing, if everything is okay, or, in case of an error, switches on a corresponding LED indication. 

It is necessary to point out that the active filter should have the corner frequency about 100Hz, despite the working frequency being <= Hz. This is due to the necessity of high linearity in the whole working frequency range, because otherwise the electrode detachment detection becomes unreliable. The capacitor at the voltage divider that reads if electrodes are detached should be about 300p for the same reason. 