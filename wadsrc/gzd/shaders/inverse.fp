uniform sampler2D tex;

void main()
{
	vec4 texel = texture2D(tex, gl_TexCoord[0].st);
	float gray = 1.0 - (texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14);
	gl_FragColor = vec4(gray, gray, gray, texel.a * gl_Color.a);
}
