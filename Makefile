CC = aarch64-none-linux-gnu-gcc
C++ = aarch64-none-linux-gnu-g++
AS = aarch64-none-linux-gnu-as
CFLAGS = -O3 -Wall -fopenmp -static

cachetestbench: main.o memcpy-arm64.o draw.o routines-arm-64bit.o
	$(C++) $(CFLAGS) -o cachetestbench main.o memcpy-arm64.o routines-arm-64bit.o draw.o

main.o : main.c
	$(CC) $(CFLAGS) -c main.c

memcpy-arm64.o : memcpy-arm64.S
	$(CC) $(CFLAGS) -c memcpy-arm64.S

routines-arm-64bit.o : routines-arm-64bit.asm
	$(AS) -march=armv8-a -c routines-arm-64bit.asm -o routines-arm-64bit.o

draw.o : draw.c
	$(CC) $(CFLAGS) -c draw.c

.PHONY: clean
clean:
	rm -f *.o cachetestbench
