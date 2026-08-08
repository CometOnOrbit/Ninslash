#ifndef GAME_NODES_COLLISION_H
#define GAME_NODES_COLLISION_H

#include <base/math.h>

#include <game/collision.h>
#include <game/nodes.h>

inline bool NodesBuildingSpaceOccupied(CCollision *pCollision, const CNodesBuildingBounds &Bounds)
{
	if(!pCollision)
		return true;

	const float MinX = Bounds.m_Min.x + 1.0f;
	const float MaxX = Bounds.m_Max.x - 1.0f;
	const float MinY = Bounds.m_Min.y + 1.0f;
	const float MaxY = Bounds.m_Max.y - 2.0f;
	if(MinX > MaxX || MinY > MaxY)
		return true;

	const int NumX = max(1, (int)ceilf((MaxX - MinX) / 8.0f) + 1);
	const int NumY = max(1, (int)ceilf((MaxY - MinY) / 8.0f) + 1);
	for(int YIndex = 0; YIndex < NumY; ++YIndex)
	{
		const float Y = min(MaxY, MinY + YIndex * 8.0f);
		for(int XIndex = 0; XIndex < NumX; ++XIndex)
		{
			const float X = min(MaxX, MinX + XIndex * 8.0f);
			if(pCollision->CheckPoint(X, Y))
				return true;
		}
	}
	return false;
}

inline bool NodesBuildingHasGroundSupport(CCollision *pCollision, const CNodesBuildingBounds &Bounds)
{
	if(!pCollision)
		return false;

	const float MinX = Bounds.m_Min.x + 1.0f;
	const float MaxX = Bounds.m_Max.x - 1.0f;
	const float SupportY = Bounds.m_Max.y + 3.0f;
	if(MinX > MaxX)
		return false;

	const int NumX = max(1, (int)ceilf((MaxX - MinX) / 8.0f) + 1);
	for(int XIndex = 0; XIndex < NumX; ++XIndex)
	{
		const float X = min(MaxX, MinX + XIndex * 8.0f);
		if(!pCollision->CheckPoint(X, SupportY))
			return false;
	}
	return true;
}

inline bool NodesBuildingInBounds(CCollision *pCollision, const CNodesBuildingBounds &Bounds)
{
	if(!pCollision)
		return false;
	const float MapWidth = pCollision->GetWidth() * 32.0f;
	const float MapHeight = pCollision->GetHeight() * 32.0f;
	return MapWidth <= 0.0f || (Bounds.m_Min.x >= 0.0f && Bounds.m_Max.x <= MapWidth && Bounds.m_Min.y >= 0.0f && Bounds.m_Max.y <= MapHeight);
}

inline bool NodesBuildingFits(CCollision *pCollision, vec2 Pos, const CNodesBuildingInfo &Info);

inline bool NodesBuildingFindGroundAtX(CCollision *pCollision, float X, float RequestedY, const CNodesBuildingInfo &Info, vec2 *pPosition, vec2 *pGround)
{
	if(!pCollision || !pPosition)
		return false;

	vec2 RayOrigin(X, max(0.0f, RequestedY - Info.m_Height - 64.0f));
	for(int i = 0; i < 64 && pCollision->CheckPoint(RayOrigin) && RayOrigin.y > 0.0f; ++i)
		RayOrigin.y = max(0.0f, RayOrigin.y - 32.0f);
	if(pCollision->CheckPoint(RayOrigin))
		return false;

	for(int Surface = 0; Surface < 64; ++Surface)
	{
		vec2 Ground;
		if(!pCollision->IntersectLine(RayOrigin, vec2(X, pCollision->GetHeight() * 32.0f), &Ground, nullptr))
			return false;
		const vec2 Candidate(X, Ground.y - NODES_BUILDING_BOTTOM_OFFSET);
		if(NodesBuildingFits(pCollision, Candidate, Info))
		{
			*pPosition = Candidate;
			if(pGround)
				*pGround = Ground;
			return true;
		}

		RayOrigin = Ground + vec2(0.0f, 1.0f);
		for(int i = 0; i < 64 && pCollision->CheckPoint(RayOrigin); ++i)
			RayOrigin.y += 32.0f;
		if(pCollision->CheckPoint(RayOrigin))
			return false;
	}
	return false;
}

inline bool NodesBuildingFindGround(CCollision *pCollision, vec2 Requested, const CNodesBuildingInfo &Info, vec2 *pPosition, vec2 *pGround)
{
	if(!pCollision || !pPosition)
		return false;

	const float CenterX = floorf(Requested.x / 32.0f) * 32.0f + 16.0f;
	for(int Radius = 0; Radius <= 8; ++Radius)
	{
		for(int Side = 0; Side < (Radius == 0 ? 1 : 2); ++Side)
		{
			const float X = CenterX + (Side == 0 ? -Radius : Radius) * 32.0f;
			if(NodesBuildingFindGroundAtX(pCollision, X, Requested.y, Info, pPosition, pGround))
				return true;
		}
	}
	return false;
}

inline bool NodesBuildingFits(CCollision *pCollision, vec2 Pos, const CNodesBuildingInfo &Info)
{
	const CNodesBuildingBounds Bounds = NodesBuildingBounds(Pos, Info);
	return NodesBuildingInBounds(pCollision, Bounds) && !NodesBuildingSpaceOccupied(pCollision, Bounds) &&
		NodesBuildingHasGroundSupport(pCollision, Bounds);
}

#endif
