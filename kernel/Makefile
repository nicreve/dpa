# Makefile for DPA kernel
# Author: <nicrevelee@gmail.com>
# Last Update:2014-11-09

MODULE_NAME = dpa

obj-m:=$(MODULE_NAME).o
$(MODULE_NAME)-objs:=dpa_main.o dpa_mem.o

BUILDDIR := /lib/modules/$(shell uname -r)/build
INSTDIR := /lib/modules/$(shell uname -r)/kernel/net/dpa

UDEVRULESFILE = /lib/udev/rules.d/99-dpa.rules

default:
	$(MAKE) -C $(BUILDDIR)  M=$(PWD) modules


install:
	install -D -m 644 $(MODULE_NAME).ko $(INSTDIR)/$(MODULE_NAME).ko
ifeq ($(wildcard $(UDEVRULESFILE)),)
	@echo "create $(UDEVRULESFILE)"
	@echo "KERNEL==\"dpa\", MODE=\"0666\"" > $(UDEVRULESFILE)
endif
ifeq (,$(DESTDIR))
	/sbin/depmod -a || true
endif


uninstall:
ifneq ($(wildcard $(INSTDIR)/$(MODULE_NAME).ko),)
	rm -f $(INSTDIR)/$(MODULE_NAME).ko;
endif
ifneq ($(wildcard $(UDEVRULESFILE)),)
	rm -f $(UDEVRULESFILE)
endif
	/sbin/depmod -a
	
	
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko *.ko.unsigned .*cmd .tmp_versions Module.markers Module.symvers modules.order