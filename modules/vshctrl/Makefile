release: all
	psp-packer vshctrl.prx

TARGET = vshctrl
OBJS = main.o virtpbp.o exports.o imports.o

INCDIR = 
CFLAGS = -O2 -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division
ASFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBS=1
USE_KERNEL_LIBC=1

LIBDIR = 
LDFLAGS = -mno-crt0 -nostartfiles
LIBS = -lpspsystemctrl_kernel -lpspusbdevice_driver -lpspusb_driver -lpsppower_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak


