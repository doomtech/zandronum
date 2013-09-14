uniform int texturemode;

void main()
{
	vec4 texel = gettexel();
	if (texturemode == 2) 
	{
		texel.a=1.0;
	}
	else if (texturemode == 1) 
	{
		texel.rgb = vec3(1.0,1.0,1.0);
	}
	gl_FragColor = lightpixel(texel);
}