#EXTRA_CFLAGS += -march=armv7-a -mtune=cortex-a8 -D__PLAT_TI_OMAP3430__ -Wall 
EXTRA_CFLAGS += -DEMU_UART3

obj-m += 2ndboot.o
2ndboot-objs := hboot.o buffers.o crc32.o cache-v7.o board-mapphone-emu_uart.o 
