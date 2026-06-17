################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/User/ADC.c \
../Application/User/BOOSTER.c \
../Application/User/BUZZER.c \
../Application/User/CLOCKING.c \
../Application/User/CORDIC.c \
../Application/User/DAC.c \
../Application/User/DMA.c \
../Application/User/Ex_DAC.c \
../Application/User/GPIO.c \
../Application/User/OPAMP.c \
../Application/User/RANDGEN.c \
../Application/User/RTC.c \
../Application/User/SPI.c \
../Application/User/TIMER.c \
../Application/User/UART.c \
../Application/User/WATCHDOG.c \
../Application/User/helper.c \
../Application/User/main.c \
../Application/User/stm32u5xx_hal_msp.c \
../Application/User/stm32u5xx_it.c \
../Application/User/syscalls.c \
../Application/User/sysmem.c \
../Application/User/system_stm32u5xx.c 

OBJS += \
./Application/User/ADC.o \
./Application/User/BOOSTER.o \
./Application/User/BUZZER.o \
./Application/User/CLOCKING.o \
./Application/User/CORDIC.o \
./Application/User/DAC.o \
./Application/User/DMA.o \
./Application/User/Ex_DAC.o \
./Application/User/GPIO.o \
./Application/User/OPAMP.o \
./Application/User/RANDGEN.o \
./Application/User/RTC.o \
./Application/User/SPI.o \
./Application/User/TIMER.o \
./Application/User/UART.o \
./Application/User/WATCHDOG.o \
./Application/User/helper.o \
./Application/User/main.o \
./Application/User/stm32u5xx_hal_msp.o \
./Application/User/stm32u5xx_it.o \
./Application/User/syscalls.o \
./Application/User/sysmem.o \
./Application/User/system_stm32u5xx.o 

C_DEPS += \
./Application/User/ADC.d \
./Application/User/BOOSTER.d \
./Application/User/BUZZER.d \
./Application/User/CLOCKING.d \
./Application/User/CORDIC.d \
./Application/User/DAC.d \
./Application/User/DMA.d \
./Application/User/Ex_DAC.d \
./Application/User/GPIO.d \
./Application/User/OPAMP.d \
./Application/User/RANDGEN.d \
./Application/User/RTC.d \
./Application/User/SPI.d \
./Application/User/TIMER.d \
./Application/User/UART.d \
./Application/User/WATCHDOG.d \
./Application/User/helper.d \
./Application/User/main.d \
./Application/User/stm32u5xx_hal_msp.d \
./Application/User/stm32u5xx_it.d \
./Application/User/syscalls.d \
./Application/User/sysmem.d \
./Application/User/system_stm32u5xx.d 


# Each subdirectory must supply rules for building sources it contributes
Application/User/%.o Application/User/%.su Application/User/%.cyclo: ../Application/User/%.c Application/User/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U575xx -c -I../../Inc -I../../Drivers/STM32U5xx_HAL_Driver/Inc -I../../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../../Drivers/CMSIS/Include -I../../Drivers/BSP/STM32U5xx_Nucleo -O1 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-User

clean-Application-2f-User:
	-$(RM) ./Application/User/ADC.cyclo ./Application/User/ADC.d ./Application/User/ADC.o ./Application/User/ADC.su ./Application/User/BOOSTER.cyclo ./Application/User/BOOSTER.d ./Application/User/BOOSTER.o ./Application/User/BOOSTER.su ./Application/User/BUZZER.cyclo ./Application/User/BUZZER.d ./Application/User/BUZZER.o ./Application/User/BUZZER.su ./Application/User/CLOCKING.cyclo ./Application/User/CLOCKING.d ./Application/User/CLOCKING.o ./Application/User/CLOCKING.su ./Application/User/CORDIC.cyclo ./Application/User/CORDIC.d ./Application/User/CORDIC.o ./Application/User/CORDIC.su ./Application/User/DAC.cyclo ./Application/User/DAC.d ./Application/User/DAC.o ./Application/User/DAC.su ./Application/User/DMA.cyclo ./Application/User/DMA.d ./Application/User/DMA.o ./Application/User/DMA.su ./Application/User/Ex_DAC.cyclo ./Application/User/Ex_DAC.d ./Application/User/Ex_DAC.o ./Application/User/Ex_DAC.su ./Application/User/GPIO.cyclo ./Application/User/GPIO.d ./Application/User/GPIO.o ./Application/User/GPIO.su ./Application/User/OPAMP.cyclo ./Application/User/OPAMP.d ./Application/User/OPAMP.o ./Application/User/OPAMP.su ./Application/User/RANDGEN.cyclo ./Application/User/RANDGEN.d ./Application/User/RANDGEN.o ./Application/User/RANDGEN.su ./Application/User/RTC.cyclo ./Application/User/RTC.d ./Application/User/RTC.o ./Application/User/RTC.su ./Application/User/SPI.cyclo ./Application/User/SPI.d ./Application/User/SPI.o ./Application/User/SPI.su ./Application/User/TIMER.cyclo ./Application/User/TIMER.d ./Application/User/TIMER.o ./Application/User/TIMER.su ./Application/User/UART.cyclo ./Application/User/UART.d ./Application/User/UART.o ./Application/User/UART.su ./Application/User/WATCHDOG.cyclo ./Application/User/WATCHDOG.d ./Application/User/WATCHDOG.o ./Application/User/WATCHDOG.su ./Application/User/helper.cyclo ./Application/User/helper.d ./Application/User/helper.o ./Application/User/helper.su ./Application/User/main.cyclo ./Application/User/main.d ./Application/User/main.o ./Application/User/main.su ./Application/User/stm32u5xx_hal_msp.cyclo ./Application/User/stm32u5xx_hal_msp.d ./Application/User/stm32u5xx_hal_msp.o ./Application/User/stm32u5xx_hal_msp.su ./Application/User/stm32u5xx_it.cyclo ./Application/User/stm32u5xx_it.d ./Application/User/stm32u5xx_it.o ./Application/User/stm32u5xx_it.su ./Application/User/syscalls.cyclo ./Application/User/syscalls.d ./Application/User/syscalls.o ./Application/User/syscalls.su ./Application/User/sysmem.cyclo ./Application/User/sysmem.d ./Application/User/sysmem.o ./Application/User/sysmem.su ./Application/User/system_stm32u5xx.cyclo ./Application/User/system_stm32u5xx.d ./Application/User/system_stm32u5xx.o ./Application/User/system_stm32u5xx.su

.PHONY: clean-Application-2f-User

