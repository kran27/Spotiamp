#ifndef TINYSTREAMER_TYPES_H_
#define TINYSTREAMER_TYPES_H_

#include "build_config.h"
#include "config.h"
#include <stddef.h>
#if !(defined(COMPILER_MSVC) && defined(OS_WIN))
#include <stdint.h>
#include <stdbool.h>
#endif  // !(defined(COMPILER_MSVC) && defined(OS_WIN))
#include <assert.h>

// MSVC does not have the stdint.h types
#if defined(COMPILER_MSVC) && defined(OS_WIN)
typedef unsigned __int8 uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
typedef __int8 int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;
#else  // Default to using the stdint.h types
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
#endif

typedef unsigned int uint;

typedef uint8 byte;

#if (defined(COMPILER_MSVC) || defined(COMPILER_TCC) || defined(COMPILER_RVCT)) && !defined(__cplusplus)
#ifndef inline
#define inline __inline
#endif
#endif

#define forceinline TSP_INLINE
#define likely TSP_LIKELY
#define unlikely TSP_UNLIKELY

#if !defined(COMPILER_MSVC)
#define _cdecl 
#endif

// MSVC does not have the stdbool header.
#if defined(COMPILER_MSVC) && !defined(__cplusplus)
typedef unsigned char bool;
#define false 0
#define true 1
#endif   // defined(COMPILER_MSVC)

#define CTASTR2(pre,post) pre ## post
#define CTASTR(pre,post) CTASTR2(pre,post)
#define STATIC_ASSERT(cond,msg) \
    typedef struct { int CTASTR(static_assertion_failed_,msg) : !!(cond); } \
        CTASTR(static_assertion_failed_x_,msg) TSP_UNUSED

#if ARCH_CPU_NEED_64BIT_ALIGN
#define CHECK_POINTER_ALIGNMENT(x) assert(((intptr_t)(x) & 7) == 0)
#else
#define CHECK_POINTER_ALIGNMENT(x) assert(((intptr_t)(x) & 3) == 0)
#endif

#endif  // TINYSTREAMER_TYPES_H_
