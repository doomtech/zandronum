varying vec4 warpData;
uniform sampler2D tex;

vec2 getWarpOffset(vec2 texCoord)
{
	const float pi = 3.14159265358979323846;
	vec2 offset = vec2(0,0);

	offset.y = sin(pi * 2.0 * (texCoord.x + warpData.x * 0.25)) * 0.05;
	offset.x = sin(pi * 2.0 * (texCoord.y + warpData.x * 0.25)) * 0.05;

	return offset * warpData.y;
}

void main()
{
	vec2 texCoord = gl_TexCoord[0].st;
	texCoord += getWarpOffset(texCoord);
	gl_FragColor = texture2D(tex, texCoord) * gl_Color;
}
