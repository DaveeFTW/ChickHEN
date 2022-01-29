PSPSDK=$(shell psp-config --pspsdk-path)
PSPDEV=$(shell psp-config --pspdev-path)
INCLUDE=$(PSPSDK)/include
INCDIR =
all:    main

clean:
	rm -rf *~ *.o *.elf *.bin *.s *.bin

CC       = psp-gcc
CFLAGS   := -D PSP -I $(INCLUDE) -W -Wall -O2 -G0 -fno-pic -mno-abicalls -w -fomit-frame-pointer

ASM      = psp-as

main.o: main.c
	$(PSPDEV)/bin/bin2c ../modules/rebootex/systemex.bin systemex.h systemex
	$(CC) $(CFLAGS) -S main.c -o main.s
	$(ASM) main.s -o main.o
	
loop.o: loop.s
	$(ASM) loop.S -o loop.o
#k1set.o: k1set.b
#	$(ASM) k1set.b -o k1set.o

#main: main.o k1set.o linker.x
#	$(PSPDEV)/bin/psp-ld -T linker.x -L$(PSPSDK)/lib main.o k1set.o -o main.elf
main: main.o loop.o linker.x
	$(PSPDEV)/bin/psp-ld -T linker.x -L$(PSPSDK)/lib main.o loop.o -o main.elf
	$(PSPDEV)/bin/psp-strip -s main.elf
	$(PSPDEV)/bin/psp-objcopy -O binary main.elf h.bin
