//created by Evil Space Tomato
uniform float timer;

vec4 Process(vec4 color)
{
	vec2 texCoord = gl_TexCoord[0].st;
	vec4 basicColor = getTexel(texCoord) * color;

	float texX = sin(texCoord.x * 100 + timer*5);
	float texY = cos(texCoord.x * 100 + timer*5);
	float vX = (texX/texY)*21.0f;
	float vY = (texY/texX)*13.0f;


	float test = mod(timer*2.0f+(vX + vY), 0.5f);

//	float test = mod(timer*2.0f+((texCoord.x/texCoord.y*abs(basicColor.r + basicColor.g/2))*21.0f + (texCoord.y/texCoord.x*abs(basicColor.b + basicColor.g/2))*13.0f), 0.3f);
	basicColor.a = basicColor.a * test;

	basicColor.r = basicColor.g = basicColor.b = 0.0f;//(basicColor.r + basicColor.g + basicColor.b) / 3.0f;
	return basicColor;
}
