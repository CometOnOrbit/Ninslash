#version 120

uniform float rnd;
uniform float time;
uniform float intensity;
uniform int screenwidth;
uniform int screenheight;

float hash21(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float valueNoise(vec2 p)
{
	vec2 cell = floor(p);
	vec2 local = fract(p);
	local = local * local * (3.0 - 2.0 * local);
	return mix(mix(hash21(cell), hash21(cell + vec2(1.0, 0.0)), local.x),
		mix(hash21(cell + vec2(0.0, 1.0)), hash21(cell + vec2(1.0, 1.0)), local.x),
		local.y);
}

float edgeDistance(vec2 uv)
{
	return min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
}

void main(void)
{
	vec2 resolution = max(vec2(float(screenwidth), float(screenheight)), vec2(1.0));
	vec2 uv = gl_TexCoord[0].st;
	vec2 centered = uv * 2.0 - 1.0;
	centered.x *= resolution.x / resolution.y;
	float danger = clamp(intensity, 0.0, 1.0);
	float critical = smoothstep(0.42, 0.84, danger);
	float t = time * 0.01;

	float coarseNoise = valueNoise(uv * vec2(12.0, 8.0) + vec2(t * 0.17, -t * 0.11));
	float fineNoise = hash21(floor(gl_FragCoord.xy / 3.0) + floor(t * 19.0) + rnd * 61.0);
	float width = mix(0.045, 0.30, danger);
	float brokenEdge = edgeDistance(uv) + (coarseNoise - 0.5) * mix(0.024, 0.070, danger);
	float edgeMask = 1.0 - smoothstep(width * 0.28, width, brokenEdge);
	float deepEdge = 1.0 - smoothstep(0.0, width * 0.32, brokenEdge);

	float horizontalBand = 1.0 - smoothstep(0.025, 0.075, abs(fract(uv.y * 9.0 - t * 0.43) - 0.5));
	float scanGate = step(0.68, hash21(vec2(floor(uv.y * 42.0), floor(t * 5.0))));
	float scanGlitch = horizontalBand * scanGate * edgeMask;

	float diagonalA = abs(fract((uv.x * 1.7 + uv.y) * 7.0 + coarseNoise * 0.22) - 0.5);
	float diagonalB = abs(fract((uv.x - uv.y * 1.35) * 8.0 - coarseNoise * 0.18) - 0.5);
	float fracture = (1.0 - smoothstep(0.022, 0.055, min(diagonalA, diagonalB))) * edgeMask;
	fracture *= step(0.48, coarseNoise + fineNoise * 0.28);

	float beatPhase = fract(t * mix(0.82, 1.18, critical));
	float firstBeat = exp(-42.0 * abs(beatPhase - 0.10));
	float secondBeat = exp(-55.0 * abs(beatPhase - 0.27)) * 0.58;
	float heartbeat = clamp(firstBeat + secondBeat, 0.0, 1.0);
	float ringRadius = mix(1.30, 0.62, smoothstep(0.02, 0.48, beatPhase));
	float ring = 1.0 - smoothstep(0.018, 0.055, abs(length(centered) - ringRadius));
	ring *= heartbeat * (0.55 + critical * 0.85);

	float tunnel = smoothstep(0.48, 1.24, length(centered));
	float staticFlicker = (fineNoise - 0.5) * (0.026 + critical * 0.052) * edgeMask;
	vec3 red = vec3(1.0, 0.006, 0.018);
	vec3 darkRed = vec3(0.065, 0.0, 0.008);
	vec3 cyanGhost = vec3(0.0, 0.34, 0.44);
	vec3 color = darkRed * (edgeMask * 0.68 + tunnel * critical * 0.28);
	color += red * (edgeMask * (0.48 + danger * 0.46) + fracture * 0.72 + ring * 0.48);
	color += cyanGhost * scanGlitch * critical * 0.34;
	color += staticFlicker;

	float onset = smoothstep(0.0, 0.10, danger);
	float alpha = edgeMask * (0.18 + danger * 0.34);
	alpha += deepEdge * (0.16 + danger * 0.24);
	alpha += fracture * (0.16 + danger * 0.24);
	alpha += scanGlitch * critical * 0.20;
	alpha += ring * (0.10 + critical * 0.16);
	alpha += tunnel * critical * 0.13;
	alpha *= onset * (0.78 + heartbeat * 0.32);
	gl_FragColor = vec4(clamp(color, 0.0, 1.0), clamp(alpha, 0.0, 0.78)) * gl_Color;
}
