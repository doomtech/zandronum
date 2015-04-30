#ifndef __GL_FUNCT
#define __GL_FUNCT

#include "v_palette.h"

class AActor;

void gl_PreprocessLevel();
void gl_CleanLevelData();
// [BB] Get value of gl_lightmode, respecting ZADF_FORCE_GL_DEFAULTS.
int gl_GetLightMode ( );

// [BB] This construction purposely overrides the CVAR gl_fogmode with a local variable of the same name.
// This allows to implement ZADF_FORCE_GL_DEFAULTS by only putting this define at the beginning of a function
// that uses gl_fogmode without any further changes in that function.
#define OVERRIDE_FOGMODE_IF_NECESSARY \
	const int gl_fogmode_CVAR_value = gl_fogmode; \
	const int gl_fogmode = ( ( zadmflags & ZADF_FORCE_GL_DEFAULTS ) && ( gl_fogmode_CVAR_value == 0 ) ) ? 1 : gl_fogmode_CVAR_value;

void gl_LinkLights();
void gl_SetActorLights(AActor *);

#endif
