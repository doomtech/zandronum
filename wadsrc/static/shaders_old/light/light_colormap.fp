uniform vec4 colormapstart;
uniform vec4 colormaprange;

vec4 lightpixel(vec4 texel)
{
	float gray = (texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14);	
	return vec4(clamp(colormapstart.rgb + gray * colormaprange.rgb, 0.0, 1.0), texel.a) * gl_Color;
}

