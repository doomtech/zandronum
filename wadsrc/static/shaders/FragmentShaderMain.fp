//===========================================================================
//
// Main shader framework
// This contains all common subroutines used by the various shaders
//
//===========================================================================

// Vertex based input
varying vec3 LightParams;
varying vec4 FogColor;
varying vec2 GlowDistance;
varying vec4 GlowTopColor;
varying vec4 GlowBottomColor;

varying float fogcoord;
varying vec3 pixelpos;


// Fog related variables
uniform int fogradial;
uniform vec4 camerapos;

// common stuff
uniform sampler2D texture1;
uniform int texturemode;
uniform float timer;
uniform float desaturation_factor;

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
// Calculates light color for a pixel
// Includes black fog which in Doom is part of the light
//
//===========================================================================

vec4 GetPixelLight()
{
	vec4 color = gl_Color;
	if (GlowTopColor.a > 0.0 && GlowDistance.x < GlowTopColor.a)
	{
		color.rgb += GlowTopColor.rgb * (1.0 - GlowDistance.x / GlowTopColor.a);
		color = min(color, 1.0);
	}
	if (GlowBottomColor.a > 0.0 && GlowDistance.y < GlowBottomColor.a)
	{
		color.rgb += GlowBottomColor.rgb * (1.0 - GlowDistance.y / GlowBottomColor.a);
		color = min(color, 1.0);
	}
	
	// Light diminishing
	if (FogColor.a == 0.0)
	{
		float fc;
		
		if (fogradial == 0) fc = fogcoord;
		else fc = max(16.0, distance(pixelpos, camerapos.xyz));
		color.rgb *= exp2 (LightParams.x * fc);
		
		float lfactor = LightParams.y;
		float dist = LightParams.z;
		if (lfactor != 1.0 && fc < dist) 
		{
			color.rgb *= lfactor - (fc / dist) * (lfactor - 1.0);
		}
	}
	return color;
}


//===========================================================================
//
// Applies desaturation if needed
//
//===========================================================================

vec4 Desaturate(vec4 texel)
{
	if (desaturation_factor == 0.0) return texel;
	
	float gray = texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14;
	return mix (vec4(gray,gray,gray,texel.a), texel, desaturation_factor);
}

//===========================================================================
//
// Applies non-black fog
//
//===========================================================================

vec4 ApplyPixelFog(vec4 pixin)
{
	// only for colored fog
	if (FogColor.a == 1.0)
	{
		float fc;
		if (fogradial == 0) fc = fogcoord;
		else fc = max(16.0, distance(pixelpos, camerapos.xyz));
		
		float factor = exp2 (LightParams.x * fc);
		return vec4(mix(FogColor, pixin, factor).rgb, pixin.a);
	}
	else return pixin;
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