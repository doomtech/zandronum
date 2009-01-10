uniform float desaturation_factor;

void main()
{
	vec4 texel = gettexel();
	float gray = texel.r * 0.3 + texel.g * 0.56 + texel.b * 0.14;
	gl_FragColor = lightpixel(mix (vec4(gray,gray,gray,texel.a), texel, desaturation_factor));
}