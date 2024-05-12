CC=avr-gcc
CFLAGS=-g -Os -mmcu=atmega8
OBJCOPY=avr-objcopy

all: main.hex

main.elf: main.c
	$(CC) $(CFLAGS) -o $@ $^

main.hex: main.elf
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@

clean:
	-rm -f main.elf main.hex