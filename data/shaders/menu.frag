#version 120

uniform sampler2D texture;
uniform float rnd;
uniform float intensity;
uniform float colorswap;
uniform float time;
uniform int screenwidth;
uniform int screenheight;

float hash21(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise2(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
			   mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x),
			   f.y);
}

float gridMask(vec2 uv, vec2 cells)
{
	vec2 edge = abs(fract(uv * cells) - 0.5);
	return max(smoothstep(0.482, 0.5, edge.x), smoothstep(0.482, 0.5, edge.y));
}

void main (void)
{
	vec2 resolution = max(vec2(float(screenwidth), float(screenheight)) / 8.0, vec2(1.0));
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 p = uv * 2.0 - 1.0;
	p.x *= resolution.x / resolution.y;
	float t = time * 0.0016;

	float atmosphere = noise2(p * 1.45 + vec2(t * 0.07, -t * 0.025));
	float fineNoise = hash21(gl_FragCoord.xy + rnd * 137.0) - 0.5;
	float minorGrid = gridMask(uv + vec2(t * 0.0025, 0.0), vec2(24.0, 14.0));
	float majorGrid = gridMask(uv + vec2(t * 0.0012, 0.0), vec2(6.0, 3.5));

	float cyanBloom = exp(-3.0 * dot(p - vec2(-0.48 + sin(t * 0.18) * 0.08, 0.12),
									 p - vec2(-0.48 + sin(t * 0.18) * 0.08, 0.12)));
	float mintBloom = exp(-4.2 * dot(p - vec2(0.62, -0.42 + cos(t * 0.14) * 0.06),
									 p - vec2(0.62, -0.42 + cos(t * 0.14) * 0.06)));
	float amberBloom = exp(-8.0 * dot(p - vec2(0.76, 0.58), p - vec2(0.76, 0.58)));
	float diagonal = smoothstep(0.085, 0.0, abs(p.x + p.y * 0.64 - 0.12 - sin(t * 0.2) * 0.05));
	float scan = smoothstep(0.035, 0.0, abs(fract(uv.y * 2.0 - t * 0.045) - 0.5));

	vec3 c = vec3(0.004, 0.012, 0.026);
	c += vec3(0.015, 0.095, 0.145) * (0.28 + atmosphere * 0.50);
	c += vec3(0.020, 0.360, 0.480) * cyanBloom * 0.34;
	c += vec3(0.060, 0.310, 0.220) * mintBloom * 0.24;
	c += vec3(0.330, 0.145, 0.025) * amberBloom * 0.13;
	c += vec3(0.070, 0.360, 0.440) * diagonal * 0.13;
	c += vec3(0.060, 0.250, 0.310) * minorGrid * 0.055;
	c += vec3(0.100, 0.430, 0.520) * majorGrid * 0.045;
	c += vec3(0.055, 0.210, 0.250) * scan * 0.028;
	c += fineNoise * 0.012;

	float vignette = 1.0 - smoothstep(0.48, 1.34, length(p));
	c *= 0.56 + vignette * 0.62;
	float fade = clamp(0.76 + intensity * 0.018, 0.76, 1.0);
	gl_FragColor = vec4(c * fade, 0.92) * gl_Color;
}
