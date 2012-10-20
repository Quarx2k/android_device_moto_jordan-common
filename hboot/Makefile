#
# Makefile for 2nd-boot
#

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

EXTRA_CFLAGS ?= -DBOARD_UMTS_JORDAN .
CFLAGS := -Os -Wall -Wno-return-type -Wno-main -fno-builtin -fno-stack-protector -march=armv7-a -ffunction-sections -fPIC $(EXTRA_CFLAGS)
AFLAGS := -D__ASSEMBLY__ -fno-builtin -march=armv7-a -ffunction-sections
LDFLAGS := -static -nostdlib --gc-sections 

LIBGCC := -L $(shell dirname `$(CC) $(CFLAGS) -print-libgcc-file-name`) -lgcc

O ?= .

ELF_TARGET := hboot.elf
RAW_TARGET := hboot.bin

VERSION := $(shell hg -q id 2>/dev/null || echo unknown)
CFLAGS += -DLDR_VERSION=\"$(VERSION)\"

A_OBJS := $(O)/init.o $(O)/mbm.o
C_OBJS := $(O)/main.o $(O)/board.o $(O)/images.o $(O)/common.o $(O)/vsprintf.o $(O)/printf.o $(O)/memstr.o $(O)/atag.o $(O)/crc32.o
LIB_OBJS := $(O)/lib/_ashldi3.o $(O)/lib/_ashrdi3.o  $(O)/lib/_div0.o $(O)/lib/_divsi3.o $(O)/lib/_lshrdi3.o $(O)/lib/_modsi3.o  $(O)/lib/_udivsi3.o $(O)/lib/_umodsi3.o  
OBJS := $(A_OBJS) $(C_OBJS) $(LIB_OBJS)

# Attempt to create a output directory.
$(shell [ -d ${O} ] || mkdir -p ${O})

# Verify if it was successful.
OUTPUT_DIR := $(shell cd $(O) && /bin/pwd)
$(if $(OUTPUT_DIR),,$(error output directory "$(O)" does not exist))

#Attempt to create a lib subdirectory
$(shell [ -d ${O}/lib ] || mkdir -p ${O}/lib)

# Verify if it was successful.
OUTPUT_DIR := $(shell cd $(O)/lib && /bin/pwd)
$(if $(OUTPUT_DIR),,$(error output directory "$(O)/lib" does not exist))

all: $(O)/$(RAW_TARGET)

$(O)/%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o : %.S
	$(CC) $(AFLAGS) -c $< -o $@

$(O)/$(RAW_TARGET): $(O)/$(ELF_TARGET)
	$(OBJCOPY) -O binary -R .note -R .comment -S $< $@

$(O)/$(ELF_TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -T ld-script -o $@ $(OBJS)

.PHONY: prep

clean:
	rm -f $(OBJS) $(O)/$(ELF_TARGET) $(O)/$(RAW_TARGET)
