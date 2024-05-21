obj-m += simple_proc.o

#KERNELDIR := /usr/src/linux-5.7
KERNELDIR += /lib/modules/$(shell uname -r)/build 
PWD += $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf *~
