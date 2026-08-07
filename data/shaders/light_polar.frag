#version 120

uniform sampler2D texture;
uniform sampler2D shadowmap;
uniform vec2 lightcenter;
uniform float lightradius;
uniform float shadowrow;
uniform float shadowrows;
uniform float shadowsamples;
uniform vec2 viewtl;
uniform vec2 viewbr;
uniform vec2 targetsize;

float DecodeShadowDistance(vec4 Encoded)
{
	return Encoded.g * (256.0 / 257.0) + Encoded.r * (1.0 / 257.0);
}

void main(void)
{
	vec4 Light = texture2D(texture, gl_TexCoord[0].xy) * gl_Color;
	if(Light.a <= 0.0 || lightradius <= 0.0 || targetsize.x <= 0.0 || targetsize.y <= 0.0 || shadowsamples <= 1.0)
	{
		gl_FragColor = Light;
		return;
	}

	vec2 Pixel = gl_FragCoord.xy;
	vec2 World = vec2(
		mix(viewtl.x, viewbr.x, Pixel.x / targetsize.x),
		mix(viewbr.y, viewtl.y, Pixel.y / targetsize.y));
	vec2 ToFragment = World - lightcenter;
	float Distance = length(ToFragment);
	float Visibility = 1.0;
	if(Distance > 1.0 && Distance < lightradius)
	{
		float Angle = fract(atan(ToFragment.y, ToFragment.x) / 6.28318530 + 1.0);
		float SamplePosition = Angle * shadowsamples;
		float FirstSample = floor(SamplePosition);
		float Blend = fract(SamplePosition);
		Blend = Blend * Blend * (3.0 - 2.0 * Blend);
		float U0 = (mod(FirstSample, shadowsamples) + 0.5) / shadowsamples;
		float U1 = (mod(FirstSample + 1.0, shadowsamples) + 0.5) / shadowsamples;
		float V = (shadowrow + 0.5) / shadowrows;
		float Distance0 = DecodeShadowDistance(texture2D(shadowmap, vec2(U0, V)));
		float Distance1 = DecodeShadowDistance(texture2D(shadowmap, vec2(U1, V)));
		float Edge = max(4.0, lightradius * 0.014);
		float Boundary0 = Distance0 * lightradius;
		float Boundary1 = Distance1 * lightradius;
		float Visibility0 = 1.0 - smoothstep(Boundary0 - Edge, Boundary0 + Edge, Distance);
		float Visibility1 = 1.0 - smoothstep(Boundary1 - Edge, Boundary1 + Edge, Distance);
		Visibility = mix(Visibility0, Visibility1, Blend);
	}

	gl_FragColor = vec4(Light.rgb * Visibility, Light.a);
}
