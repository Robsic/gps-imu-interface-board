TOOLCHAIN ?= arm-none-eabi-
CC      := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE    := $(TOOLCHAIN)size

TARGET  := openimu300zi_emulator
BUILD   := build
SOURCES := src/main.c ../common/startup_stm32f103.s
OBJECTS := $(BUILD)/main.o $(BUILD)/startup_stm32f103.o

CFLAGS  := -mcpu=cortex-m3 -mthumb -std=c11 -Os -g3 \
           -ffreestanding -fdata-sections -ffunction-sections \
           -Wall -Wextra -Werror
LDFLAGS := -mcpu=cortex-m3 -mthumb -nostdlib -T ../common/linker.ld \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

.PHONY: all clean

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: src/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup_stm32f103.o: ../common/startup_stm32f103.s | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJECTS) ../common/linker.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -lgcc -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD)
