KDIR = /lib/modules/`uname -r`/build

obj-m := gpio17.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean