CC = avr-gcc
CFLAGS = -g -Os -mmcu=atmega8
OBJCOPY = avr-objcopy
SIZE = avr-size
BUILD_DIR = build

all: $(BUILD_DIR)/main.hex size

$(BUILD_DIR)/main.elf: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/main.hex: $(BUILD_DIR)/main.elf | $(BUILD_DIR)
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@

size: $(BUILD_DIR)/main.elf
	$(SIZE) -C --mcu=atmega8 $^

clean:
	-rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@