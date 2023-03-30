all: module bmp_read
obj-m += bmp_i2c.o

#KDIR=/lib/modules/$(shell uname -r)/build 
#Export KDIR path to your envirorment
bmp_read:
	arm-linux-gnueabihf-gcc bmp_read.c -o bmp_read
module:
	$(MAKE) -C $(KDIR) M=$(shell pwd) ARCH=arm CROSS_COMPILE=arm-poky-linux-gnueabi-

clean:clean_module clean_bmp_read

clean_module:
	$(MAKE) -C $(KDIR) M=$(shell pwd) clean
clean_bmp_read:
	rm -rf bmp_read
