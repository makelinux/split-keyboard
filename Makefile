# ccflags-y += -DDEBUG
obj-m += split-keyboard.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KERNELDIR) M=$$PWD modules
	sudo rmmod split_keyboard
	sudo insmod ./split-keyboard.ko

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$$PWD modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c *.mod \
		.tmp_versions modules.order Module.symvers .cache.mk \
		*.tmp *.log
