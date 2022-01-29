release: all
	psp-fixup-imports -m 500mapfile.txt systemctrl.prx
	psp-packer -b systemctrl.prx

TARGET = systemctrl
OBJS = main.o memlmd.o loadexec.o sceDisplay_driver.o sceCtrl_driver.o

INCDIR = 
CFLAGS = -O2 -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division
ASFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBS=1
USE_KERNEL_LIBC=1

LIBDIR = 
LDFLAGS = -nostartfiles
LIBS = 


PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak


