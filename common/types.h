/*
 * ARM Privilege Test Framework - Basic Types
 */
#ifndef _TYPES_H_
#define _TYPES_H_

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;
typedef unsigned long       uintptr_t;
typedef long                intptr_t;
typedef unsigned long       size_t;
typedef long                ssize_t;

typedef _Bool               bool;
#define true                1
#define false               0

#define NULL                ((void *)0)

#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))
#define ALIGN_UP(x, a)      (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define BIT(n)              (1ULL << (n))

#endif /* _TYPES_H_ */
