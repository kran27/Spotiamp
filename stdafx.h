// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

//#define WITH_SDL

#if !defined(WITH_SDL)
#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <Windowsx.h>
#endif

#define VERSION_STR "0.2.1"

#pragma warning(disable: 4530)
// TODO: reference additional headers your program requires here
