#version 120

// The light source contribution is bound to unit 0 and the collision flags
// texture to unit 1. This shader intentionally stays within GLSL 1.20: the
// game still supports fixed-function fallback hardware.
uniform sampler2D texture;
uniform sampler2D collision;
uniform vec2 lightcenter;
uniform float lightradius;
uniform vec2 collisionsize;
uniform vec2 viewtl;
uniform vec2 viewbr;
uniform vec2 targetsize;

int CollisionFlag(vec2 Tile)
{
	if(Tile.x < 0.0 || Tile.y < 0.0 || Tile.x >= collisionsize.x || Tile.y >= collisionsize.y)
		return 1;
	vec2 Uv = (Tile + vec2(0.5, 0.5)) / collisionsize;
	int Flag = int(texture2D(collision, Uv).a * 255.0 + 0.5);
	// Directional collision tiles are solid. Platforms are intentionally omitted:
	// they are one-way gameplay surfaces (the player can pass through them from
	// below), so they must not cast an all-direction light shadow.
	if(Flag == 129 || Flag == 130)
		return 1;
	if(Flag == 132) // COLFLAG_PLATFORM: non-opaque one-way surface
		return 0;
	return Flag;
}

bool IsBlocked(vec2 World, int Flag)
{
	vec2 Local = fract(World / 32.0);
	// Leave a one-pixel tolerance along a ramp edge. Exact comparisons make a
	// ray that is parallel to the slope flip between the two sides due to
	// floating-point rounding, producing a solid black line on the ramp.
	const float SlopeEpsilon = 0.05;

	// Damage/death tiles affect gameplay but are not opaque geometry. Only
	// solid tiles and the four slope shapes cast light shadows.
	if(Flag == 1)
		return true;
	if(Flag == 8) // TILE_RAMP_LEFT: local x < local y
		return Local.x < Local.y - SlopeEpsilon;
	if(Flag == 16) // TILE_RAMP_RIGHT: local x + local y > 1
		return Local.x + Local.y > 1.0 + SlopeEpsilon;
	if(Flag == 32) // TILE_ROOFSLOPE_LEFT: local x + local y < 1
		return Local.x + Local.y < 1.0 - SlopeEpsilon;
	if(Flag == 64) // TILE_ROOFSLOPE_RIGHT: local x > local y
		return Local.x > Local.y + SlopeEpsilon;
	return false;
}

float RayVisibility(vec2 Origin, vec2 Target)
{
	vec2 ToTarget = Target - Origin;
	float Distance = length(ToTarget);
	if(Distance <= 1.0)
		return 1.0;

	vec2 Direction = ToTarget / Distance;
	vec2 Tile = floor(Origin / 32.0);
	vec2 TileStep = vec2(Direction.x >= 0.0 ? 1.0 : -1.0,
		Direction.y >= 0.0 ? 1.0 : -1.0);
	float DeltaX = abs(Direction.x) > 0.0001 ? 32.0 / abs(Direction.x) : 1.0e20;
	float DeltaY = abs(Direction.y) > 0.0001 ? 32.0 / abs(Direction.y) : 1.0e20;
	float BoundaryX = Direction.x >= 0.0 ? (Tile.x + 1.0) * 32.0 : Tile.x * 32.0;
	float BoundaryY = Direction.y >= 0.0 ? (Tile.y + 1.0) * 32.0 : Tile.y * 32.0;
	float NextX = abs(Direction.x) > 0.0001 ? (BoundaryX - Origin.x) / Direction.x : 1.0e20;
	float NextY = abs(Direction.y) > 0.0001 ? (BoundaryY - Origin.y) / Direction.y : 1.0e20;
	float Current = 0.0;

	// A 700px ray can cross at most about 44 tiles along a diagonal. Keep a
	// small margin while avoiding the old 96-iteration loop, which encouraged
	// GLSL 1.20 drivers to unroll excessive work for every light fragment.
	for(int i = 0; i < 48; i++)
	{
		float Next = min(NextX, NextY);
		if(Next > Distance)
			Next = Distance;
		int Flag = CollisionFlag(Tile);
		if(Flag == 1)
			return 0.0;
		// The camera light is centered on the player's body. When the player is
		// standing on a slope, that origin can be a few pixels inside the slope's
		// solid half. Do not let the origin tile self-shadow the player; subsequent
		// tiles remain fully opaque.
		bool OriginSlope = i == 0 &&
			(Flag == 8 || Flag == 16 || Flag == 32 || Flag == 64) &&
			IsBlocked(Origin, Flag);

		// Slopes are half-planes. Their predicate is linear along a ray, so
		// testing both ends and the midpoint of the tile segment catches the
		// exact crossing without reintroducing a fine-grained ray step.
		float SegmentLength = max(Next - Current, 0.0);
		float SegmentInset = min(0.5, SegmentLength * 0.25);
		float SegmentStartDistance = Current + SegmentInset;
		float SegmentEndDistance = Next - SegmentInset;
		if(SegmentEndDistance < SegmentStartDistance)
			SegmentStartDistance = SegmentEndDistance = (Current + Next) * 0.5;
		vec2 SegmentStart = Origin + Direction * SegmentStartDistance;
		vec2 SegmentEnd = Origin + Direction * SegmentEndDistance;
		vec2 SegmentMiddle = (SegmentStart + SegmentEnd) * 0.5;
		if(!OriginSlope && (IsBlocked(SegmentStart, Flag) || IsBlocked(SegmentMiddle, Flag) ||
			IsBlocked(SegmentEnd, Flag)))
			return 0.0;

		if(Next >= Distance)
			break;
		if(NextX < NextY)
		{
			Tile.x += TileStep.x;
			Current = NextX;
			NextX += DeltaX;
		}
		else
		{
			Tile.y += TileStep.y;
			Current = NextY;
			NextY += DeltaY;
		}
	}
	return 1.0;
}

void main()
{
	vec4 Light = texture2D(texture, gl_TexCoord[0].xy);
	if(Light.a <= 0.0 || lightradius <= 0.0 || targetsize.x <= 0.0 || targetsize.y <= 0.0)
	{
		gl_FragColor = Light;
		return;
	}

	// gl_FragCoord has its origin at the lower-left, while MapScreen uses a
	// top-left world rectangle. Use the actual FBO dimensions, not the saved
	// logical window size, so HiDPI and resized windows stay aligned.
	vec2 Pixel = gl_FragCoord.xy;
	vec2 World = vec2(
		mix(viewtl.x, viewbr.x, Pixel.x / targetsize.x),
		mix(viewbr.y, viewtl.y, Pixel.y / targetsize.y));
	vec2 ToFragment = World - lightcenter;
	float Distance = length(ToFragment);
	float Visibility = 1.0;

	if(Distance > 1.0 && Distance < lightradius)
	{
		// Three sub-pixel-offset rays turn the binary tile boundary into a
		// stable coverage value. This removes the near-player stair steps while
		// retaining exact collision tests and avoids a noisy random dither.
		vec2 Direction = ToFragment / Distance;
		vec2 Perpendicular = vec2(-Direction.y, Direction.x);
		Visibility = 0.0;
		for(int Sample = -1; Sample <= 1; Sample++)
		{
			vec2 SampleWorld = World + Perpendicular * float(Sample) * 2.0;
			Visibility += RayVisibility(lightcenter, SampleWorld);
		}
		Visibility /= 3.0;
	}

	gl_FragColor = vec4(Light.rgb * Visibility, Light.a);
}
