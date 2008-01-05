

#include "m_menu.h"
#include "v_video.h"
#include "gl/gl_intern.h"
#include "version.h"


extern value_t YesNo[2];
extern value_t NoYes[2];
extern value_t OnOff[2];
extern bool gl_disabled;

void StartGLLightMenu (void);
void StartDisableGL();
void ReturnToMainMenu();

CUSTOM_CVAR(Bool, gl_nogl, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	Printf("This won't take effect until "GAMENAME" is restarted.\n");
}

EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR(Int, gl_spriteclip)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool, gl_blendcolormaps)
EXTERN_CVAR(Bool, gl_texture_usehires)
EXTERN_CVAR(Bool, gl_precache)
EXTERN_CVAR(Bool, gl_render_precise)
EXTERN_CVAR(Bool, gl_sprite_blend)
EXTERN_CVAR(Bool, gl_fakecontrast)
EXTERN_CVAR (Bool, gl_lights_additive)
EXTERN_CVAR(Bool, gl_warp_shader)
EXTERN_CVAR(Bool, gl_colormap_shader)
EXTERN_CVAR(Bool, gl_brightmap_shader)
EXTERN_CVAR (Float, gl_light_ambient)
EXTERN_CVAR(Int, gl_billboard_mode)
EXTERN_CVAR(Int, gl_particles_style)

static value_t SpriteclipModes[]=
{
	{ 0.0, "Never" },
	{ 1.0, "Smart" },
	{ 2.0, "Always" },
};

static value_t FilterModes[] =
{
	{ 0.0, "None" },
	{ 1.0, "None (mipmapped)" },
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

static value_t LightingModes[] =
{
	{ 0.0, "Standard" },
	{ 1.0, "Bright" },
	{ 3.0, "Doom" },
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
	{ 1.0, "Y Axis" },
	{ 2.0, "X/Y Axis" },
};


static value_t Particles[] =
{
	{ 0.0, "Square" },
	{ 1.0, "Round" },
	{ 2.0, "Smooth" },
};



menuitem_t OpenGLItems[] = {
	{ more,     "Dynamic Light Options", {NULL}, {0.0}, {0.0},	{0.0}, {(value_t *)StartGLLightMenu} },
	{ redtext,	" ",						{NULL},							{0.0}, {0.0}, {0.0}, {NULL} },
	{ discrete, "Sprite billboard",			{&gl_billboard_mode},			{2.0}, {0.0}, {0.0}, {BillboardModes} },
	{ redtext,	" ",						{NULL},							{0.0}, {0.0}, {0.0}, {NULL} },
	{ discrete, "Vertical Sync",			{&vid_vsync},					{2.0}, {0.0}, {0.0}, {OnOff} },
//	{ discrete, "Refresh rate",				{&gl_vid_refreshHz},			{7.0}, {0.0}, {0.0}, {Hz} },
//	{ more,		"Apply Refresh rate setting",{NULL+},						{7.0}, {0.0}, {0.0}, {(value_t *)ApplyRefresh} },
	{ discrete, "Rendering quality",		{&gl_render_precise},			{2.0}, {0.0}, {0.0}, {Precision} },
	{ discrete, "Environment map on mirrors",{&gl_mirror_envmap},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Enhanced night vision mode",{&gl_enhanced_lightamp},		{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Sector light mode",		{&gl_lightmode},				{4.0}, {0.0}, {0.0}, {LightingModes} },
	{ discrete, "Adjust sprite clipping",	{&gl_spriteclip},				{3.0}, {0.0}, {0.0}, {SpriteclipModes} },
	{ discrete, "Smooth sprite edges",		{&gl_sprite_blend},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Particle style",			{&gl_particles_style},			{3.0}, {0.0}, {0.0}, {Particles} },
	{ discrete, "Enable brightness maps",	{&gl_brightmap_shader},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Shaders for texture warp",	{&gl_warp_shader},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Shaders for colormaps",	{&gl_colormap_shader},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Depth Fog",				{&gl_depthfog},					{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete, "Fake contrast",			{&gl_fakecontrast},				{2.0}, {0.0}, {0.0}, {OnOff} },
	//{ discrete, "Boom colormap handling",	{&gl_blendcolormaps},			{2.0}, {0.0}, {0.0}, {Colormaps} },
	{ slider,	"Ambient light level",		{&gl_light_ambient},			{0.0}, {255.0}, {5.0}, {NULL} },
	{ redtext,	" ",						{NULL},							{0.0}, {0.0}, {0.0}, {NULL} },
	{ discrete, "Textures enabled",			{&gl_texture},					{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Texture Filter mode",		{&gl_texture_filter},			{5.0}, {0.0}, {0.0}, {FilterModes} },
	{ discrete, "Anisotropic filter",		{&gl_texture_filter_anisotropic},{5.0},{0.0}, {0.0}, {Anisotropy} },
	{ discrete, "Texture Format",			{&gl_texture_format},			{4.0}, {0.0}, {0.0}, {TextureFormats} },
	{ discrete, "Enable hires textures",	{&gl_texture_usehires},			{2.0}, {0.0}, {0.0}, {YesNo} },
	{ discrete, "Precache GL textures",		{&gl_precache},					{2.0}, {0.0}, {0.0}, {YesNo} },
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

void StartDisableGL()
{
	M_SwitchMenu(&OpenGLMessage);
	gl_nogl=true;
}

void ReturnToMainMenu()
{
	M_StartControlPanel(false);
}

void StartGLMenu (void)
{
	if (!gl_disabled)
	{
		M_SwitchMenu(&OpenGLMenu);
	}
	else
	{
		M_SwitchMenu(&OpenGLMessage);
		gl_nogl=false;
	}
}

void StartGLLightMenu (void)
{
	M_SwitchMenu(&GLLightMenu);
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

