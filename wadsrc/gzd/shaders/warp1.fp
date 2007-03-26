varying vec4 warpData;
varying float fogcoord;
uniform sampler2D tex;

vec2 getWarpOffset(vec2 texCoord)
{
	// warpData.x = time factor x
	// warpData.y = warp amount x
	// warpData.z = time factor y
	// warpData.w = warp amount y

	const float pi = 3.14159265358979323846;
	vec2 offset = vec2(0,0);

	offset.y = sin(pi * 2.0 * (texCoord.x + warpData.x * 0.25)) * 0.05;
	offset.x = sin(pi * 2.0 * (texCoord.y + warpData.x * 0.25)) * 0.05;

	return offset * warpData.y;
}

void main()
{
	const float LOG2E = 1.442692;	// = 1/log(2)
	vec2 texCoord = gl_TexCoord[0].st;
	texCoord += getWarpOffset(texCoord);
	vec4 texel = texture2D(tex, texCoord) * gl_Color;

	float factor = exp2 ( -gl_Fog.density * fogcoord * LOG2E);
	vec4 fogged = mix (gl_Fog.color, texel, factor);

	gl_FragColor = vec4(fogged.rgb, texel.a);
}
