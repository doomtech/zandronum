
#ifndef __GL_GLOW
#define __GL_GLOW

#include "gl/gl_renderstruct.h"

#pragma warning(disable:4018)

#ifndef NO_GLOW

void gl_InitGlow(const char * lumpnm);
bool gl_isGlowingTexture(unsigned int texno);
void gl_CheckGlowing(GLWall * wall);
int gl_CheckSpriteGlow(int floorpic, int lightlevel, fixed_t floordiff);

#else

inline void gl_InitGlow(const char * lumpnm) {}
inline bool gl_isGlowingTexture(unsigned int texno) { return false; }
inline void gl_CheckGlowing(GLWall * wall) {}
inline int gl_CheckSpriteGlow(int floorpic, int lightlevel, fixed_t floordiff) { return lightlevel; }

#endif


#endif
