# Makefile for pagespeak-btn out-of-tree kernel module
#
# Yocto usage: passes kernel path to make automatically via KERNEL_SRC
# variable when building the module as part of the Yocto build process.
#
# Manual cross-compile usage:
#   make KERNEL_SRC=/path/to/built/kernel
#
# Native test build (host kernel only, not for the target):
#   make KERNEL_SRC=/lib/modules/$(uname -r)/build

KERNEL_SRC ?= $(or ($(KERNEL_SRC)),/lib/modules/$(shell uname -r)/build)

# Declare the module object to build
obj-m := pagespeak-btn.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
