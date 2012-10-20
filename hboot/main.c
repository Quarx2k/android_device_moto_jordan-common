#include "types.h"
#include "stdio.h"
#include "atag.h"
#include "common.h"
#include "memory.h"
#include "images.h"
#include "mbm.h"
#include "board.h"

#define ARCH_NUMBER 2241

void critical_error(const char* error)
{
	printf("Critical error %d\n", error);
	while (1);
}

void __attribute__((__naked__)) l2cache_enable(void)
{
	/* We have ES3.1 OMAP3430 on the phones at least, so we can set it directly */
	__asm__ volatile 
	(
		"stmfd sp!, {r4,lr}\n"
		"mrc p15, 0, r4, c1, c0, 1\n"
		"orr r4, r4, #0x2\n"
		"mcr p15, 0, r4, c1, c0, 1\n"
		"ldmfd sp!, {r4,pc}\n"
	);
}

void __attribute__((__naked__)) enter_kernel(int zero, int arch, void *atags, unsigned long kernel_address) 
{
	__asm__ volatile 
	(
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
		atags = atag_build();
		
		/* Reinit board */
		board_init();
		printf("Board initialized.\n");
		
		/* Disable Watchdog (seqeunce taken from MBM) */
		write32(0xAAAA, BOARD_WDTIMER2_BASE + 0x48);
		delay(500);
		
		write32(0x5555, BOARD_WDTIMER2_BASE + 0x48);
		delay(500);
		
		printf("Booting kernel.\n");
		l2cache_enable();
		enter_kernel(0, ARCH_NUMBER, atags, (unsigned long) image.data);
	}
	else 
		critical_error("No kernel image.");

  return 0;
}
