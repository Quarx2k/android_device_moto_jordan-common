#ifndef HBOOT_H
#define HBOOT_H

#include "buffers.h"

#include <linux/types.h>

uint32_t get_high_mem(size_t size);
void free_high_mem(void *p);
uint32_t get_high_pages(unsigned int order);
void free_high_pages(void *p, unsigned int order);
uint32_t get_high_page(void);
void free_high_page(void *p);
int allocate_buffer(uint8_t tag, uint8_t type, uint8_t attrs, uint32_t size, uint32_t rest);
int free_buffer(int handle);
int select_buffer(int handle);
int buffer_append_userdata(const char __user *data, size_t len, loff_t *ppos);
int buffers_init(void);
void buffers_destroy(void);

typedef void (*bootfunc_t)(void *, uint32_t);
bootfunc_t get_bootentry(uint32_t *bootsize, int handle);
void *get_bootlist(uint32_t *listsize, int handle);

struct hboot_buffer_req 
{
	uint8_t tag;
	uint8_t type;
	uint8_t attrs;
	uint32_t size;
	uint32_t rest;
};

#define MAX_BUFFERS_COUNT 9

#define INVALID_BUFFER_HANDLE		((int)-1)
#define HBOOT_ALLOCATE_BUFFER		0
#define HBOOT_FREE_BUFFER				1
#define HBOOT_SELECT_BUFFER			4
#define HBOOT_BOOT							3
#endif

