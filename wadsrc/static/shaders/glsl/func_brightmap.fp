uniform sampler2D texture2;

vec4 Process(vec4 color)
{
	vec4 brightpix = desaturate(texture2D(texture2, gl_TexCoord[0].st));
	return getTexel(gl_TexCoord[0].st) * min (color + brightpix, 1.0);
}

