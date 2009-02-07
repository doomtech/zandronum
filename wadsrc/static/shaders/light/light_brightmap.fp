varying float fogcoord;
uniform int fogenabled;
uniform sampler2D brightmap;
uniform vec3 camerapos;
varying vec3 pixelpos;
uniform float lightfactor;
uniform float lightdist;

vec4 lightpixel(vec4 pixin)
{
	vec4 lightcolor = gl_Color;

	if (fogenabled != 0)
	{
		const float LOG2E = 1.442692;	// = 1/log(2)
		float fc;
		if (fogenabled == 1) fc = fogcoord;
		else fc = max(16.0, distance(pixelpos, camerapos));
		
		if (lightfactor != 1.0 && fc < lightdist) 
		{
			pixin.rgb *= lightfactor - (fc / lightdist) * (lightfactor - 1.0);
		}
		
		float factor = exp2 ( -gl_Fog.density * fc * LOG2E);
		lightcolor = vec4(mix(gl_Fog.color, lightcolor, factor).rgb, lightcolor.a);
	}

	vec4 bright = texture2D(brightmap, gl_TexCoord[0].st);// * (vec4(1.0,1.0,1.0,1.0) - lightcolor);
	bright.a = 0.0;
	//lightcolor += bright;
	lightcolor = min (lightcolor + bright, 1.0);
	return pixin * lightcolor;
}
