
vec4 lightpixel(vec4 texel)
{
	float gray = 1.0 - (texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14);
	return vec4(gray, gray, gray, texel.a) * gl_Color;
}

