

v101: refresh is added
      (1) refresh is added;
	  (2) kernel is used bb-kernel-3.8.13-bone79
	  (3) HW platform is beaglebone black

v100: make sure it works for beaglebone system / Linux 4.5.4-bone4
      (Reset Key)
          sh-keygen -f "/home/chm/.ssh/known_hosts" -R 192.168.7.2
      (COPY)
	  scp duraTOUCHlinux.ko debian@192.168.7.2:/home/debian/work/.
      (Debug)
	  putty &
	  putty
	  ssh debian@192.168.7.2


Now move to uswork/driver fodler and add refresh code into the driver
======================================================================================
Move code to Beagle's bb-kernel, make it into driver/input/touchscreen dolder

Move code to Raspberry / driver and changed for something


v009: Driver for Linux is working
      (1) Verify it again;
      (2) Add a tool for copying ko file
      (3) Linux working folder is lcoated @ /root/appdev 

v008: Make it work on TQ210
      (1) We can see the touch data report to android system;
      (2) With UICO app(APK), we can see the toucnh track (frist time);
      (3) Interrupt PIN should be changed for new platform;
      (4) Sometimes, we can find the system is hang, (just long delay actually)
      (5) When track is appeared, if we use mouse to clear the track, track will not be appeared any more(BUG!)
      (6) duraTOUCH.h is gone, which is porting to duraTOUCH.c now
      (7) Three command to make different Driver
          make P=BBB-LINUX
          make P=BBB-ANDROID
          make P=TQ210-ANDROID

v007: Try to make the code working for android and linux at the same time
      (1) Change Makefile for the propose
      (2) make P=BBB-LINUX, or make P=BBB-ANDROID is the correct command;
      (3) make is not working, make clean is working
      (4) "cat /dev/input/event2 | hexdump" or "evtest" could be used to verify the driver now. source code for evtest could be found in internet.
      (5) evtest etc should be run in linux, /root/appdev is good location.

v006: Try to add input system
      (1) add INPUT system, allocate and register
      (2) add suspend and resume, but it is no enabled in LINUX enviroment;
      (3) dT Testing for input system is created, but it is fail

v005: Try to add i2c new device into the system
      (1) in Beaglebone black system: echo "duraTOUCHic" 0x48 > /sys/bus/i2c/devices/i2c-1/new_device
      (2) remove i2c_register_board_info function in the init function, it is unnecessary
      (3) i2c_probe will be called now
      (4) work_queue has to be added in the interrupt code, otherwise, it is not working

v004: I2C function will be inserted, ISR has added the I2C functions;
      (1) I2C probe function is now called, since the device name is not matched;
      (2) File operations array could be removed; since it is useless for touch IC driver

V003: Open, Release, Read and Write (functions) are added, we can test it by reading back the interrupting info
      (1) File operations array is added; it works;

v002: Interrupt code in the driver is ready
      (1) The ISR of GPIO (60) interrupt is working

v001: It is come from Derek code originally

