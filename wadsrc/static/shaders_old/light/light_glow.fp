varying float fogcoord;
uniform int fogenabled;
uniform vec3 camerapos;
varying vec3 pixelpos;
uniform float lightfactor;
uniform float lightdist;

uniform vec4 topglowcolor;
uniform vec4 bottomglowcolor;
varying float topdist;
varying float bottomdist;

vec4 lightpixel(vec4 pixin)
{
	vec4 color = gl_Color;
	if (topglowcolor.a > 0.0 && topdist < topglowcolor.a)
	{
		color.rgb += topglowcolor.rgb * (1.0 - topdist / topglowcolor.a);
	}
	if (bottomglowcolor.a > 0.0 && bottomdist < bottomglowcolor.a)
	{
		color.rgb += bottomglowcolor.rgb * (1.0 - bottomdist / bottomglowcolor.a);
	}
	color = min(color, 1.0);
	
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
		pixin *= color;
		return vec4(mix(gl_Fog.color, pixin, factor).rgb, pixin.a);
	}
	else return pixin * color;
}
