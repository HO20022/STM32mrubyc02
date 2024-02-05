################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/mrubyc/alloc.c \
../Core/Src/mrubyc/c_array.c \
../Core/Src/mrubyc/c_hash.c \
../Core/Src/mrubyc/c_math.c \
../Core/Src/mrubyc/c_numeric.c \
../Core/Src/mrubyc/c_object.c \
../Core/Src/mrubyc/c_range.c \
../Core/Src/mrubyc/c_string.c \
../Core/Src/mrubyc/class.c \
../Core/Src/mrubyc/console.c \
../Core/Src/mrubyc/error.c \
../Core/Src/mrubyc/global.c \
../Core/Src/mrubyc/hal.c \
../Core/Src/mrubyc/keyvalue.c \
../Core/Src/mrubyc/load.c \
../Core/Src/mrubyc/mrblib.c \
../Core/Src/mrubyc/rrt0.c \
../Core/Src/mrubyc/symbol.c \
../Core/Src/mrubyc/value.c \
../Core/Src/mrubyc/vm.c 

OBJS += \
./Core/Src/mrubyc/alloc.o \
./Core/Src/mrubyc/c_array.o \
./Core/Src/mrubyc/c_hash.o \
./Core/Src/mrubyc/c_math.o \
./Core/Src/mrubyc/c_numeric.o \
./Core/Src/mrubyc/c_object.o \
./Core/Src/mrubyc/c_range.o \
./Core/Src/mrubyc/c_string.o \
./Core/Src/mrubyc/class.o \
./Core/Src/mrubyc/console.o \
./Core/Src/mrubyc/error.o \
./Core/Src/mrubyc/global.o \
./Core/Src/mrubyc/hal.o \
./Core/Src/mrubyc/keyvalue.o \
./Core/Src/mrubyc/load.o \
./Core/Src/mrubyc/mrblib.o \
./Core/Src/mrubyc/rrt0.o \
./Core/Src/mrubyc/symbol.o \
./Core/Src/mrubyc/value.o \
./Core/Src/mrubyc/vm.o 

C_DEPS += \
./Core/Src/mrubyc/alloc.d \
./Core/Src/mrubyc/c_array.d \
./Core/Src/mrubyc/c_hash.d \
./Core/Src/mrubyc/c_math.d \
./Core/Src/mrubyc/c_numeric.d \
./Core/Src/mrubyc/c_object.d \
./Core/Src/mrubyc/c_range.d \
./Core/Src/mrubyc/c_string.d \
./Core/Src/mrubyc/class.d \
./Core/Src/mrubyc/console.d \
./Core/Src/mrubyc/error.d \
./Core/Src/mrubyc/global.d \
./Core/Src/mrubyc/hal.d \
./Core/Src/mrubyc/keyvalue.d \
./Core/Src/mrubyc/load.d \
./Core/Src/mrubyc/mrblib.d \
./Core/Src/mrubyc/rrt0.d \
./Core/Src/mrubyc/symbol.d \
./Core/Src/mrubyc/value.d \
./Core/Src/mrubyc/vm.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/mrubyc/%.o Core/Src/mrubyc/%.su Core/Src/mrubyc/%.cyclo: ../Core/Src/mrubyc/%.c Core/Src/mrubyc/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F401xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Core/Inc/mrubyc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-mrubyc

clean-Core-2f-Src-2f-mrubyc:
	-$(RM) ./Core/Src/mrubyc/alloc.cyclo ./Core/Src/mrubyc/alloc.d ./Core/Src/mrubyc/alloc.o ./Core/Src/mrubyc/alloc.su ./Core/Src/mrubyc/c_array.cyclo ./Core/Src/mrubyc/c_array.d ./Core/Src/mrubyc/c_array.o ./Core/Src/mrubyc/c_array.su ./Core/Src/mrubyc/c_hash.cyclo ./Core/Src/mrubyc/c_hash.d ./Core/Src/mrubyc/c_hash.o ./Core/Src/mrubyc/c_hash.su ./Core/Src/mrubyc/c_math.cyclo ./Core/Src/mrubyc/c_math.d ./Core/Src/mrubyc/c_math.o ./Core/Src/mrubyc/c_math.su ./Core/Src/mrubyc/c_numeric.cyclo ./Core/Src/mrubyc/c_numeric.d ./Core/Src/mrubyc/c_numeric.o ./Core/Src/mrubyc/c_numeric.su ./Core/Src/mrubyc/c_object.cyclo ./Core/Src/mrubyc/c_object.d ./Core/Src/mrubyc/c_object.o ./Core/Src/mrubyc/c_object.su ./Core/Src/mrubyc/c_range.cyclo ./Core/Src/mrubyc/c_range.d ./Core/Src/mrubyc/c_range.o ./Core/Src/mrubyc/c_range.su ./Core/Src/mrubyc/c_string.cyclo ./Core/Src/mrubyc/c_string.d ./Core/Src/mrubyc/c_string.o ./Core/Src/mrubyc/c_string.su ./Core/Src/mrubyc/class.cyclo ./Core/Src/mrubyc/class.d ./Core/Src/mrubyc/class.o ./Core/Src/mrubyc/class.su ./Core/Src/mrubyc/console.cyclo ./Core/Src/mrubyc/console.d ./Core/Src/mrubyc/console.o ./Core/Src/mrubyc/console.su ./Core/Src/mrubyc/error.cyclo ./Core/Src/mrubyc/error.d ./Core/Src/mrubyc/error.o ./Core/Src/mrubyc/error.su ./Core/Src/mrubyc/global.cyclo ./Core/Src/mrubyc/global.d ./Core/Src/mrubyc/global.o ./Core/Src/mrubyc/global.su ./Core/Src/mrubyc/hal.cyclo ./Core/Src/mrubyc/hal.d ./Core/Src/mrubyc/hal.o ./Core/Src/mrubyc/hal.su ./Core/Src/mrubyc/keyvalue.cyclo ./Core/Src/mrubyc/keyvalue.d ./Core/Src/mrubyc/keyvalue.o ./Core/Src/mrubyc/keyvalue.su ./Core/Src/mrubyc/load.cyclo ./Core/Src/mrubyc/load.d ./Core/Src/mrubyc/load.o ./Core/Src/mrubyc/load.su ./Core/Src/mrubyc/mrblib.cyclo ./Core/Src/mrubyc/mrblib.d ./Core/Src/mrubyc/mrblib.o ./Core/Src/mrubyc/mrblib.su ./Core/Src/mrubyc/rrt0.cyclo ./Core/Src/mrubyc/rrt0.d ./Core/Src/mrubyc/rrt0.o ./Core/Src/mrubyc/rrt0.su ./Core/Src/mrubyc/symbol.cyclo ./Core/Src/mrubyc/symbol.d ./Core/Src/mrubyc/symbol.o ./Core/Src/mrubyc/symbol.su ./Core/Src/mrubyc/value.cyclo ./Core/Src/mrubyc/value.d ./Core/Src/mrubyc/value.o ./Core/Src/mrubyc/value.su ./Core/Src/mrubyc/vm.cyclo ./Core/Src/mrubyc/vm.d ./Core/Src/mrubyc/vm.o ./Core/Src/mrubyc/vm.su

.PHONY: clean-Core-2f-Src-2f-mrubyc

