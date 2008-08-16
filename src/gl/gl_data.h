
#ifndef __GL_DATA_H
#define __GL_DATA_H


#include "r_defs.h"
#include "gl/gl_basic.h"
#include "gl/gl_intern.h"


side_t* getNextSide(sector_t * sec, line_t* line);

extern FTexture *glpart2;
extern FTexture *glpart;
extern FTexture *mirrortexture;
extern FTexture *gllight;


#endif
