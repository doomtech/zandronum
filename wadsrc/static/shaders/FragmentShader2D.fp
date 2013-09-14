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
	gl_FragColor = texel;
}
