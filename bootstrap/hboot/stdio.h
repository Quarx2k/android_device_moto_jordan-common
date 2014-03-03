#ifndef __STDIO_H__
#define __STDIO_H__
#include "stdarg.h"

#define EOF ((int)-1)

int putchar(int c);
int puts(const char *s);
int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *ftm, ...);
int vprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...);

#endif // __STDIO_H__
