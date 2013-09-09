
#ifndef __GL_LIGHTS_H__
#define __GL_LIGHTS_H__

#include "gl/gl_intern.h"
#include "gl/gl_data.h"
#include "gl/common/glc_geometric.h"
#include "gl/common/glc_dynlight.h"

//
// Light helper methods
//


extern unsigned int frameStartMS;

bool gl_SetupLight(Plane & p, ADynamicLight * light, Vector & nearPt, Vector & up, Vector & right, float & scale, int desaturation, bool checkside=true, bool forceadditive=true);
bool gl_SetupLightTexture();
void gl_GetLightForThing(AActor * thing, float upper, float lower, float & r, float & g, float & b);

extern bool i_useshaders;

#endif
