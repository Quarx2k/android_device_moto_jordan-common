#include "types.h"
#include "stdio.h"
#include "error.h"
#include "atag.h"
#include "common.h"
#include "memory.h"
#include "images.h"
#include "mbm.h"

#define ARCH_NUMBER 2241

/* OMAP Registers */

#define WDTIMER2_BASE						0x48314000

void critical_error(error_t err)
{
	printf("Critical error %d\n", (int)err);
	while (1);
}

void __attribute__((__naked__)) enter_kernel(int zero, int arch, void *atags, int kern_addr) 
{
	__asm__ volatile (
		"bx r3\n"
	);
}

int main() 
{
	void* atags;
	struct memory_image image;
	
	printf("=== HBOOT START ===\n");

	/* Complete images */
	image_complete();
	
	if (image_find(IMG_LINUX, &image) != NULL)
	{
		printf("KERNEL FOUND!\n");
		atags = atag_build();
		
		/* Disable Watchdog (seqeunce taken from MBM) */
		write32(0xAAAA, WDTIMER2_BASE + 0x48);
		delay(500);
		
		write32(0x5555, WDTIMER2_BASE + 0x48);
		delay(500);
		
		printf("Watchdog disabled.\n");
		
		printf("BOOTING KERNEL!\n");
		enter_kernel(0, ARCH_NUMBER, atags, KERNEL_DEST);
	}
	else 
		critical_error(IMG_NOT_PROVIDED);

  return 0;
}
