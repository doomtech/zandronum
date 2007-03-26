varying vec4 warpData;
uniform sampler2D tex;

vec2 getWarpOffset(vec2 texCoord)
{
	const float pi = 3.14159265358979323846;
	vec2 offset = vec2(0,0);

	float siny = sin(pi * 2.0 * (texCoord.y * 2.2 + warpData.x * 0.75)) * 0.03;
	offset.y = siny + sin(pi * 2.0 * (texCoord.x * 0.75 + warpData.x * 0.75)) * 0.03;
	offset.x = siny + sin(pi * 2.0 * (texCoord.x * 1.1 + warpData.x * 0.45)) * 0.02;

	return offset;
}

void main()
{
	vec2 texCoord = gl_TexCoord[0].st;
	texCoord += getWarpOffset(texCoord);
	gl_FragColor = texture2D(tex, texCoord) * gl_Color;
}
