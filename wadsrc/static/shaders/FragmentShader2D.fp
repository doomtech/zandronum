//===========================================================================
//
// Main shader framework
// This contains all common subroutines used by the various shaders
//
//===========================================================================

// common stuff
uniform sampler2D texture1;
uniform int texturemode;
uniform float timer;

vec4 ProcessPixel();

//===========================================================================
//
// Calculates light color for a pixel - always constant for 2D
//
//===========================================================================

vec4 GetPixelLight()
{
	return gl_Color;
}

//===========================================================================
//
// Applies desaturation if needed
//
//===========================================================================

vec4 Desaturate(vec4 texel)
{
	return texel;
}

//===========================================================================
//
// Applies non-black fog
//
//===========================================================================

vec4 ApplyPixelFog(vec4 pixin)
{
	return pixin;
}

//===========================================================================
//
// Main entry point for shader
//
//===========================================================================

void main()
{
	vec4 texel = ProcessPixel();	// ProcessPixel is the shader specific routine which is located in a separate file.
	if (texturemode == 1) texel.a=1.0;
	else if (texturemode == 2) texel.rgb = gl_Color.rgb;
	gl_FragColor = texel;
}