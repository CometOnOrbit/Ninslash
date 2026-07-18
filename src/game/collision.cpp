#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <math.h>
#include <vector>
#include <engine/map.h>
#include <engine/kernel.h>
#include <engine/shared/mapchunk.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

void CCollision::ClearModifTileCache()
{
	for(int i = 0; i < MODIF_TILE_CACHE_SIZE; i++)
	{
		m_aModifTileGroup[i] = -1;
		m_aModifTileLayer[i] = -1;
		m_apModifTilemap[i] = 0;
		m_apModifTiles[i] = 0;
	}
}


CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
	m_pBlocks = 0;
	m_pLightRays = 0;
	ClearModifTileCache();
	m_pMapChunk = NULL;

	m_PathLen = 0;
	m_pPath = 0;
	m_pCenterWaypoint = 0;
	m_GlobalAcid = true;
	m_Time = 0;
	
	for (int i = 0; i < MAX_WAYPOINTS; i++)
		m_apWaypoint[i] = 0;
}

CCollision::~CCollision()
{
	if (m_pBlocks)
		delete[] m_pBlocks;
	
	if (m_pLightRays)
		delete[] m_pLightRays;
	
	for (int i = 0; i < MAX_WAYPOINTS; i++)
		if (m_apWaypoint[i])
			delete m_apWaypoint[i];
		
	if (m_pMapChunk)
		delete[] m_pMapChunk;
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pLayers = pLayers;
	ClearModifTileCache();
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));
	
	m_pMapChunk = m_pLayers->GetMapChunk();
	
	m_pBlocks = new bool[m_Width*m_Height];
	for (int i = 0; i < m_Width*m_Height; i++)
		m_pBlocks[i] = false;
	
	m_pLightRays = new int[m_Width*m_Height];
	for (int i = 0; i < m_Width*m_Height; i++)
		m_pLightRays[i] = 0;
	
	m_LowestPoint = 0;
	m_Time = 0;
	m_GlobalAcid = true;
	
	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;

		if(Index > 128)
			continue;
		m_pTiles[i].m_Index = 0;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID;
			break;
		case TILE_INSTADEATH:
			m_pTiles[i].m_Index = COLFLAG_INSTADEATH;
			break;
		case TILE_RAMP_LEFT:
			m_pTiles[i].m_Index = COLFLAG_RAMP_LEFT;
			break;
		case TILE_RAMP_RIGHT:
			m_pTiles[i].m_Index = COLFLAG_RAMP_RIGHT;
			break;
		case TILE_ROOFSLOPE_LEFT:
			m_pTiles[i].m_Index = COLFLAG_ROOFSLOPE_LEFT;
			break;
		case TILE_ROOFSLOPE_RIGHT:
			m_pTiles[i].m_Index = COLFLAG_ROOFSLOPE_RIGHT;
			break;
		case TILE_DAMAGEFLUID:
			m_pTiles[i].m_Index = COLFLAG_DAMAGEFLUID;
			break;
		case TILE_MOVELEFT:
			m_pTiles[i].m_Index = COLFLAG_MOVELEFT;
			break;
		case TILE_MOVERIGHT:
			m_pTiles[i].m_Index = COLFLAG_MOVERIGHT;
			break;
		case TILE_HANG:
			m_pTiles[i].m_Index = COLFLAG_HANG;
			break;
		case TILE_PLATFORM:
			m_pTiles[i].m_Index = COLFLAG_PLATFORM;
			break;
		default:
			m_pTiles[i].m_Index = 0;
		}
	}
	
	m_pCenterWaypoint = 0;
	
	for (int i = 0; i < MAX_WAYPOINTS; i++)
		m_apWaypoint[i] = 0;
	
	InitLightRays();
}

void CCollision::RefreshMapgenDimensions()
{
	if(!m_pLayers || !m_pLayers->GameLayer())
		return;

	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));
	ClearModifTileCache();
	m_LowestPoint = 0;

	if(m_pBlocks)
		delete[] m_pBlocks;
	if(m_pLightRays)
		delete[] m_pLightRays;

	m_pBlocks = new bool[m_Width*m_Height];
	m_pLightRays = new int[m_Width*m_Height];
	for(int i = 0; i < m_Width*m_Height; i++)
	{
		m_pBlocks[i] = false;
		m_pLightRays[i] = 0;
	}
}




vec2 CCollision::GetRandomWaypointPos()
{
	int n = 0;
	while (n++ < 10)
	{
		int i = rand()%m_WaypointCount;
	
		if (m_apWaypoint[i])
			return m_apWaypoint[i]->m_Pos;
	}
	
	return vec2(0, 0);
}



void CCollision::ClearWaypoints()
{
	m_WaypointCount = 0;
	
	for (int i = 0; i < MAX_WAYPOINTS; i++)
	{
		if (m_apWaypoint[i])
			delete m_apWaypoint[i];
		
		m_apWaypoint[i] = NULL;
	}
	
	m_pCenterWaypoint = NULL;
}

/*
void CCollision::WaypointAreaSize(CWaypoint *Area)
{
	
	
}
*/

void CCollision::RemoveClosedAreas()
{
	return;
	for (int i = 0; i < MAX_WAYPOINTS; i++)
	{
		if (m_apWaypoint[i])
		{
			m_apWaypoint[i]->CalculateAreaSize(0);
			
			if (m_apWaypoint[i]->GetAreaSize() < 1000)
			{
				m_apWaypoint[i]->m_ToBeDeleted = true;
			}
		}
	}
	
	for (int i = 0; i < MAX_WAYPOINTS; i++)
	{
		if (m_apWaypoint[i] && m_apWaypoint[i]->m_ToBeDeleted)
		{
			m_WaypointCount--;
			m_apWaypoint[i]->ClearConnections();
			delete m_apWaypoint[i];
			m_apWaypoint[i] = NULL;
		}
	}
}



void CCollision::AddWaypoint(vec2 Position, bool InnerCorner)
{
	if (m_WaypointCount >= MAX_WAYPOINTS)
		return;
	
	m_apWaypoint[m_WaypointCount] = new CWaypoint(Position, InnerCorner);
	m_WaypointCount++;
}





void CCollision::GenerateWaypoints()
{
	ClearWaypoints();
	
	if (m_pMapChunk)
		return;
	
	for(int x = 2; x < m_Width-2; x++)
	{
		for(int y = 2; y < m_Height-2; y++)
		{
			if (m_pTiles[y*m_Width+x].m_Index == 214)
			{
				AddWaypoint(vec2(x, y));
				continue;
			}

			if (m_pTiles[y*m_Width+x].m_Index && m_pTiles[y*m_Width+x].m_Index < 130)
				continue;

			bool SawbladeNearby = false;
			for(int yy = -1; yy <= 1 && !SawbladeNearby; yy++)
				for(int xx = -1; xx <= 1; xx++)
					if(m_pTiles[(y+yy)*m_Width+x+xx].m_Index == ENTITY_SAWBLADE + ENTITY_OFFSET)
					{
						SawbladeNearby = true;
						break;
					}
			if(SawbladeNearby)
				continue;

			auto IsSolidNeighbor = [this, x, y](int OffsetX, int OffsetY) {
				const int TileX = x + OffsetX;
				const int TileY = y + OffsetY;
				const int Index = TileY*m_Width+TileX;
				const int Tile = m_pTiles[Index].m_Index;
				if(Tile == ENTITY_SAWBLADE + ENTITY_OFFSET ||
					(m_pBlocks[Index] && TileX > 0 && TileY > 0 && TileX < m_Width-1 && TileY < m_Height-1) ||
					Tile == COLFLAG_MOVELEFT || Tile == COLFLAG_MOVERIGHT || Tile == COLFLAG_PLATFORM)
					return true;
				if(Tile > 128)
					return false;
				if(Tile & COLFLAG_SOLID)
					return true;
				if(Tile & COLFLAG_RAMP_LEFT)
					return true;
				if(Tile & COLFLAG_RAMP_RIGHT)
					return false;
				return (Tile & COLFLAG_ROOFSLOPE_LEFT) != 0 || (Tile & COLFLAG_ROOFSLOPE_RIGHT) != 0;
			};

			// find all outer corners
			if ((IsSolidNeighbor(-1, -1) && !IsSolidNeighbor(-1, 0) && !IsSolidNeighbor(0, -1)) ||
				(IsSolidNeighbor(-1, 1) && !IsSolidNeighbor(-1, 0) && !IsSolidNeighbor(0, 1)) ||
				(IsSolidNeighbor(1, 1) && !IsSolidNeighbor(1, 0) && !IsSolidNeighbor(0, 1)) ||
				(IsSolidNeighbor(1, -1) && !IsSolidNeighbor(1, 0) && !IsSolidNeighbor(0, -1)))
			{
				// outer corner found -> create a waypoint
				
				// check validity (solid tiles under the corner)
				/*
				bool Found = false;
				for (int i = 0; i < 20; ++i)
					if (IsTileSolid(x*32, (y+i)*32))
						Found = true;
					*/
					
				bool Found = true;
				
				// count slopes
				int Slopes = 0;
				
				for (int xx = -2; xx <= 2; xx++)
					for (int yy = -2; yy <= 2; yy++)
						if (GetTile((x+xx)*32, (y+yy)*32) >= COLFLAG_RAMP_LEFT) Slopes++;
				
				if (Found && Slopes < 3)
					AddWaypoint(vec2(x, y));
			}
			else
			// find all inner corners
			if ((IsSolidNeighbor(1, 0) || IsSolidNeighbor(-1, 0)) && (IsSolidNeighbor(0, -1) || IsSolidNeighbor(0, 1)))
			{
				// inner corner found -> create a waypoint
				//AddWaypoint(vec2(x, y), true);
				
				// check validity (solid tiles under the corner)
				bool Found = false;
				for (int i = 0; i < 20; ++i)
					if (IsTileSolid(x*32, (y+i)*32))
						Found = true;
					
				// count slopes
				int Slopes = 0;
				
				for (int xx = -2; xx <= 2; xx++)
					for (int yy = -2; yy <= 2; yy++)
						if (GetTile((x+xx)*32, (y+yy)*32) >= COLFLAG_RAMP_LEFT) Slopes++;
				
				// too tight spots to go
				if ((IsSolidNeighbor(0, -1) && IsSolidNeighbor(0, 1)) ||
					(IsSolidNeighbor(-1, 0) && IsSolidNeighbor(1, 0)))
					Found = false;
				
				if (Found && Slopes < 3)
					AddWaypoint(vec2(x, y));
			}
		}
	}

	if(m_WaypointCount == 0)
	{
		m_ConnectionCount = 0;
		RemoveClosedAreas();
		return;
	}

	std::vector<signed char> aVisibilityCache(MAX_WAYPOINTS * MAX_WAYPOINTS, -1);
	std::vector<unsigned char> aWaypointCollisionTiles(m_Width * m_Height, 0);
	const int CollisionTileMask = COLFLAG_SOLID | COLFLAG_DEATH | COLFLAG_INSTADEATH |
		COLFLAG_RAMP_LEFT | COLFLAG_RAMP_RIGHT | COLFLAG_ROOFSLOPE_LEFT | COLFLAG_ROOFSLOPE_RIGHT;
	for(int y = 0; y < m_Height; y++)
	{
		for(int x = 0; x < m_Width; x++)
		{
			const int Index = y * m_Width + x;
			const int Tile = m_pTiles[Index].m_Index;
			aWaypointCollisionTiles[Index] = Tile == ENTITY_SAWBLADE + ENTITY_OFFSET ||
				(m_pBlocks[Index] && x > 0 && y > 0 && x < m_Width - 1 && y < m_Height - 1) ||
				Tile == COLFLAG_MOVELEFT || Tile == COLFLAG_MOVERIGHT ||
				(Tile <= COLFLAG_DAMAGEFLUID && (Tile & CollisionTileMask));
		}
	}
		
	bool KeepGoing = true;
	bool ConnectionsCurrent = false;
	int i = 0;
	
	while (KeepGoing && i++ < 10)
	{
		ConnectWaypoints(&aVisibilityCache[0], &aWaypointCollisionTiles[0]);
		ConnectionsCurrent = true;
		KeepGoing = GenerateSomeMoreWaypoints();
		if(KeepGoing)
			ConnectionsCurrent = false;
	}
	if(!ConnectionsCurrent)
		ConnectWaypoints(&aVisibilityCache[0], &aWaypointCollisionTiles[0]);
	
	RemoveClosedAreas();
}


// create a new waypoints between connected, far apart ones
bool CCollision::GenerateSomeMoreWaypoints()
{
	bool Result = false;
	
	for (int i = 0; i < m_WaypointCount; i++)
	{
		CWaypoint *pWaypoint = m_apWaypoint[i];
		if(!pWaypoint)
			continue;

		for (int j = 0; j < m_WaypointCount; j++)
		{
			CWaypoint *pOther = m_apWaypoint[j];
			if(!pOther || !pWaypoint->Connected(pOther))
				continue;

			// Process a two-way connection only on its first traversal.
			if(j < i && pOther->Connected(pWaypoint))
				continue;

			if (abs(pWaypoint->m_X - pOther->m_X) > 20 && pWaypoint->m_Y == pOther->m_Y)
			{
				int x = (pWaypoint->m_X + pOther->m_X) / 2;

				if (IsTileSolid(x*32, (pWaypoint->m_Y+1)*32) || IsTileSolid(x*32, (pWaypoint->m_Y-1)*32))
				{
					AddWaypoint(vec2(x, pWaypoint->m_Y));
					Result = true;
				}
			}

			if (abs(pWaypoint->m_Y - pOther->m_Y) > 30 && pWaypoint->m_X == pOther->m_X)
			{
				int y = (pWaypoint->m_Y + pOther->m_Y) / 2;
				
				if (IsTileSolid((pWaypoint->m_X+1)*32, y*32) || IsTileSolid((pWaypoint->m_X-1)*32, y*32))
				{
					AddWaypoint(vec2(pWaypoint->m_X, y));
					Result = true;
				}
			}
		}
	}
	
	return Result;
}



CWaypoint *CCollision::GetWaypointAt(int x, int y)
{
	for (int i = 0; i < m_WaypointCount; i++)
	{
		if (m_apWaypoint[i])
		{
			if (m_apWaypoint[i]->m_X == x && m_apWaypoint[i]->m_Y == y)
				return m_apWaypoint[i];
		}
	}
	return NULL;
}


bool CCollision::WaypointLineBlocked(vec2 Pos0, vec2 Pos1, const unsigned char *pTileCanCollide)
{
	const float Distance = distance(Pos0, Pos1);
	if(Distance <= 0.0f)
	{
		const int X = round_to_int(Pos0.x);
		const int Y = round_to_int(Pos0.y);
		const int Nx = clamp(X / 32, 0, m_Width - 1);
		const int Ny = clamp(Y / 32, 0, m_Height - 1);
		return pTileCanCollide[Ny * m_Width + Nx] && SolidState(X, Y, true, true, true) != SS_NOCOL;
	}

	const float InvDistance = 1.0f / Distance;
	const int End = int(Distance + 1.0f);
	auto TileIndexAt = [this, Pos0, Pos1, InvDistance](int Sample) {
		const float a = Sample * InvDistance;
		const vec2 Pos = mix(Pos0, Pos1, a);
		const int X = round_to_int(Pos.x);
		const int Y = round_to_int(Pos.y);
		const int Nx = clamp(X / 32, 0, m_Width - 1);
		const int Ny = clamp(Y / 32, 0, m_Height - 1);
		return Ny * m_Width + Nx;
	};

	for(int i = 0; i < End; i++)
	{
		const float a = i * InvDistance;
		const vec2 Pos = mix(Pos0, Pos1, a);
		const int X = round_to_int(Pos.x);
		const int Y = round_to_int(Pos.y);
		const int Nx = clamp(X / 32, 0, m_Width - 1);
		const int Ny = clamp(Y / 32, 0, m_Height - 1);
		const int TileIndex = Ny * m_Width + Nx;
		if(pTileCanCollide[TileIndex])
		{
			if(SolidState(X, Y, true, true, true) != SS_NOCOL)
				return true;
			continue;
		}

		const int Probe = i + 32 < End ? i + 32 : End - 1;
		if(Probe <= i)
			continue;

		if(TileIndexAt(Probe) == TileIndex)
		{
			i = Probe;
			continue;
		}

		// Tile coordinates are monotonic along a line, so locate the last
		// original sample that remains inside this known-safe tile.
		int LastInside = i;
		int FirstOutside = Probe;
		while(LastInside + 1 < FirstOutside)
		{
			const int Middle = (LastInside + FirstOutside) / 2;
			if(TileIndexAt(Middle) == TileIndex)
				LastInside = Middle;
			else
				FirstOutside = Middle;
		}
		i = LastInside;
	}
	return false;
}


void CCollision::ConnectWaypoints(signed char *pVisibilityCache, const unsigned char *pTileCanCollide)
{
	m_ConnectionCount = 0;
	
	// clear existing connections
	for (int i = 0; i < m_WaypointCount; i++)
	{
		if (!m_apWaypoint[i])
			continue;
		
		m_apWaypoint[i]->ClearConnections();
	}
		
	
		
	for (int i = 0; i < m_WaypointCount; i++)
	{
		if (!m_apWaypoint[i])
			continue;
		
		int x, y;
		
		x = m_apWaypoint[i]->m_X - 1;
		y = m_apWaypoint[i]->m_Y;
		
		// find waypoints at left
		while (y > 0 && x > 0 && (!m_pTiles[y*m_Width+x].m_Index || m_pTiles[y*m_Width+x].m_Index >= 128))
		{
			CWaypoint *W = GetWaypointAt(x, y);
			
			if (W)
			{
				if (m_apWaypoint[i]->Connect(W))
					m_ConnectionCount++;
				break;
			}
			
			//if (!IsTileSolid(x*32, (y-1)*32) && !IsTileSolid(x*32, (y+1)*32))
			if (!IsTileSolid(x*32, (y+1)*32))
				break;
			
			x--;
		}
		
		x = m_apWaypoint[i]->m_X;
		y = m_apWaypoint[i]->m_Y - 1;
		
		int n = 0;
		
		// find waypoints at up
		//bool SolidFound = false;
		while (y > 0 && x > 0 && (!m_pTiles[y*m_Width+x].m_Index || m_pTiles[y*m_Width+x].m_Index >= 128) && n++ < 10)
		{
			CWaypoint *W = GetWaypointAt(x, y);
			
			//if (IsTileSolid((x+1)*32, y*32) || IsTileSolid((x+1)*32, y*32))
			//	SolidFound = true;
			
			//if (W && SolidFound)
			if (W)
			{
				if (m_apWaypoint[i]->Connect(W))
					m_ConnectionCount++;
				break;
			}
			
			y--;
		}
	}
	
	
	// connect to near, visible waypoints
	for (int i = 0; i < m_WaypointCount; i++)
	{
		CWaypoint *pWaypoint = m_apWaypoint[i];
		if (!pWaypoint || pWaypoint->m_InnerCorner)
			continue;
		
		for (int j = 0; j < m_WaypointCount; j++)
		{
			if (pWaypoint->m_ConnectionCount >= MAX_WAYPOINTCONNECTIONS)
				break;

			CWaypoint *pOther = m_apWaypoint[j];
			if (pOther && pOther->m_InnerCorner)
				continue;
			
			if (!pOther || pWaypoint->m_Pos.y == pOther->m_Pos.y)
				continue;

			const vec2 Delta = pWaypoint->m_Pos - pOther->m_Pos;
			if (dot(Delta, Delta) >= 1000.0f * 1000.0f || pWaypoint->Connected(pOther))
				continue;

			signed char &Visibility = pVisibilityCache[i * MAX_WAYPOINTS + j];
			if(Visibility < 0)
				Visibility = WaypointLineBlocked(pWaypoint->m_Pos, pOther->m_Pos, pTileCanCollide) ? 0 : 1;

			if(Visibility && pWaypoint->Connect(pOther))
				m_ConnectionCount++;
		}
	}
}

vec2 CCollision::GetClosestWaypointPos(vec2 Pos)
{
	CWaypoint *Wp = GetClosestWaypoint(Pos);
	
	if (Wp)
		return Wp->m_Pos;
	
	return vec2(0, 0);
}

CWaypoint *CCollision::GetClosestWaypoint(vec2 Pos)
{
	CWaypoint *W = NULL;
	float Dist = 9000;
	
	for (int i = 0; i < m_WaypointCount; i++)
	{
		if (m_apWaypoint[i])
		{
			if (m_GlobalAcid && GetGlobalAcidLevel() < Pos.y)
				continue;
			
			int d = distance(m_apWaypoint[i]->m_Pos, Pos);
			
			if (d < Dist && d < 800)
			{
				if (!FastIntersectLine(m_apWaypoint[i]->m_Pos, Pos) || Dist == 9000)
				{
					W = m_apWaypoint[i];
					Dist = d;
				}
			}
		}
	}
	
	return W;
}


void CCollision::SetWaypointCenter(vec2 Position)
{
	m_pCenterWaypoint = GetClosestWaypoint(Position);
	
	// clear path weights
	for (int i = 0; i < m_WaypointCount; i++)
	{
		if (m_apWaypoint[i])
			m_apWaypoint[i]->m_PathDistance = 0;
	}
	
	if (m_pCenterWaypoint)
		m_pCenterWaypoint->SetCenter();
	
}


void CCollision::AddWeight(vec2 Pos, int Weight)
{
	CWaypoint *Wp = GetClosestWaypoint(Pos);
	
	if (Wp)
		Wp->AddWeight(Weight);
}



void CCollision::InitLightRays()
{
	for (int y = m_Height-1; y > 0; y--)
		for (int x = 0; x < m_Width; x++)
		{
			const int Tile = GetTileRay(x*32, y*32, true);
			const int LeftTile = GetTileRay((x-1)*32, y*32, true);
			const int UpTile = GetTileRay(x*32, (y-1)*32, true);
			const int UpLeftTile = GetTileRay((x-1)*32, (y-1)*32, true);
			int &LightRay = m_pLightRays[y*m_Width+x];

			// outer corners
			if (Tile == COLFLAG_SOLID && !LeftTile && !UpTile)
				LightRay = 1;
			
			if (!Tile && LeftTile == COLFLAG_SOLID && !UpTile && !UpLeftTile)
				LightRay = 1;
			
			if (!Tile && UpTile == COLFLAG_SOLID && !UpLeftTile)
				LightRay = 1;
			
			if (!Tile && UpLeftTile == COLFLAG_SOLID && !UpTile && !LeftTile)
				LightRay = 1;
			

			// inner corners
			if (Tile == COLFLAG_SOLID && !UpTile && UpLeftTile == COLFLAG_SOLID)
				LightRay = 1;
			
			if (Tile == COLFLAG_SOLID && UpTile == COLFLAG_SOLID && LeftTile == COLFLAG_SOLID && !UpLeftTile)
				LightRay = 1;
			
			if (!Tile && UpTile == COLFLAG_SOLID && LeftTile == COLFLAG_SOLID)
				LightRay = 1;
			
			if (Tile == COLFLAG_SOLID && UpTile == COLFLAG_SOLID && !LeftTile && UpLeftTile == COLFLAG_SOLID)
				LightRay = 1;
			
			
			// slope & ramp corners
			if (Tile == COLFLAG_ROOFSLOPE_RIGHT && (UpLeftTile == COLFLAG_SOLID || !UpLeftTile))
				LightRay = 1;
			
			if ((!Tile || Tile == COLFLAG_SOLID) && UpLeftTile == COLFLAG_ROOFSLOPE_RIGHT)
				LightRay = 1;
			
			
			if (!Tile && UpTile == COLFLAG_ROOFSLOPE_LEFT && (LeftTile == COLFLAG_SOLID || !LeftTile))
				LightRay = 1;
			
			if (!Tile && (UpTile == COLFLAG_SOLID || !UpTile) && LeftTile == COLFLAG_ROOFSLOPE_LEFT)
				LightRay = 1;
			
			
			if ((Tile == COLFLAG_SOLID || !Tile) && UpLeftTile == COLFLAG_RAMP_LEFT)
				LightRay = 1;
			
			if (Tile == COLFLAG_RAMP_LEFT && (!UpLeftTile || UpLeftTile == COLFLAG_SOLID))
				LightRay = 1;
			
		
			if ((Tile == COLFLAG_SOLID || !Tile) && UpTile == COLFLAG_RAMP_RIGHT && (!LeftTile || LeftTile == COLFLAG_SOLID))
				LightRay = 1;
			
			if ((Tile == COLFLAG_SOLID || !Tile) && LeftTile == COLFLAG_RAMP_RIGHT && (!UpTile || UpTile == COLFLAG_SOLID))
				LightRay = 1;
			
			
			// screen border helpers
			if (!LightRay)
			{
				if ((!Tile && LeftTile) || (Tile && !LeftTile))
					LightRay = -1;
				
				if ((!Tile && UpTile) || (Tile && !UpTile))
					LightRay -= 2;
			}
		}
}

int CCollision::GetLightRay(ivec2 Pos)
{
	//int Nx = clamp(Pos.x/32, 0, m_Width-1);
	//int Ny = clamp(Pos.y/32, 0, m_Height-1);
	
	int Nx = GetModularPos(Pos.x/32);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(Pos.y/32, 0, m_Height-1);
	
	return m_pLightRays[Ny*m_Width+Nx];
}


void CCollision::SetBlock(ivec2 Pos, bool Block)
{
	int Nx = clamp(Pos.x/32, 0, m_Width-1);
	int Ny = clamp(Pos.y/32, 0, m_Height-1);
	
	m_pBlocks[Ny*m_Width+Nx] = Block;
}


bool CCollision::GetBlock(int x, int y)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	
	if (Nx > 0 && Ny > 0 && Nx < m_Width-1 && Ny < m_Height-1)
		return m_pBlocks[Ny*m_Width+Nx];
	
	return 0;
}

bool CCollision::CanBuildBlock(int x, int y)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	
	if (Nx > 0 && Ny > 0 && Nx < m_Width-1 && Ny < m_Height-1)
		return true;
	
	return false;
}

int CCollision::GetChunkSize() { return m_pMapChunk?m_pMapChunk->GetSize():0; }

int CCollision::GetModularPos(int x)
{
	if (m_pMapChunk)
	{
		m_pMapChunk = m_pMapChunk->GetMapChunk(x);
		int chunk = m_pMapChunk->GetIndex();
		int chunksize = m_pMapChunk->GetSize();
		return x%chunksize+chunk*chunksize;
	}
	
	return x;
}

int CCollision::GetTile(int x, int y, bool Down, bool IncludeBlocks)
{
	int Nx = GetModularPos(x/32);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	if (m_pTiles[Ny*m_Width+Nx].m_Index == ENTITY_SAWBLADE + ENTITY_OFFSET)
		return COLFLAG_SOLID;
	
	if (IncludeBlocks && m_pBlocks[Ny*m_Width+Nx] && Nx > 0 && Ny > 0 && Nx < m_Width-1 && Ny < m_Height-1)
		return COLFLAG_SOLID;
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVELEFT || m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVERIGHT)
		return COLFLAG_SOLID;
	
	if (!Down && m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_PLATFORM)
		return COLFLAG_SOLID;
	
	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
}

int CCollision::GetTileRay(int x, int y, bool Down)
{
	//int Nx = GetModularPos(x/32);
	//Nx = clamp(Nx, 0, m_Width-1);
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	if (m_pTiles[Ny*m_Width+Nx].m_Index == ENTITY_SAWBLADE + ENTITY_OFFSET)
		return COLFLAG_SOLID;
	
	if (m_pBlocks[Ny*m_Width+Nx] && Nx > 0 && Ny > 0 && Nx < m_Width-1 && Ny < m_Height-1)
		return COLFLAG_SOLID;
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVELEFT || m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVERIGHT)
		return COLFLAG_SOLID;
	
	if (!Down && m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_PLATFORM)
		return COLFLAG_SOLID;
	
	int i = m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
	
	if (i == COLFLAG_SOLID || i == COLFLAG_RAMP_LEFT || i == COLFLAG_RAMP_RIGHT || i == COLFLAG_ROOFSLOPE_LEFT || i == COLFLAG_ROOFSLOPE_RIGHT)
		return i;
	
	return 0;
}


int CCollision::GetLowestPoint()
{
	if (!m_pLayers || !m_Height)
		return 0;
	
	if (!m_LowestPoint)
		for (int y = m_Height-1; y > 0; y--)
			for (int x = 0; x < m_Width; x++)
			{
				if (!IsTileSolid(x*32, y*32))
				{
					m_LowestPoint = (y+1)*32;
					return m_LowestPoint;
				}
			}
		
	return m_LowestPoint;
}
	

float CCollision::GetGlobalAcidLevel()
{
	return GetLowestPoint() + m_Time;
}


int CCollision::ForceState(int x, int y)
{
	int Nx = GetModularPos(x/32);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVELEFT)
		return -1;
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVERIGHT)
		return 1;
	
	return 0;
}


bool CCollision::IsHangTile(float x, float y)
{
	int Nx = GetModularPos(round_to_int(x)/32);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(round_to_int(y)/32, 0, m_Height-1);
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_HANG)
		return true;
	
	return false;
}


bool CCollision::IsPlatform(float x, float y)
{
	int Nx = GetModularPos(round_to_int(x)/32);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(round_to_int(y)/32, 0, m_Height-1);
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_PLATFORM)
		return true;
	
	return false;
}

bool CCollision::IsSawblade(float x, float y)
{
	int Nx = GetModularPos(round_to_int(x)/32);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(round_to_int(y)/32, 0, m_Height-1);
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == ENTITY_SAWBLADE + ENTITY_OFFSET)
		return true;
	
	return false;
}


int CCollision::SolidState(int x, int y, bool IncludeDeath, bool Down, bool IncludeBlocks)
{
	unsigned char sol = GetTile(x, y, Down, IncludeBlocks);

	if(sol& COLFLAG_SOLID || (IncludeDeath && (sol&COLFLAG_DEATH || sol&COLFLAG_INSTADEATH)))
		return true;
	else if(sol&COLFLAG_RAMP_LEFT) {
		//return ((31-x%32) > (31-y%32));
		return ((31-x%32) > (31-y%32) ? SS_COL : ((31-x%32) == (31-y%32) ? SS_COL_RL : SS_NOCOL));
	}
	else if(sol&COLFLAG_RAMP_RIGHT) {
		//return (x%32 > (31-y%32));
		return (x%32 > (31-y%32) ? SS_COL : (x%32 == (31-y%32) ? SS_COL_RR : SS_NOCOL));
	}
	else if(sol&COLFLAG_ROOFSLOPE_LEFT) {
		//return ((31-x%32)> y%32);
		return ((31-x%32) > y%32 ? SS_COL : ((31-x%32) == y%32 ? SS_COL_HL : SS_NOCOL));
	}
	else if(sol&COLFLAG_ROOFSLOPE_RIGHT) {
		return (x%32 > y%32 ? SS_COL : (x%32 == y%32 ? SS_COL_HR : SS_NOCOL));
	}
	else
		return 0;
	//return GetTile(x, y)&COLFLAG_SOLI
}


bool CCollision::IsTileSolid(int x, int y, bool IncludeDeath)
{
	/*
	int t = GetTile(x, y);
	if (IncludeDeath && GetTile(x, y)&COLFLAG_DEATH)
		return true;
	
	return GetTile(x, y)&COLFLAG_SOLID;
	*/
	
	return SolidState(x, y) != SS_NOCOL;
}



int CCollision::GetRayPoint(int x, int y)
{
	int Nx = GetModularPos(x);
	Nx = clamp(Nx, 0, m_Width-1);
	int Ny = clamp(y, 0, m_Height-1);

	if (m_pTiles[Ny*m_Width+Nx].m_Index == ENTITY_SAWBLADE + ENTITY_OFFSET)
		return COLFLAG_SOLID;
	
	if (m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVELEFT || m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_MOVERIGHT)
		return COLFLAG_SOLID;
	
	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
	
	return 0;
}



int CCollision::IsInFluid(float x, float y)
{
	if (m_GlobalAcid && y > GetGlobalAcidLevel())
		return true;
	
	return GetTile(round_to_int(x), round_to_int(y)) == CCollision::COLFLAG_DAMAGEFLUID;
}

int CCollision::FastIntersectLine(vec2 Pos0, vec2 Pos1)
{
	const float Distance = distance(Pos0, Pos1);
	if(Distance <= 0.0f)
		return CheckPoint(Pos0.x, Pos0.y) ? GetCollisionAt(Pos0.x, Pos0.y) : 0;
	const float InvDistance = 1.0f / Distance;
	const int End = int(Distance + 1.0f);

	for(int i = 0; i < End; i++)
	{
		const float a = i * InvDistance;
		const vec2 Pos = mix(Pos0, Pos1, a);
		if(CheckPoint(Pos.x, Pos.y))
			return GetCollisionAt(Pos.x, Pos.y);
	}
	return 0;
}


// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, bool IncludeDeath, bool IncludePlatforms, bool IncludeBlocks)
{
	const float Distance = distance(Pos0, Pos1);
	if(Distance <= 0.0f)
	{
		if(pOutCollision)
			*pOutCollision = Pos0;
		if(pOutBeforeCollision)
			*pOutBeforeCollision = Pos0;
		return CheckPoint(Pos0.x, Pos0.y, IncludeDeath, !IncludePlatforms, IncludeBlocks) ?
			GetCollisionAt(Pos0.x, Pos0.y, !IncludePlatforms, IncludeBlocks) : 0;
	}
	const float InvDistance = 1.0f / Distance;
	const int End = int(Distance + 1.0f);
	vec2 Last = Pos0;

	for(int i = 0; i < End; i++)
	{
		const float a = i * InvDistance;
		const vec2 Pos = mix(Pos0, Pos1, a);
		if(CheckPoint(Pos.x, Pos.y, IncludeDeath, !IncludePlatforms, IncludeBlocks))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(Pos.x, Pos.y, !IncludePlatforms, IncludeBlocks);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

bool CCollision::IntersectBlocks(vec2 Pos0, vec2 Pos1)
{
	const float Distance = distance(Pos0, Pos1);
	if(Distance <= 0.0f)
		return GetBlock(Pos0.x, Pos0.y);
	const float InvDistance = 1.0f / Distance;
	const int End = int(Distance + 1.0f);

	for(int i = 0; i < End; i++)
	{
		const float a = i * InvDistance;
		const vec2 Pos = mix(Pos0, Pos1, a);
		if(GetBlock(Pos.x, Pos.y))
		{
			return true;
		}
	}
	
	return false;
}

// TODO: OPT: rewrite this smarter!
bool CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces, bool IgnoreCollision)
{
	bool Bounced = false;
	
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(!IgnoreCollision && CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
			
			Bounced = true;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
			
			Bounced = true;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
	
	return Bounced;
}

int CCollision::TestBox(vec2 Pos, vec2 Size, bool Down)
{
	Size *= 0.5f;
	int r;
	for(int x = 0; x <= Size.x; x++) {
		if( (r = CheckPoint(Pos.x+x, Pos.y-Size.y, false, true)) )
			return r;
		if( (r = CheckPoint(Pos.x+x, Pos.y+Size.y, false, Down)) )
			return r;
		
		if( (r = CheckPoint(Pos.x-x, Pos.y-Size.y, false, true)) )
			return r;
		if( (r = CheckPoint(Pos.x-x, Pos.y+Size.y, false, Down)) )
			return r;
	}
	
	for(int y = 0; y <= Size.y; y++) {
		int r;
		if( (r = CheckPoint(Pos.x-Size.x, Pos.y+y, false, true)) )
			return r;
		if( (r = CheckPoint(Pos.x+Size.x, Pos.y+y, false, true)) )
			return r;
		
		if( (r = CheckPoint(Pos.x-Size.x, Pos.y-y, false, true)) )
			return r;
		if( (r = CheckPoint(Pos.x+Size.x, Pos.y-y, false, true)) )
			return r;
	}
			
	/*if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y))
		return true;*/
	return 0;
}



float VectorDotProduct(vec2 v1, vec2 v2)
{
	return v1.x * v2.x + v1.y * v2.y;
}

vec2 CCollision::Reflect(vec2 v, vec2 n)
{
    return v - n * 2.0f * VectorDotProduct(v, n);
}


vec2 CCollision::WallReflect(vec2 Pos, vec2 Direction, int Collision)
{
		if (Collision == COLFLAG_RAMP_LEFT)
			return Reflect(Direction, normalize(vec2(1, -1)));
		else if (Collision == COLFLAG_RAMP_RIGHT)
			return Reflect(Direction, normalize(vec2(-1, -1)));
		else if (Collision == COLFLAG_ROOFSLOPE_LEFT)
			return Reflect(Direction, normalize(vec2(1, 1)));
		else if (Collision == COLFLAG_ROOFSLOPE_RIGHT)
			return Reflect(Direction, normalize(vec2(-1, 1)));
		else
		{
			if (!GetCollisionAt(Pos.x, Pos.y-8) || !GetCollisionAt(Pos.x, Pos.y+8))
				return Reflect(Direction, normalize(vec2(0, -1)));
			else
				return Reflect(Direction, normalize(vec2(-1, 0)));
		}
}


void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool check_speed, bool Down)
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	
	if (Vel.y < 0.0f)
		Down = true;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice
			int rr = TestBox(vec2(NewPos.x, NewPos.y), Size, Down);
			
			/*if (rr == SS_COL_RL || rr == SS_COL_RR) {
				std::cerr << "COL: " << rr << std::endl;
				int r = 0;
				if(rr == SS_COL_RL) {
					//Vel.x *= invsqrt2;
					Vel.y = Vel.x;
				}
				else if(rr == SS_COL_RR) {
					//Vel.x *= invsqrt2;
					Vel.y = -Vel.x;
				}
				//NewPos = Pos;
			}
			else*/ if(rr)
			{
				int Hits = 0;
				int r = 0;
				
				if( (r = TestBox(vec2(Pos.x, NewPos.y), Size, Down)) )
				{
					//bool taken_care = false;
					NewPos.y = Pos.y;
					if(r == SS_COL_RR && Vel.x >= -Vel.y && (!check_speed || fabs(Vel.x) > 0.005f)) {
						float new_force = Vel.x * invsqrt2 - Vel.y * invsqrt2; 
						//if(new_force/Distance < 0.95f) {
							Vel.y = -new_force * invsqrt2;
							Vel.x = new_force  * invsqrt2;
							//std::cerr << "C1 " << new_force/Distance << std::endl;
							//taken_care = true;
						//}
					}
					else if(r == SS_COL_RL && Vel.x <= Vel.y && (!check_speed || fabs(Vel.x) > 0.005f)) {
						float new_force = -Vel.x * invsqrt2 - Vel.y * invsqrt2;
						//std::cerr << "C2pre " << Vel.x << ", " << check_speed << std::endl;
						//if(new_force/Distance < 0.95f) {
							Vel.y = -new_force * invsqrt2;
							Vel.x = -new_force * invsqrt2;
							//std::cerr << "C2 " << new_force/Distance << std::endl;
							//taken_care = true;
						//}
					}
					else
						Vel.y *= -Elasticity;
					Hits++;
					//Vel.y *= -Elasticity;
					//NewPos.y = Pos.y;
				}

				if( (r = TestBox(vec2(NewPos.x, Pos.y), Size, Down)) )
				{
					
					/*bool climbing = false;
					//std::cerr << "Oh" << std::endl;
					for(int y = 1; y <= 2; y++) {
						if(!TestBox(vec2(NewPos.x, NewPos.y-y), Size)) {
							//std::cerr << "WUI " << y << std::endl;
							NewPos = vec2(NewPos.x, NewPos.y-y);
							climbing = true;
							break;
						}
					}
					if(!climbing) {*/
					//bool taken_care = false;
					NewPos.x = Pos.x;
					if(r == SS_COL_RR && Vel.x >= -Vel.y && (!check_speed || fabs(Vel.x) > 0.005f)) {
						float new_force = Vel.x * invsqrt2 - Vel.y * invsqrt2; 
						//if(new_force/Distance < 0.95f) {
							Vel.y = -new_force * invsqrt2;
							Vel.x = new_force  * invsqrt2;
							//std::cerr << "D1 " << new_force/Distance << std::endl;
							//taken_care = true;
						//}
					}
					else if(r == SS_COL_RL && Vel.x <= Vel.y && (!check_speed || fabs(Vel.x) > 0.005f)) {
						float new_force = -Vel.x * invsqrt2 - Vel.y * invsqrt2;
						//if(new_force/Distance < 0.95f) {
							Vel.y = -new_force * invsqrt2;
							Vel.x = -new_force * invsqrt2;
							//std::cerr << "D2 " << new_force/Distance << std::endl;
							//taken_care = true;
						//}
					}
					else
						Vel.x *= -Elasticity;
							
					//} else {
						//Vel.x *= 0.85f;
						//float newvely = -abs(Vel.x/2.0f);
						
						//if(Vel.y > newvely)
						//	Vel.y = newvely;
					//}
					
					Hits++;
					//Vel.x *= -Elasticity;
					//NewPos.x = Pos.x;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}
			/*else if(rr != 0) {
				NewPos.y = Pos.y;
				Vel.y *= -Elasticity;
				NewPos.x = Pos.x;
				Vel.x *= -Elasticity;
			}*/
			
			Pos = NewPos;
		}
	}
	
	/*if(Vel.y >= 0) {
		bool was_hitting = false;
		for(int y = 3; y >= 0; y--) {
			bool hitting = TestBox(vec2(Pos.x, Pos.y+y), Size);
			if(!hitting && was_hitting) {
				Pos.y+=y;
				break;
			}
			else if (hitting) {
				was_hitting = true;
			}
		}
	}*/

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

bool CCollision::ClearTileLayer(int group, int layer)
{
	const unsigned CacheHash = static_cast<unsigned>(group) * 31u + static_cast<unsigned>(layer);
	const int CacheIndex = CacheHash & (MODIF_TILE_CACHE_SIZE - 1);
	if(m_aModifTileGroup[CacheIndex] != group || m_aModifTileLayer[CacheIndex] != layer)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(group);
		CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer + layer);
		m_aModifTileGroup[CacheIndex] = group;
		m_aModifTileLayer[CacheIndex] = layer;
		m_apModifTilemap[CacheIndex] = pLayer->m_Type == LAYERTYPE_TILES ? reinterpret_cast<CMapItemLayerTilemap *>(pLayer) : 0;
		m_apModifTiles[CacheIndex] = !m_apModifTilemap[CacheIndex] ? 0 : m_apModifTilemap[CacheIndex] == m_pLayers->GameLayer() ?
			m_pTiles : static_cast<CTile *>(m_pLayers->Map()->GetData(m_apModifTilemap[CacheIndex]->m_Data));
	}

	CMapItemLayerTilemap *pTilemap = m_apModifTilemap[CacheIndex];
	if(!pTilemap || pTilemap->m_Width != m_Width || pTilemap->m_Height != m_Height)
		return false;

	mem_zero(m_apModifTiles[CacheIndex], m_Width * m_Height * sizeof(CTile));
	return true;
}


bool CCollision::ModifTile(ivec2 pos, int group, int layer, int tile, int flags, int reserved)
{
	const unsigned CacheHash = static_cast<unsigned>(group) * 31u + static_cast<unsigned>(layer);
	const int CacheIndex = CacheHash & (MODIF_TILE_CACHE_SIZE - 1);
	if(m_aModifTileGroup[CacheIndex] != group || m_aModifTileLayer[CacheIndex] != layer)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(group);
		CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+layer);
		m_aModifTileGroup[CacheIndex] = group;
		m_aModifTileLayer[CacheIndex] = layer;
		m_apModifTilemap[CacheIndex] = pLayer->m_Type == LAYERTYPE_TILES ? reinterpret_cast<CMapItemLayerTilemap *>(pLayer) : 0;
		m_apModifTiles[CacheIndex] = !m_apModifTilemap[CacheIndex] ? 0 : m_apModifTilemap[CacheIndex] == m_pLayers->GameLayer() ?
			m_pTiles : static_cast<CTile *>(m_pLayers->Map()->GetData(m_apModifTilemap[CacheIndex]->m_Data));
	}
	if(!m_apModifTilemap[CacheIndex])
		return false;

	CMapItemLayerTilemap *pTilemap = m_apModifTilemap[CacheIndex];
    int TotalTiles = pTilemap->m_Width*pTilemap->m_Height;
    int tpos = (int)pos.y*pTilemap->m_Width+(int)pos.x;
    if (tpos < 0 || tpos >= TotalTiles)
        return false;


    if (pTilemap != m_pLayers->GameLayer())
    {
        CTile *pTiles = m_apModifTiles[CacheIndex];
        pTiles[tpos].m_Flags = flags;
        pTiles[tpos].m_Index = tile;
        pTiles[tpos].m_Reserved = reserved;
    }
    else
    {
        m_pTiles[tpos].m_Index = tile;
        m_pTiles[tpos].m_Flags = flags;
        m_pTiles[tpos].m_Reserved = reserved;

        switch(tile)
        {
        case TILE_DEATH:
            m_pTiles[tpos].m_Index = COLFLAG_DEATH;
            break;
        case TILE_SOLID:
            m_pTiles[tpos].m_Index = COLFLAG_SOLID;
            break;
        case TILE_DAMAGEFLUID:
            m_pTiles[tpos].m_Index = COLFLAG_DAMAGEFLUID;
            break;
        case TILE_MOVELEFT:
            m_pTiles[tpos].m_Index = COLFLAG_MOVELEFT;
            break;
        case TILE_MOVERIGHT:
            m_pTiles[tpos].m_Index = COLFLAG_MOVERIGHT;
            break;
        case TILE_RAMP_LEFT:
            m_pTiles[tpos].m_Index = COLFLAG_RAMP_LEFT;
            break;
        case TILE_RAMP_RIGHT:
            m_pTiles[tpos].m_Index = COLFLAG_RAMP_RIGHT;
            break;
		case TILE_ROOFSLOPE_LEFT:
			m_pTiles[tpos].m_Index = COLFLAG_ROOFSLOPE_LEFT;
			break;
		case TILE_ROOFSLOPE_RIGHT:
			m_pTiles[tpos].m_Index = COLFLAG_ROOFSLOPE_RIGHT;
			break;
        case TILE_HANG:
            m_pTiles[tpos].m_Index = COLFLAG_HANG;
            break;
        case TILE_PLATFORM:
            m_pTiles[tpos].m_Index = COLFLAG_PLATFORM;
            break;
        default:
            if(tile <= 128)
                m_pTiles[tpos].m_Index = 0;
        }
    }

    return true;
}

// Set here for CMake building
void CWaypoint::SetCenter(int Distance)
{
	// set self's distance
	m_PathDistance = Distance;
	
	// set connections' distance
	for (int i = 0; i < m_ConnectionCount; i++)
	{
		if (m_apConnection[i])
		{
			if (m_apConnection[i]->m_PathDistance == 0)
			{
				m_apConnection[i]->m_PathDistance = Distance + m_aDistance[i];
			}
		}
	}
	
	// visit connections
  	for (int i = 0; i < m_ConnectionCount; i++)
	{
		if (m_apConnection[i])
		{
			if (m_apConnection[i]->m_PathDistance >= Distance + m_aDistance[i])
			{
				m_apConnection[i]->SetCenter(Distance + m_aDistance[i]);
			}
		}
   }
}
