
obj-m += bmp_spi.o

#KDIR=/lib/modules/$(shell uname -r)/build 

all:
	$(MAKE) -C $(KDIR) M=$(shell pwd) ARCH=arm CROSS_COMPILE=arm-poky-linux-gnueabi-

clean:
	$(MAKE) -C $(KDIR) M=$(shell pwd) clean
