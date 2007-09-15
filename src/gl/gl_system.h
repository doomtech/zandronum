#ifndef __GL_SYSTEM
#define __GL_SYSTEM

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINDOWS 0x410
#define _WIN32_WINNT 0x0501			// Support the mouse wheel and session notification.
#define _WIN32_IE 0x0500

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <gl/glext.h>
#include <gl/wglext.h>

#if defined(_MSC_VER)
#pragma warning(disable:4018)
#endif
#endif

#endif
