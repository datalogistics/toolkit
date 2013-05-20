################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/lodn1.5_client/src/client.pb-c.c \
../src/lodn1.5_client/src/datatypes.pb-c.c \
../src/lodn1.5_client/src/jrb.c \
../src/lodn1.5_client/src/jval.c \
../src/lodn1.5_client/src/lodn_api.c \
../src/lodn1.5_client/src/lodn_dispatcher_policy.c \
../src/lodn1.5_client/src/lodn_errno.c \
../src/lodn1.5_client/src/lodn_io.c \
../src/lodn1.5_client/src/lodn_url.c \
../src/lodn1.5_client/src/protocol_envelope.c 

OBJS += \
./src/lodn1.5_client/src/client.pb-c.o \
./src/lodn1.5_client/src/datatypes.pb-c.o \
./src/lodn1.5_client/src/jrb.o \
./src/lodn1.5_client/src/jval.o \
./src/lodn1.5_client/src/lodn_api.o \
./src/lodn1.5_client/src/lodn_dispatcher_policy.o \
./src/lodn1.5_client/src/lodn_errno.o \
./src/lodn1.5_client/src/lodn_io.o \
./src/lodn1.5_client/src/lodn_url.o \
./src/lodn1.5_client/src/protocol_envelope.o 

C_DEPS += \
./src/lodn1.5_client/src/client.pb-c.d \
./src/lodn1.5_client/src/datatypes.pb-c.d \
./src/lodn1.5_client/src/jrb.d \
./src/lodn1.5_client/src/jval.d \
./src/lodn1.5_client/src/lodn_api.d \
./src/lodn1.5_client/src/lodn_dispatcher_policy.d \
./src/lodn1.5_client/src/lodn_errno.d \
./src/lodn1.5_client/src/lodn_io.d \
./src/lodn1.5_client/src/lodn_url.d \
./src/lodn1.5_client/src/protocol_envelope.d 


# Each subdirectory must supply rules for building sources it contributes
src/lodn1.5_client/src/%.o: ../src/lodn1.5_client/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


