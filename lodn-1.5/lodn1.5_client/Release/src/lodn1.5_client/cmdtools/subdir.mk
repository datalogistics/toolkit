################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/lodn1.5_client/cmdtools/get_status.c \
../src/lodn1.5_client/cmdtools/lodn_cp.c \
../src/lodn1.5_client/cmdtools/lodn_exportExnode.c \
../src/lodn1.5_client/cmdtools/lodn_importExnode.c \
../src/lodn1.5_client/cmdtools/lodn_ls.c \
../src/lodn1.5_client/cmdtools/lodn_mkdir.c \
../src/lodn1.5_client/cmdtools/lodn_rmdir.c \
../src/lodn1.5_client/cmdtools/lodn_stat.c \
../src/lodn1.5_client/cmdtools/lodn_unlink.c \
../src/lodn1.5_client/cmdtools/test_session.c 

OBJS += \
./src/lodn1.5_client/cmdtools/get_status.o \
./src/lodn1.5_client/cmdtools/lodn_cp.o \
./src/lodn1.5_client/cmdtools/lodn_exportExnode.o \
./src/lodn1.5_client/cmdtools/lodn_importExnode.o \
./src/lodn1.5_client/cmdtools/lodn_ls.o \
./src/lodn1.5_client/cmdtools/lodn_mkdir.o \
./src/lodn1.5_client/cmdtools/lodn_rmdir.o \
./src/lodn1.5_client/cmdtools/lodn_stat.o \
./src/lodn1.5_client/cmdtools/lodn_unlink.o \
./src/lodn1.5_client/cmdtools/test_session.o 

C_DEPS += \
./src/lodn1.5_client/cmdtools/get_status.d \
./src/lodn1.5_client/cmdtools/lodn_cp.d \
./src/lodn1.5_client/cmdtools/lodn_exportExnode.d \
./src/lodn1.5_client/cmdtools/lodn_importExnode.d \
./src/lodn1.5_client/cmdtools/lodn_ls.d \
./src/lodn1.5_client/cmdtools/lodn_mkdir.d \
./src/lodn1.5_client/cmdtools/lodn_rmdir.d \
./src/lodn1.5_client/cmdtools/lodn_stat.d \
./src/lodn1.5_client/cmdtools/lodn_unlink.d \
./src/lodn1.5_client/cmdtools/test_session.d 


# Each subdirectory must supply rules for building sources it contributes
src/lodn1.5_client/cmdtools/%.o: ../src/lodn1.5_client/cmdtools/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


