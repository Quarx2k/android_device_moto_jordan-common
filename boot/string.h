#ifndef __STRING_H__
#define __STRIN_H__

#include "types.h"

void *memcpy(void *d, const void *s, size_t);
void *memset(void *d, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
char *strcpy(char *d, const char *s);
int strcmp(const char *s1, const char *s2);

#endif // __STRING_H__
