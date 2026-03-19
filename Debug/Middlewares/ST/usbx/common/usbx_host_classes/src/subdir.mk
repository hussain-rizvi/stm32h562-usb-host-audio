################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.c \
../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.c 

OBJS += \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.o \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.o 

C_DEPS += \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.d \
./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ST/usbx/common/usbx_host_classes/src/%.o Middlewares/ST/usbx/common/usbx_host_classes/src/%.su Middlewares/ST/usbx/common/usbx_host_classes/src/%.cyclo: ../Middlewares/ST/usbx/common/usbx_host_classes/src/%.c Middlewares/ST/usbx/common/usbx_host_classes/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -DTX_SINGLE_MODE_NON_SECURE=1 -DUX_INCLUDE_USER_DEFINE_FILE -DUSE_HAL_DRIVER -DSTM32H562xx -DFX_INCLUDE_USER_DEFINE_FILE -c -I../Core/Inc -I../AZURE_RTOS/App -I../USBX/App -I../USBX/Target -I../Drivers/STM32H5xx_HAL_Driver/Inc -I../Drivers/STM32H5xx_HAL_Driver/Inc/Legacy -I../Middlewares/ST/threadx/common/inc -I../Drivers/CMSIS/Device/ST/STM32H5xx/Include -I../Middlewares/ST/threadx/ports/cortex_m33/gnu/inc -I../Middlewares/ST/usbx/common/core/inc -I../Middlewares/ST/usbx/ports/generic/inc -I../Middlewares/ST/usbx/common/usbx_stm32_host_controllers -I../Middlewares/ST/usbx/common/usbx_host_classes/inc -I../Drivers/CMSIS/Include -I../FileX/App -I../FileX/Target -I../Middlewares/ST/filex/common/inc -I../Middlewares/ST/filex/ports/generic/inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ST-2f-usbx-2f-common-2f-usbx_host_classes-2f-src

clean-Middlewares-2f-ST-2f-usbx-2f-common-2f-usbx_host_classes-2f-src:
	-$(RM) ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_activate.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_alternate_setting_locate.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_configure.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_request.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_control_value_set.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_deactivate.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptor_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_descriptors_parse.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_controls_list_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_device_type_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_endpoints_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entity_control_value_set.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.cyclo
	-$(RM) ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_entry.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_set.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_feedback_transfer_completed.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_notification.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_interrupt_start.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_raw_sampling_parse.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_read.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_stop.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_sampling_set.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_streaming_terminal_get.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_transfer_request_completed.su ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.cyclo ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.d ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.o ./Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_audio_write.su

.PHONY: clean-Middlewares-2f-ST-2f-usbx-2f-common-2f-usbx_host_classes-2f-src

