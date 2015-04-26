//created by Evil Space Tomato
uniform float timer;

vec4 Process(vec4 color)
{
	vec2 texCoord = gl_TexCoord[0].st;
	vec4 basicColor = getTexel(texCoord) * color;

	texCoord.x = int(texCoord.x * 128) / 128.0f;
	texCoord.y = int(texCoord.y * 128) / 128.0f;

	float texX = sin(mod(texCoord.x * 100 + timer*5, 3.489)) + texCoord.x / 4;
	float texY = cos(mod(texCoord.y * 100 + timer*5, 3.489)) + texCoord.y / 4;
	float vX = (texX/texY)*21.0f;
	float vY = (texY/texX)*13.0f;


	float test = mod(timer*2.0f+(vX + vY), 0.5f);
	basicColor.a = basicColor.a * test;
	basicColor.rgb = vec3(0.0f,0.0f,0.0f);
	return basicColor;
}
