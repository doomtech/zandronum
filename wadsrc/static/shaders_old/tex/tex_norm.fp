uniform sampler2D tex;

vec4 gettexel()
{
	vec2 texCoord = gl_TexCoord[0].st;
	return texture2D(tex, texCoord);
}
