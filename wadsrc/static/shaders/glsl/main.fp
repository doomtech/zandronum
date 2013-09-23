#ifdef DYNLIGHT_TEST

#version 120
#extension GL_EXT_gpu_shader4 : enable
uniform samplerBuffer lightRGB;
uniform sampler2D tex;


void main()
{
	int index = int(clamp(gl_TexCoord[0].s, 0, 32.0));
	vec4 light = texelFetchBuffer(lightRGB, index);
	vec4 texel = texture2D(tex, gl_TexCoord[0].st);

	gl_FragColor = texel*2.0 + light/20.0;
}

vec4 desaturate(vec4 texel)
{
	return texel;
}

vec4 getTexel(vec2 st)
{
	return texture2D(tex, st);
}

#else


#ifdef DYNLIGHT
#version 120
#extension GL_EXT_gpu_shader4 : enable
uniform ivec3 lightrange;
uniform usamplerBuffer lightIndex;
uniform samplerBuffer lightPositions;
uniform samplerBuffer lightRGB;

#endif



varying float fogcoord;
uniform int fogenabled;
uniform vec3 camerapos;
varying vec3 pixelpos;
uniform float lightfactor;
uniform float lightdist;
uniform float desaturation_factor;

uniform vec4 topglowcolor;
uniform vec4 bottomglowcolor;
varying float topdist;
varying float bottomdist;

uniform int texturemode;
uniform sampler2D tex;

vec4 Process(vec4 color);


//===========================================================================
//
// Desaturate a color
//
//===========================================================================

vec4 desaturate(vec4 texel)
{
	#ifndef NO_DESATURATE
		float gray = (texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14);	
		return mix (vec4(gray,gray,gray,texel.a), texel, desaturation_factor);
	#else
		return texel;
	#endif
}

//===========================================================================
//
// Calculate light
//
//===========================================================================

vec4 getLightColor(float fogdist, float fogfactor)
{
	vec4 color = gl_Color;

	#ifndef NO_GLOW
	//
	// handle glowing walls
	//
	if (topglowcolor.a > 0.0 && topdist < topglowcolor.a)
	{
		color.rgb += desaturate(topglowcolor * (1.0 - topdist / topglowcolor.a)).rgb;
	}
	if (bottomglowcolor.a > 0.0 && bottomdist < bottomglowcolor.a)
	{
		color.rgb += desaturate(bottomglowcolor * (1.0 - bottomdist / bottomglowcolor.a)).rgb;
	}
	color = min(color, 1.0);
	#endif

	#ifndef NO_FOG
	//
	// apply light diminishing	
	//
	if (fogenabled > 0)
	{
		#ifndef NO_SM4
			// special lighting mode 'Doom' not available on older cards for performance reasons.
			if (fogdist < lightdist) 
			{
				color.rgb *= lightfactor - (fogdist / lightdist) * (lightfactor - 1.0);
			}
		#endif
		
		//color = vec4(color.rgb * (1.0 - fogfactor), color.a);
		color.rgb = mix(vec3(0.0, 0.0, 0.0), color.rgb, fogfactor);
	}
	#endif
	
	// calculation of actual light color is complete.
	return color;
}

//===========================================================================
//
// Gets a texel and performs common manipulations
//
//===========================================================================

vec4 getTexel(vec2 st)
{
	vec4 texel = texture2D(tex, st);
	
	#ifndef NO_TEXTUREMODE
	//
	// Apply texture modes
	//
	if (texturemode == 2) 
	{
		texel.a = 1.0;
	}
	else if (texturemode == 1) 
	{
		texel.rgb = vec3(1.0,1.0,1.0);
	}
	#endif

	return desaturate(texel);
}

//===========================================================================
//
// Applies colored fog
//
//===========================================================================

#ifndef NO_FOG
vec4 applyFog(vec4 frag, float fogfactor)
{
	return vec4(mix(gl_Fog.color, frag, fogfactor).rgb, frag.a);
}
#endif


//===========================================================================
//
// Main shader routine
//
//===========================================================================

void main()
{
	float fogdist = 0.0;
	float fogfactor = 0.0;
	
	#ifdef DYNLIGHT
		vec4 dynlight = vec4(0.0,0.0,0.0,0.0);
		vec4 addlight = vec4(0.0,0.0,0.0,0.0);
	#endif

	#ifndef NO_FOG
	//
	// calculate fog factor
	//
	if (fogenabled != 0)
	{
		const float LOG2E = 1.442692;	// = 1/log(2)
		//if (abs(fogenabled) == 1) 
		#ifndef NO_SM4
			if (fogenabled == 1 || fogenabled == -1) 
			{
				fogdist = fogcoord;
			}
			else 
			{
				fogdist = max(16.0, distance(pixelpos, camerapos));
			}
		#else
			fogdist = fogcoord;
		#endif
		fogfactor = exp2 ( -gl_Fog.density * fogdist * LOG2E);
	}
	#endif
	
	vec4 frag = getLightColor(fogdist, fogfactor);
	
	
	#ifdef DYNLIGHT
		for(int i=lightrange.x; i<lightrange.y; i++)
		{
			int lightidx = int(texelFetchBuffer(lightIndex, i).r);
			vec4 lightpos = texelFetchBuffer(lightPositions, lightidx);
			vec4 lightcolor = texelFetchBuffer(lightRGB, lightidx);
			
			lightcolor.rgb *= max(lightpos.a - distance(pixelpos, lightpos.rgb),0.0) / lightpos.a;
			
			if (lightcolor.a == 1.0)
			{
				addlight += lightcolor;
			}
			else if (lightcolor.a == 0.0)
			{
				dynlight += lightcolor;
			}
			else
			{	
				dynlight -= lightcolor;
			}
		}
		if (lightrange.z != 0)
		{
			addlight += dynlight;
		}
		else
		{
			frag.rgb = clamp(frag.rgb + dynlight.rgb, 0.0, 2.0);
		}
	#endif
		
	frag = Process(frag);

	#ifdef DYNLIGHT
		frag.rgb += addlight.rgb;
	#endif

	#ifndef NO_FOG
		if (fogenabled < 0) 
		{
			frag = applyFog(frag, fogfactor);
		}
	#endif
	gl_FragColor = frag;
}


#endif // DYNLIGHT test
