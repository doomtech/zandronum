
#ifndef __GL_DATA_H
#define __GL_DATA_H


struct GLRenderSettings
{

	SBYTE lightmode;
	bool nocoloredspritelighting;

	SBYTE map_lightmode;
	SBYTE map_nocoloredspritelighting;

	FVector3 skyrotatevector;

};

extern GLRenderSettings glset;

#include "r_defs.h"

struct sector_t;
struct line_t;
class FTexture;

void gl_FreeSpecialTextures();
void gl_InitSpecialTextures();
void gl_PreprocessLevel();
void gl_CleanLevelData();
FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t ang, bool *mirror);

struct MapData;
void gl_CheckNodes(MapData * map);
bool gl_LoadGLNodes(MapData * map);


side_t* getNextSide(sector_t * sec, line_t* line);


extern FTexture *glpart2;
extern FTexture *glpart;
extern FTexture *mirrortexture;
extern FTexture *gllight;


#endif
