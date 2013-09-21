#ifndef __GL_LIGHTDATA
#define __GL_LIGHTDATA

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);
int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon);
PalEntry gl_CalcLightColor(int light, PalEntry pe, int blendfactor);
float gl_GetFogDensity(int lightlevel, PalEntry fogcolor);



#endif