#
# Makefile for 2nd-boot
#

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

CFLAGS := -Os -Wall -Wno-return-type -Wno-main -fno-builtin -fno-stack-protector -march=armv7-a -D__LOADER__ -ffunction-sections -fPIC 
AFLAGS := -D__ASSEMBLY__ -fno-builtin -march=armv7-a -D__LOADER__ -ffunction-sections
LDFLAGS := -static -nostdlib --gc-sections 

O ?= .

ELF_TARGET := hboot.elf
RAW_TARGET := hboot.bin

VERSION := $(shell hg -q id 2>/dev/null || echo unknown)
CFLAGS += -DLDR_VERSION=\"$(VERSION)\"

A_OBJS := $(O)/init.o $(O)/mbm.o
C_OBJS := $(O)/main.o $(O)/images.o $(O)/console.o $(O)/vsprintf.o $(O)/printf.o $(O)/memstr.o $(O)/atag.o $(O)/crc32.o
OBJS := $(A_OBJS) $(C_OBJS)

$(O)/%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o : %.S
	$(CC) $(AFLAGS) -c $< -o $@

$(O)/$(RAW_TARGET): $(O)/$(ELF_TARGET)
	$(OBJCOPY) -O binary -R .note -R .comment -S $< $@

$(O)/$(ELF_TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -T ld-script -o $@ $(OBJS)

.PHONY: clean

all: $(O)/$(RAW_TARGET)

clean:
	rm -f $(OBJS) $(O)/$(ELF_TARGET) $(O)/$(RAW_TARGET)




