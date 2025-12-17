CC = aarch64-none-linux-gnu-gcc
C++ = aarch64-none-linux-gnu-g++
AS = aarch64-none-linux-gnu-as
CFLAGS = -O3 -march=armv8.3-a+simd -fopenmp -static -g

cachetestbench: main.o memcpy-arm64.o routines-arm-64bit.o matrix-multiply.o save2file.o latency.o
	$(C++) $(CFLAGS) -o cachetestbench main.o memcpy-arm64.o routines-arm-64bit.o matrix-multiply.o save2file.o latency.o -lpthread -lrt -lm

main.o : main.c
	$(CC) $(CFLAGS) -c main.c

memcpy-arm64.o : memcpy-arm64.S
	$(CC) $(CFLAGS) -c memcpy-arm64.S

routines-arm-64bit.o : routines-arm-64bit.asm
	$(AS) -march=armv8-a -c routines-arm-64bit.asm -o routines-arm-64bit.o

matrix-multiply.o : matrix-multiply.cpp
	$(C++) $(CFLAGS) -c matrix-multiply.cpp

draw.o : draw.c
	$(CC) $(CFLAGS) -c draw.c

save2file.o : save2file.c
	$(CC) $(CFLAGS) -c save2file.c

latency.o : latency.c
	$(CC) $(CFLAGS) -c latency.c

.PHONY: clean
clean:
	rm -f *.o cachetestbench
