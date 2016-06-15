#******************************************************************
# 2016-05-02
# UICO Sunnyvale Office
# Qiuliang Fu
# dT101, dT201, dT401
#******************************************************************

#---------------------------------------------
# Beaglebone Black , Linux 3.8 for "clean"
#---------------------------------------------
KDIR3P8 := /opt/beaglebone/sourceCode/bb-kernel-3.8.13-bone79/KERNEL/
KDIR4P5 := /opt/beaglebone/sourceCode/bb-kernel-4.5.4-bone4/KERNEL/
CUR_KERNEL_DIR=$(KDIR3P8)
TARGET_NAME := duraTOUCH


#------------------------------------------------------------
# Dragon Board 410C, Qualcomm 
#        Android: Source Code
#        Kernel:  XXXX
#------------------------------------------------------------
ifeq ("$(P)", "DB410C-ANDROID")
#KERNEL_DIR=/opt/dragon410c/sourceCode/out/target/product/msm8916_64/obj/KERNEL_OBJ
#KERNEL_DIR=/opt/beaglebone/sourceCode/BBBAndroid/kernel/
#KERNEL_DIR=/opt/dragon410c/sourceCode-NonAndroid/linux/kernel/
KERNEL_DIR=/opt/dragon410c/sourceCode-NonAndroid/linux.git/

#CROSS_COMPILE=/opt/dragon410c/sourceCode/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
#CROSS_COMPILE=/opt/dragon410c/sourceCode/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
#CROSS_COMPILE=/opt/dragon410c/sourceCode/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.8/bin/arm-linux-androideabi-
#CROSS_COMPILE=/opt/dragon410c/sourceCode/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

#CROSS_COMPILE=/opt/dragon410c/toolChain/gcc-linaro-4.9-2014.11-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
CROSS_COMPILE=/opt/dragon410c/toolChain/gcc-linaro-4.9-2016.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-

#CROSS_COMPILE=/opt/beaglebone/toolChain/gcc-linaro-4.9-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

EXTRA_CFLAGS+=-DANDROID_FUNC -DDB410C 
DRIVERCMD = mv duraTOUCH.ko duraTOUCHdb410c.ko
MAKEAPPCMD = 
endif

#---------------------------------------------
# Raspberry Pi 3(2016), Linux 4.4
#---------------------------------------------
ifeq ("$(P)", "RASPI-LINUX")
KERNEL_DIR=/opt/optWork/raspberry/linux/
CROSS_COMPILE=/opt/optWork/raspberry/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-
EXTRA_CFLAGS+=-DRASPI
DRIVERCMD = mv duraTOUCH.ko duraTOUCHraspi.ko
MAKEAPPCMD = 
endif

#---------------------------------------------
# TQ210, Samsung 6410, Linux 3.0.8
#---------------------------------------------
ifeq ("$(P)", "TQ210-ANDROID")
KERNEL_DIR=/opt/CN1100/Kernels/Samsung/TQ210/Kernel-3.0.8
CROSS_COMPILE=/opt/CN1100/Kernels/Samsung/TQ210/Toolchain-3.0.8-TQ210/bin/arm-linux-
EXTRA_CFLAGS+=-DANDROID_FUNC -DTQ210
DRIVERCMD = mv duraTOUCH.ko duraTOUCHtq210.ko
MAKEAPPCMD = 
endif

#---------------------------------------------
# Beaglebone Black , Linux 4.5.4, pSoC1
#---------------------------------------------
ifeq ("$(P)", "BBB-LINUX-PSOC1")
#KERNEL_DIR=$(KDIR3P8)
KERNEL_DIR=$(CUR_KERNEL_DIR)
CROSS_COMPILE=/opt/beaglebone/toolChain/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
EXTRA_CFLAGS+=-DBBB -DPSOC1_I2C -DTWO_FINGERS
DRIVERCMD = mv duraTOUCH.ko duraTOUCHlinux.ko
MAKEAPPCMD = #${CC}gcc -lpthread dT_test.c -o dT_test
endif

#---------------------------------------------
# Beaglebone Black , Linux 4.5.4
#---------------------------------------------
ifeq ("$(P)", "BBB-LINUX")
KERNEL_DIR=$(KDIR3P8)
#CROSS_COMPILE=/opt/beaglebone/toolChain/gcc-linaro-4.9-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
CROSS_COMPILE=/opt/beaglebone/toolChain/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
EXTRA_CFLAGS+=-DBBB
DRIVERCMD = mv duraTOUCH.ko duraTOUCHlinux.ko
MAKEAPPCMD = ${CROSS_COMPILE}gcc fwReflash.c -o fwReflash
endif

#--------------------------------------------------------------------
# Beaglebone Black , 
#     BBB-ANDROID (4.2.2, Kernel: 3.8.13)
#     Compiler:   gcc-linaro-4.9-2016.02-x86_64_arm-linux-gnueabihf
#--------------------------------------------------------------------
ifeq ("$(P)", "BBB-ANDROID")
KERNEL_DIR=/opt/beaglebone/sourceCode/BBBAndroid/kernel/
#CROSS_COMPILE=/opt/beaglebone/toolChain/gcc-linaro-4.8-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
CROSS_COMPILE=/opt/beaglebone/toolChain/gcc-linaro-4.9-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
AND_CROSS_COMPILE=/opt/beaglebone/sourceCode/BBBAndroid/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.7/bin/arm-linux-androideabi-
EXTRA_CFLAGS+=-DANDROID_FUNC -DBBB
DRIVERCMD = mv duraTOUCH.ko duraTOUCHandroid.ko
MAKEAPPCMD = ${CROSS_COMPILE}gcc fwReflash.c -o fwReflash
endif

ifneq ($(KERNELRELEASE),)
obj-m += $(TARGET_NAME).o
else

all:clean modules testApp

modules:clean
	KCPPFLAGS="$(EXTRA_CFLAGS)" make -C $(KERNEL_DIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
	$(DRIVERCMD)

testApp:
	$(MAKEAPPCMD)

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean 
#make -C $(CUR_KERNEL_DIR) M=$(PWD) clean 
endif

