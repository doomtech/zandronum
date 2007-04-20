#ifdef _WIN32
//#define __RPCNDR_H__		// this header causes problems!
//#define __wtypes_h__
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINDOWS 0x410
#define _WIN32_WINNT 0x0501			// Support the mouse wheel and session notification.
#define _WIN32_IE 0x0500
#define DIRECTINPUT_VERSION 0x800
#define DIRECTDRAW_VERSION 0x0300

#define DWORD WINDOWS_DWORD	// I don't want to depend on this throughout the GL code!

#include <windows.h>
#include <mmsystem.h>
#include <winsock.h>
#include <dshow.h>
#include <d3d9.h>
#include <dsound.h>
#include <dinput.h>
#include <lmcons.h>
#include <shlobj.h>
#endif

#undef DWORD
#ifndef CALLBACK
#define CALLBACK
#endif
#include <math.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <direct.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <malloc.h>
#include <time.h>

#ifdef _MSC_VER
#define    F_OK    0    /* Check for file existence */
#define    W_OK    2    /* Check for write permission */
#define    R_OK    4    /* Check for read permission */
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gl/gl.h>
#include <gl/glu.h>
#include <gl/glext.h>
#ifdef _WIN32
#define DWORD WINDOWS_DWORD	// I don't want to depend on this throughout the GL code!
#include <gl/wglext.h>
#undef DWORD
#endif
#include "r_render/r_render.h"

extern RenderContext gl;

#ifdef LoadMenu
#undef LoadMenu
#endif
#ifdef DrawText
#undef DrawText
#endif
#ifdef GetCharWidth
#undef GetCharWidth
#endif

#undef S_NORMAL
#undef OPAQUE


#ifdef _MSC_VER
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)     // truncate from double to float
#endif

#ifdef WIN32
#undef WIN32
#endif
