CC = aarch64-none-linux-gnu-gcc
C++ = aarch64-none-linux-gnu-g++
AS = aarch64-none-linux-gnu-as
CFLAGS = -O3 -fopenmp

TARGET = cachetestbench
OBJS = main.o memcpy-arm64.o routines-arm-64bit.o matrix-multiply.o save2file.o latency.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(C++) $(CFLAGS) -o $@ $(OBJS) -lpthread -lrt -lm

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

memcpy-arm64.o: memcpy-arm64.S
	$(CC) $(CFLAGS) -c -o $@ $<

routines-arm-64bit.o: routines-arm-64bit.asm
	$(AS) -march=armv8-a -c -o $@ $<

draw.o: draw.c
	$(CC) $(CFLAGS) -c -o $@ $<

save2file.o: save2file.c
	$(CC) $(CFLAGS) -c -o $@ $<

latency.o: latency.c
	$(CC) $(CFLAGS) -c -o $@ $<

matrix-multiply.o: matrix-multiply.cpp
	$(C++) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET)
