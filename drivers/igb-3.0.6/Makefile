# Makefile for Intel igb driver 3.0.6-DPA
# Author: <nicrevelee@gmail.com>
# Last Update:2014-04-29

MODULE_NAME = igb
MODULE_VER_SUFFIX = -DPA

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := igb_main.o igb_ethtool.o e1000_82575.o \
	    e1000_mac.o e1000_nvm.o e1000_phy.o e1000_mbx.o

BUILDDIR:=/lib/modules/$(shell uname -r)/build
INSTDIR:=/lib/modules/$(shell uname -r)/kernel/drivers/net/igb

KBUILD_EXTRA_SYMBOLS += $(shell dirname $$(dirname $$(pwd)))/kernel/Module.symvers
export KBUILD_EXTRA_SYMBOLS


EXTRA_CFLAGS += -DDPA_ENABLE

default:
	$(MAKE) -C $(BUILDDIR)  M=$(PWD) modules
	
	
install:
#备份原始驱动
ifneq ($(wildcard $(INSTDIR)/$(MODULE_NAME).ko),)
ifneq ($(shell modinfo -F version $(INSTDIR)/$(MODULE_NAME).ko)),)
ifeq ($(filter %$(MODULE_VER_SUFFIX),$(shell modinfo -F version $(INSTDIR)/$(MODULE_NAME).ko)),)
	cp $(INSTDIR)/$(MODULE_NAME).ko $(INSTDIR)/$(MODULE_NAME).ko.bak
endif
endif
endif
	install -D -m 644 $(MODULE_NAME).ko $(INSTDIR)/$(MODULE_NAME).ko
ifeq (,$(DESTDIR))
	/sbin/depmod -a || true
endif


uninstall:
ifneq ($(wildcard $(INSTDIR)/$(MODULE_NAME).ko),)
	rm -f $(INSTDIR)/$(MODULE_NAME).ko;
endif
#恢复原始驱动
ifneq ($(wildcard $(INSTDIR)/$(MODULE_NAME).ko.bak),)
	mv $(INSTDIR)/$(MODULE_NAME).ko.bak $(INSTDIR)/$(MODULE_NAME).ko;
endif
	/sbin/depmod -a
	
	
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko *.ko.unsigned .*cmd .tmp_versions Module.markers Module.symvers modules.order
