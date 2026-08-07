#version 120

uniform sampler2D texture;
uniform vec2 targetsize;

void main(void)
{
	vec2 Texel = 1.0 / max(targetsize, vec2(1.0));
	vec2 Uv = gl_TexCoord[0].xy;
	vec4 Light = texture2D(texture, Uv) * 0.40;
	Light += texture2D(texture, Uv + vec2(Texel.x, 0.0)) * 0.15;
	Light += texture2D(texture, Uv - vec2(Texel.x, 0.0)) * 0.15;
	Light += texture2D(texture, Uv + vec2(0.0, Texel.y)) * 0.15;
	Light += texture2D(texture, Uv - vec2(0.0, Texel.y)) * 0.15;
	gl_FragColor = Light * gl_Color;
}
