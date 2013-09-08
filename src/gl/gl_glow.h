
#ifndef __GL_GLOW
#define __GL_GLOW

#include "gl/gl_renderstruct.h"

#ifdef _MSC_VER
#pragma warning(disable:4018)
#endif

void gl_InitGlow(const char * lumpnm);
int gl_CheckSpriteGlow(FTextureID floorpic, int lightlevel, fixed_t floordiff);

#endif
