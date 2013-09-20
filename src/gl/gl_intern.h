

#ifndef _GL_INTERN_H
#define _GL_INTERN_H

#include "r_defs.h"
#include "c_cvars.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

EXTERN_CVAR(Bool, gl_vid_compatibility)
EXTERN_CVAR(Bool,gl_enhanced_nightvision)
EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Int, gl_texture_filter)
EXTERN_CVAR(Float, gl_texture_filter_anisotropic)
EXTERN_CVAR(Int, gl_texture_format)
EXTERN_CVAR(Bool, gl_texture_usehires)

EXTERN_CVAR(Int, gl_weaponlight)
EXTERN_CVAR(Bool, gl_atifog)

EXTERN_CVAR(Bool, gl_forcemultipass)

EXTERN_CVAR (Bool, gl_lights);
EXTERN_CVAR (Bool, gl_attachedlights);
EXTERN_CVAR (Bool, gl_lights_checkside);
EXTERN_CVAR (Float, gl_lights_intensity);
EXTERN_CVAR (Float, gl_lights_size);
EXTERN_CVAR (Bool, gl_lights_additive);
EXTERN_CVAR (Bool, gl_light_sprites);
EXTERN_CVAR (Bool, gl_light_particles);

EXTERN_CVAR(Int, gl_fogmode)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool,gl_mirror_envmap)
EXTERN_CVAR(Int,gl_nearclip)

extern int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;
extern int gl_spriteindex;
extern int palette_brightness;
extern bool gl_shaderactive;


#endif // _GL_INTERN_H
