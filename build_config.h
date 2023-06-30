#if defined(_WIN32)
#define OS_WIN 1
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#endif

#if defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86 1
#endif