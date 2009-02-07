
#ifndef __GL_GLOW
#define __GL_GLOW

#include "gl/gl_renderstruct.h"

#ifdef _MSC_VER
#pragma warning(disable:4018)
#endif

#ifndef NO_GLOW

void gl_InitGlow(const char * lumpnm);
void gl_CheckGlowing(GLWall * wall);
int gl_CheckSpriteGlow(FTextureID floorpic, int lightlevel, fixed_t floordiff);

#else

inline void gl_InitGlow(const char * lumpnm) {}
inline void gl_CheckGlowing(GLWall * wall) {}
inline int gl_CheckSpriteGlow(FTextureID floorpic, int lightlevel, fixed_t floordiff) { return lightlevel; }

#endif


#endif
