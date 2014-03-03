#include "string.h"

void *memcpy(void *dest, const void *src, size_t n) 
{
	char *d = (char*)dest;
	const char *s = (const char*)src;

	while (n-- > 0)
		*d++ = *s++;

	return dest;
}

void *memset(void *dest, int fill, size_t n) 
{
	unsigned char *d = (unsigned char*)dest;

	while (n-- > 0)
		*d++ = (unsigned char)fill;
	
	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) 
{
	unsigned char *ss1 = (unsigned char*)s1;
	unsigned char *ss2 = (unsigned char*)s2;

	while (n-- > 0) 
	{
		if (*ss1 != *ss2)
			return *ss1 - *ss2;
		
		++ss1;
		++ss2;
	}
	return 0;
}

size_t strlen(const char *s) 
{
	size_t n = 0;

	while (*s++)
		n++;
	
	return n;
}

char *strcpy(char *d, const char *s) 
{
	char *dest = d;

	while (*d++ = *s++);
	
	return dest;
}

int strcmp(const char *s1, const char *s2) 
{
	size_t l1, l2;
	l1 = strlen(s1);
	l2 = strlen(s2);
	if (l1 < l2) 
		return -1;
	
	if (l1 > l2) 
		return 1;
	
	while (*s1) 
	{
		if (*s1 != *s2) 
			return *s1 - *s2;
		
		++s1;
		++s2;
	}
	
	return 0;
}
