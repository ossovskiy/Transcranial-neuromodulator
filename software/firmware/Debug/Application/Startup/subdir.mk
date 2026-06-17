################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Application/Startup/startup_stm32u575zitxq.s 

OBJS += \
./Application/Startup/startup_stm32u575zitxq.o 

S_DEPS += \
./Application/Startup/startup_stm32u575zitxq.d 


# Each subdirectory must supply rules for building sources it contributes
Application/Startup/%.o: ../Application/Startup/%.s Application/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m33 -g -c -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Application-2f-Startup

clean-Application-2f-Startup:
	-$(RM) ./Application/Startup/startup_stm32u575zitxq.d ./Application/Startup/startup_stm32u575zitxq.o

.PHONY: clean-Application-2f-Startup

