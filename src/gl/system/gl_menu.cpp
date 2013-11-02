

#include "gl/system/gl_system.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "m_menu.h"
#include "v_video.h"
#include "version.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"

// GL related CVARs
CVAR(Bool, gl_portals, true, 0)
CVAR(Bool, gl_noquery, false, 0)

CUSTOM_CVAR(Int, r_mirror_recursions,4,CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	if (self<0) self=0;
	if (self>10) self=10;
}
bool gl_plane_reflection_i;	// This is needed in a header that cannot include the CVAR stuff...
CUSTOM_CVAR(Bool, gl_plane_reflection, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	gl_plane_reflection_i = self;
}

CVAR(Bool,gl_mirrors,true,0)	// This is for debugging only!
CVAR(Bool,gl_mirror_envmap, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR(Bool, gl_render_segs, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_seamless, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)


CUSTOM_CVAR(Bool, gl_render_precise, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	//gl_render_segs=self;
	gl_seamless=self;
}



extern value_t YesNo[2];
extern value_t NoYes[2];
extern value_t OnOff[2];

void StartGLLightMenu (void);
void StartGLTextureMenu (void);
void StartGLPrefMenu (void);
void StartGLShaderMenu (void);
void ReturnToMainMenu();

CVAR(Bool, gl_vid_compatibility, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

EXTERN_CVAR(Bool, vid_vsync)
EXTERN_CVAR(Int, gl_spriteclip)
EXTERN_CVAR(Bool, gl_spritebrightfog)
EXTERN_CVAR(Int, gl_enhanced_nv_stealth)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool, gl_precache)
EXTERN_CVAR(Bool, gl_render_precise)
EXTERN_CVAR(Bool, gl_sprite_blend)
EXTERN_CVAR(Bool, gl_trimsprites)
EXTERN_CVAR(Bool, gl_lights_additive)
EXTERN_CVAR(Int, gl_light_ambient)
EXTERN_CVAR(Int, gl_billboard_mode)
EXTERN_CVAR(Int, gl_particles_style)
EXTERN_CVAR(Int, gl_texture_hqresize)
EXTERN_CVAR(Flag, gl_texture_hqresize_textures)
EXTERN_CVAR(Flag, gl_texture_hqresize_sprites)
EXTERN_CVAR(Flag, gl_texture_hqresize_fonts)
// [BB]
EXTERN_CVAR(Bool, gl_use_models)

static value_t SpriteclipModes[]=
{
	{ 0.0, "Never" },
	{ 1.0, "Smart" },
	{ 2.0, "Always" },
	{ 3.0, "Smarter" },
};

static value_t EnhancedStealth[]=
{
	{ 0.0, "Never" },
	{ 1.0, "Infrared only" },
	{ 2.0, "Infrared and torch" },
	{ 3.0, "Any fixed colormap" },
};

static value_t VBOModes[]=
{
	{ 0.0, "Off" },
	{ 1.0, "Static" },
	{ 2.0, "Dynamic" },
};

static value_t FilterModes[] =
{
	{ 0.0, "None" },
	{ 1.0, "None (nearest mipmap)" },
	{ 5.0, "None (linear mipmap)" },
	{ 2.0, "Linear" },
	{ 3.0, "Bilinear" },
	{ 4.0, "Trilinear" },
};

static value_t TextureFormats[] =
{
	{ 0.0, "RGBA8" },
	{ 1.0, "RGB5_A1" },
	{ 2.0, "RGBA4" },
	{ 3.0, "RGBA2" },
	// [BB] Added modes for texture compression.
	{ 4.0, "COMPR_RGBA" },
	{ 5.0, "S3TC_DXT1" },
	{ 6.0, "S3TC_DXT3" },
	{ 7.0, "S3TC_DXT5" },
};

static value_t Anisotropy[] =
{
	{ 1.0, "Off" },
	{ 2.0, "2x" },
	{ 4.0, "4x" },
	{ 8.0, "8x" },
	{ 16.0, "16x" },
};

static value_t Colormaps[] =
{
	{ 0.0, "Use as palette" },
	{ 1.0, "Blend" },
};

static value_t LightingModes2[] =
{
	{ 0.0, "Standard" },
	{ 1.0, "Bright" },
	{ 3.0, "Doom" },
	{ 4.0, "Legacy" },
};

static value_t LightingModes[] =
{
	{ 0.0, "Standard" },
	{ 1.0, "Bright" },
	{ 2.0, "Doom" },
	{ 3.0, "Dark" },
	{ 4.0, "Legacy" },
};

static value_t Precision[] =
{
	{ 0.0, "Speed" },
	{ 1.0, "Quality" },
};


static value_t Hz[] =
{
	{ 0.0, "Optimal" },
	{ 60.0, "60" },
	{ 70.0, "70" },
	{ 72.0, "72" },
	{ 75.0, "75" },
	{ 85.0, "85" },
	{ 100.0, "100" }
};

static value_t BillboardModes[] =
{
	{ 0.0, "Y Axis" },
	{ 1.0, "X/Y Axis" },
};


static value_t Particles[] =
{
	{ 0.0, "Square" },
	{ 1.0, "Round" },
	{ 2.0, "Smooth" },
};

static value_t HqResizeModes[] =
{
   { 0.0, "Off" },
   { 1.0, "Scale2x" },
   { 2.0, "Scale3x" },
   { 3.0, "Scale4x" },
// [BB] hqnx scaling is only supported with the MS compiler.
#if (defined _MSC_VER) && (!defined _WIN64)
   { 4.0, "hq2x" },
   { 5.0, "hq3x" },
   { 6.0, "hq4x" },
#endif
};

#if (defined _MSC_VER) && (!defined _WIN64)
	const float MAXRESIZE = 7.;
#else
	const float MAXRESIZE = 4.;
#endif

static value_t HqResizeTargets[] =
{
   { 0.0, "Everything" },
   { 1.0, "Sprites/fonts" },
};
 
static value_t FogMode[] =
{
	{ 0.0, "Off" },
	{ 1.0, "Standard" },
	{ 2.0, "Radial" },
};

static menuitem_t OpenGLItems[] = {
	{ more,     "Dynamic Light Options",	{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLLightMenu} },
	{ more,     "Texture Options",			{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLTextureMenu} },
	{ more,     "Shader Options",			{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLShaderMenu} },
	{ more,     "Preferences",				{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLPrefMenu} },
};

static menuitem_t OpenGLItems2[] = {
	{ more,     "Dynamic Light Options",	{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLLightMenu} },
	{ more,     "Texture Options",			{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLTextureMenu} },
	{ more,     "Preferences",				{NULL}, {0.0}, {0.0},	{0.0},	{(value_t *)StartGLPrefMenu} },
};


menuitem_t GLTextureItems[] = {
	{ discrete, "Textures enabled",			{&gl_texture},					{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Texture Filter mode",		{&gl_texture_filter},			{6.0}, {0.0}, {0.0}, {FilterModes} },
	{ discrete, "Anisotropic filter",		{&gl_texture_filter_anisotropic},{5.0},{0.0}, {0.0}, {Anisotropy} },
	{ discrete, "Texture Format",			{&gl_texture_format},			{8.0}, {0.0}, {0.0}, {TextureFormats} },
	{ discrete, "Enable hires textures",	{&gl_texture_usehires},			{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "High Quality Resize mode",	{&gl_texture_hqresize},			{MAXRESIZE}, {0.0}, {0.0}, {HqResizeModes} },
	{ discrete, "Resize textures",			{&gl_texture_hqresize_textures},{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Resize sprites",			{&gl_texture_hqresize_sprites},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Resize fonts",				{&gl_texture_hqresize_fonts},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Precache GL textures",		{&gl_precache},					{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Camera textures offscreen",{&gl_usefb},					{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Trim sprite edges",		{&gl_trimsprites},				{2.0}, {0.0}, {0.0}, {OnOff} },
};

menuitem_t GLLightItems[] = {
	{ discrete, "Dynamic Lights enabled",	{&gl_lights},			{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Enable light definitions",	{&gl_attachedlights},	{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Clip lights",				{&gl_lights_checkside},	{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Lights affect sprites",	{&gl_light_sprites},	{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Lights affect particles",	{&gl_light_particles},	{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Force additive lighting",	{&gl_lights_additive},	{2.0}, {0.0}, {0.0}, {YesNo} },
	{ slider,	"Light intensity",			{&gl_lights_intensity}, {0.0}, {1.0}, {0.1f}, {NULL} },
	{ slider,	"Light size",				{&gl_lights_size},		{0.0}, {2.0}, {0.1f}, {NULL} },
	{ discrete, "Use shaders for lights",	{&gl_dynlight_shader},	{2.0}, {0.0}, {0.0}, {YesNo} },
};

menuitem_t GLPrefItems[] = {
	{ discrete, "Sector light mode",		{&gl_lightmode},				{4.0}, {0.0}, {0.0}, {LightingModes} },
	{ discrete, "Fog mode",					{&gl_fogmode},					{3.0}, {0.0}, {0.0}, {FogMode} },
	{ discrete, "Environment map on mirrors",{&gl_mirror_envmap},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Enhanced night vision mode",{&gl_enhanced_nightvision},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "ENV shows stealth monsters",{&gl_enhanced_nv_stealth},		{4.0}, {0.0}, {0.0}, {EnhancedStealth} },
	{ discrete, "Force brightness in fog",	{&gl_spritebrightfog},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Adjust sprite clipping",	{&gl_spriteclip},				{4.0}, {0.0}, {0.0}, {SpriteclipModes} },
	{ discrete, "Smooth sprite edges",		{&gl_sprite_blend},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Sprite billboard",			{&gl_billboard_mode},			{2.0}, {0.0}, {0.0}, {BillboardModes} },
	{ discrete, "Particle style",			{&gl_particles_style},			{3.0}, {0.0}, {0.0}, {Particles} },
	{ slider,	"Ambient light level",		{&gl_light_ambient},			{1.0}, {255.0}, {5.0}, {NULL} },
	{ discrete, "Rendering quality",		{&gl_render_precise},			{2.0}, {0.0}, {0.0}, {Precision} },
	{ discrete, "Use vertex buffer",		{&gl_usevbo},					{3.0}, {0.0}, {0.0}, {VBOModes} },
	// [BB]
	{ discrete, "Use models",				{&gl_use_models},				{2.0}, {0.0}, {0.0}, {OnOff} },
};

menuitem_t GLShaderItems[] = {
	{ discrete, "Enable brightness maps",	{&gl_brightmap_shader},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Shaders for texture warp",	{&gl_warp_shader},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Shaders for fog",			{&gl_fog_shader},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Shaders for colormaps",	{&gl_colormap_shader},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Shaders for glowing textures",	{&gl_glow_shader},			{2.0}, {0.0}, {0.0}, {OnOff} },
};

menuitem_t OpenGLDisabled[] = {
	{ redtext,	"This won't take effect",			{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	"until "GAMENAME" is restarted.",		{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ more,     "",									{NULL}, {0.0}, {0.0},	{0.0}, {(value_t *)ReturnToMainMenu} },
};

menu_t OpenGLMenu = {
   "OPENGL OPTIONS",
   0,
   sizeof(OpenGLItems)/sizeof(OpenGLItems[0]),
   0,
   OpenGLItems,
   0,
};

menu_t OpenGLMessage = {
   "",
   2,
   sizeof(OpenGLDisabled)/sizeof(OpenGLDisabled[0]),
   0,
   OpenGLDisabled,
   0,
};

menu_t GLLightMenu = {
   "LIGHT OPTIONS",
   0,
   sizeof(GLLightItems)/sizeof(GLLightItems[0]),
   0,
   GLLightItems,
   0,
};

menu_t GLTextureMenu = {
   "TEXTURE OPTIONS",
   0,
   sizeof(GLTextureItems)/sizeof(GLTextureItems[0]),
   0,
   GLTextureItems,
   0,
};

menu_t GLPrefMenu = {
   "PREFERENCES",
   0,
   sizeof(GLPrefItems)/sizeof(GLPrefItems[0]),
   0,
   GLPrefItems,
   0,
};

menu_t GLShaderMenu = {
   "SHADER OPTIONS",
   0,
   sizeof(GLShaderItems)/sizeof(GLShaderItems[0]),
   0,
   GLShaderItems,
   0,
};

void ReturnToMainMenu()
{
	M_StartControlPanel(false);
}

void StartGLMenu (void)
{
	M_SwitchMenu(&OpenGLMenu);
}

void StartGLLightMenu (void)
{
	if (gl.maxuniforms < 1024)
	{
		GLLightMenu.numitems = sizeof(GLLightItems)/sizeof(GLLightItems[0]) - 1;
	}
	M_SwitchMenu(&GLLightMenu);
}

void StartGLTextureMenu (void)
{
	M_SwitchMenu(&GLTextureMenu);
}

void StartGLPrefMenu (void)
{
	M_SwitchMenu(&GLPrefMenu);
}

void StartGLShaderMenu (void)
{
	M_SwitchMenu(&GLShaderMenu);
}

void gl_SetupMenu()
{
	if (gl.shadermodel == 2)
	{
		// Radial fog and Doom lighting are not available in SM 2 cards
		// The way they are implemented does not work well on older hardware.
		// For SM 3 this is implemented through shader recompilation.

		menuitem_t *lightmodeitem = &GLPrefItems[0];
		menuitem_t *fogmodeitem = &GLPrefItems[1];

		// disable 'Doom' lighting mode
		lightmodeitem->e.values = LightingModes2;
		lightmodeitem->b.numvalues = 4;

		// disable radial fog
		fogmodeitem->b.numvalues = 2;

		// disable features that don't work without shaders.
		if (gl_lightmode == 2) gl_lightmode = 3;
		if (gl_fogmode == 2) gl_fogmode = 1;
	}

	if (gl.shadermodel != 3)
	{
		// The shader menu will only be visible on SM3. 
		// SM2 won't use shaders unless unavoidable (and then it's automatic) and SM4 will always use shaders.
		OpenGLMenu.numitems = sizeof(OpenGLItems2)/sizeof(OpenGLItems2[0]);
		OpenGLMenu.items = OpenGLItems2;
	}
}

CUSTOM_CVAR (Float, vid_brightness, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma(Gamma); //Brightness (self);
	}
}

CUSTOM_CVAR (Float, vid_contrast, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma(Gamma); //SetContrast (self);
	}
}

