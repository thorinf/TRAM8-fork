CC = avr-gcc
CFLAGS = -g -Os -mmcu=atmega8
OBJCOPY = avr-objcopy
SIZE = avr-size

all: main.hex size

main.elf: main.c
	$(CC) $(CFLAGS) -o $@ $^

main.hex: main.elf
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@

size: main.elf
	$(SIZE) -C --mcu=atmega8 main.elf

clean:
	-rm -f main.elf main.hex
