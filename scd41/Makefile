KDIR = /lib/modules/`uname -r`/build

obj-m := scd41.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean