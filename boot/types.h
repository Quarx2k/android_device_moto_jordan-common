#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* Integer that can hold any pointer */
typedef uint32_t addr_t;

/* Integer that can hold any ARM instruction */
typedef uint32_t arm_insn_t;

/* Integer that can hold any Thumb instruction */
typedef uint16_t thumb_insn_t;

typedef uint32_t size_t;
typedef int32_t ssize_t;

typedef int bool;

#endif // __TYPES_H__
