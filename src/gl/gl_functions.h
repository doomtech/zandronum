#ifndef __GL_FUNCT
#define __GL_FUNCT

#include "v_palette.h"

class AActor;

void gl_PreprocessLevel();
void gl_CleanLevelData();

// [BB] This construction purposely overrides the CVAR gl_fogmode with a local variable of the same name.
// This allows to implement ZADF_FORCE_GL_DEFAULTS by only putting this define at the beginning of a function
// that uses gl_fogmode without any further changes in that function.
#define OVERRIDE_FOGMODE_IF_NECESSARY \
	const int gl_fogmode_CVAR_value = gl_fogmode; \
	const int gl_fogmode = ( ( zadmflags & ZADF_FORCE_GL_DEFAULTS ) && ( gl_fogmode_CVAR_value == 0 ) ) ? 1 : gl_fogmode_CVAR_value;

// [EP] Override also gl_light_ambient and gl_lightmode in the same way as gl_fogmode,
// except that the method is different, since gl_fogmode == 0 is treated as if gl_fogmode == 1.
#define TEMP_CVAR( cvar ) cvar ## _CVAR_value
#define TEMP_DEFAULT_CVAR( cvar ) cvar ## _CVAR_defaultvalue

#define OVERRIDE_INT_GL_CVAR_IF_NECESSARY( cvar ) \
	const int TEMP_CVAR( cvar ) = cvar; \
	const int TEMP_DEFAULT_CVAR( cvar ) = cvar.GetGenericRepDefault( CVAR_Int ).Int; \
	const int cvar = ( zadmflags & ZADF_FORCE_GL_DEFAULTS ) ? TEMP_DEFAULT_CVAR( cvar ) : TEMP_CVAR( cvar )

void gl_LinkLights();
void gl_SetActorLights(AActor *);

#endif
