#ifndef TINY_SPOTIFY_FPU_UTIL_H_
#define TINY_SPOTIFY_FPU_UTIL_H_
#include "build_config.h"
#include <math.h>

#if defined(OS_WIN) && defined(ARCH_CPU_X86)
float _cdecl Pow(float base, float exp);
#else  // defined(OS_WIN) && defined(ARCH_CPU_X86)
static inline float Pow(float base, float exp) { return powf(base, exp); }
#endif  // defined(OS_WIN) && defined(ARCH_CPU_X86)

#endif  // TINY_SPOTIFY_FPU_UTIL_H_