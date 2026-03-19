################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../USBX/App/app_usbx.c \
../USBX/App/app_usbx_host.c \
../USBX/App/ux_host_audio.c 

OBJS += \
./USBX/App/app_usbx.o \
./USBX/App/app_usbx_host.o \
./USBX/App/ux_host_audio.o 

C_DEPS += \
./USBX/App/app_usbx.d \
./USBX/App/app_usbx_host.d \
./USBX/App/ux_host_audio.d 


# Each subdirectory must supply rules for building sources it contributes
USBX/App/%.o USBX/App/%.su USBX/App/%.cyclo: ../USBX/App/%.c USBX/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -DTX_SINGLE_MODE_NON_SECURE=1 -DUX_INCLUDE_USER_DEFINE_FILE -DUSE_HAL_DRIVER -DSTM32H562xx -DFX_INCLUDE_USER_DEFINE_FILE -c -I../Core/Inc -I../AZURE_RTOS/App -I../USBX/App -I../USBX/Target -I../Drivers/STM32H5xx_HAL_Driver/Inc -I../Drivers/STM32H5xx_HAL_Driver/Inc/Legacy -I../Middlewares/ST/threadx/common/inc -I../Drivers/CMSIS/Device/ST/STM32H5xx/Include -I../Middlewares/ST/threadx/ports/cortex_m33/gnu/inc -I../Middlewares/ST/usbx/common/core/inc -I../Middlewares/ST/usbx/ports/generic/inc -I../Middlewares/ST/usbx/common/usbx_stm32_host_controllers -I../Middlewares/ST/usbx/common/usbx_host_classes/inc -I../Drivers/CMSIS/Include -I../FileX/App -I../FileX/Target -I../Middlewares/ST/filex/common/inc -I../Middlewares/ST/filex/ports/generic/inc -I../minimp3-master -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-USBX-2f-App

clean-USBX-2f-App:
	-$(RM) ./USBX/App/app_usbx.cyclo ./USBX/App/app_usbx.d ./USBX/App/app_usbx.o ./USBX/App/app_usbx.su ./USBX/App/app_usbx_host.cyclo ./USBX/App/app_usbx_host.d ./USBX/App/app_usbx_host.o ./USBX/App/app_usbx_host.su ./USBX/App/ux_host_audio.cyclo ./USBX/App/ux_host_audio.d ./USBX/App/ux_host_audio.o ./USBX/App/ux_host_audio.su

.PHONY: clean-USBX-2f-App

