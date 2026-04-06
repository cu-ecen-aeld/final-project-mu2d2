# Makefile for pagespeak-btn out-of-tree kernel module
#
# Yocto usage: the module BitBake class invokes
#   make -C <KERNEL_SRC> M=<this dir> modules
# so KERNEL_SRC is provided by the build environment.
#
# Manual cross-compile usage:
#   make KERNEL_SRC=/path/to/built/kernel
#
# Native test build (host kernel only, not for the target):
#   make KERNEL_SRC=/lib/modules/$(uname -r)/build

# Fall back to the running host kernel if KERNEL_SRC is not set
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# Declare the module object to build
obj-m := pagespeak-btn.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
