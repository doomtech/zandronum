
vec4 lightpixel(vec4 texel)
{
	float gray = texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14;
	float grayx = clamp(gray * 1.5, 0.0, 1.0);
	return vec4(grayx, gray * 0.5, 0.0, texel.a) * gl_Color;
}

