//===========================================================================
//
// Special version of the shader framework for
// full screen colormaps. This is kept separate because its
// functionality is mutually exclusive with the regular shader.
//
//===========================================================================

// common stuff
uniform sampler2D texture1;
uniform int texturemode;
uniform float timer;
uniform float desaturation_factor;

// used to set color for fullscreen colormaps
uniform vec4 colormapcolor;

vec4 ProcessPixel();

//===========================================================================
//
// Applies texture mode
//
//===========================================================================

vec4 ApplyTextureMode(vec4 texel)
{
	if (texturemode == 2) texel.a=1.0;
	else if (texturemode == 1) texel.rgb = vec3(1.0,1.0,1.0);
	return texel;
}

//===========================================================================
//
// These 3 routines are needed so that the same ProcessPixel routines
// as for the regular shader can be used with this framework.
//
//===========================================================================

vec4 GetPixelLight()
{
	return gl_Color;
}

vec4 Desaturate(vec4 texel)
{
	return texel;
}

vec4 ApplyPixelFog(vec4 pixin)
{
	return pixin;
}

//===========================================================================
//
// Full screen colormaps
//
// The preset default colormaps are:
//
// inverse == (1.0,1.0,1.0,-1.0)
// gold == (1.5, 0.5, 0.0, 1.0)
// red == (1.5, 0.0, 0.0, 1.0)
// green == (1.5, 1.5, 1.0, 1.0)
//
//===========================================================================

vec4 ApplyColormap(vec4 texel)
{
	float gray = texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14;
	if (colormapcolor.a < 0.0) gray = 1.0 - gray;
	
	return clamp(vec4(colormapcolor.r*gray, colormapcolor.g*gray, colormapcolor.b*gray, texel.a), 0.0, 1.0);
}

//===========================================================================
//
// Main entry point for shader
//
//===========================================================================

void main()
{
	vec4 texel = ProcessPixel();	// ProcessPixel is the shader specific routine which is located in a separate file.
	if (colormapcolor.a != 0.0) texel = ApplyColormap(texel);
	gl_FragColor = texel;
}