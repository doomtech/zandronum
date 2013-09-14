varying float fogcoord;
uniform int fogenabled;
uniform vec3 camerapos;
varying vec3 pixelpos;
uniform float lightfactor;
uniform float lightdist;

vec4 lightpixel(vec4 pixin)
{
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
		pixin *= gl_Color;
		return vec4(mix(gl_Fog.color, pixin, factor).rgb, pixin.a);
	}
	else return pixin * gl_Color;
}
