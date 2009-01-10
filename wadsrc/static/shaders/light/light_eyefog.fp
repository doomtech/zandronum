varying float fogcoord;
uniform int fogenabled;

vec4 lightpixel(vec4 pixin)
{
	if (fogenabled != 0)
	{
		const float LOG2E = 1.442692;	// = 1/log(2)
		float factor = exp2 ( -gl_Fog.density * fogcoord * LOG2E);
		pixin *= gl_Color;
		return vec4(mix(gl_Fog.color, pixin, factor).rgb, pixin.a);
	}
	else return pixin * gl_Color;
}
