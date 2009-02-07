

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

EXTERN_CVAR(Int, gl_weaponlight)
EXTERN_CVAR(Bool, gl_atifog)

EXTERN_CVAR(Bool, gl_forcemultipass)
EXTERN_CVAR(Bool, gl_warp_shader)
EXTERN_CVAR(Bool, gl_fog_shader)
EXTERN_CVAR(Bool, gl_colormap_shader)
EXTERN_CVAR(Bool, gl_brightmap_shader)
EXTERN_CVAR(Bool, gl_glow_shader)

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

extern float pitch;
extern float viewvecX,viewvecY;

extern int rendered_lines,rendered_flats,rendered_sprites,rendered_decals,render_vertexsplit,render_texsplit;
extern int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;
extern DWORD gl_fixedcolormap;
extern int gl_lightcount;
extern int gl_spriteindex;
extern int palette_brightness;
extern AActor * viewactor;
extern bool gl_shaderactive;

typedef enum
{
        area_normal,
        area_below,
        area_above,
		area_default
} area_t;

extern area_t			in_area;


#endif // _GL_INTERN_H
