#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"
typedef Win32GLFrameBuffer OpenGLFrameBuffer;
#else
#include "sdlglvideo.h"
typedef SDLGLFB OpenGLFrameBuffer;
#endif

#endif //__GL_FRAMEBUFFER
