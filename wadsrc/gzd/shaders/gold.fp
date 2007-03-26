uniform sampler2D tex;

void main()
{
	vec4 texel = texture2D(tex, gl_TexCoord[0].st);
	float gray = texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14;
	float grayx = clamp(gray * 1.5, 0.0, 1.0);
	gl_FragColor = vec4(grayx, gray, 0.0, texel.a * gl_Color.a);
}
