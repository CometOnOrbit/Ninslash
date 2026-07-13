#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/shared/config.h>
#include <game/questinfo.h>

#include "room.h"
#include "maze.h"


CMaze::CMaze(int w, int h)
{
	m_W = w;
	m_H = h;
	
	m_aOpen = new bool[w * h];
	m_aConnected = new bool[w * h];
	
	for (int i = 0; i < w*h; i++)
	{
		m_aOpen[i] = false;
		m_aConnected[i] = false;
	}
	
	Generate();
}

CMaze::~CMaze()
{
	if (m_aOpen)
		delete[] m_aOpen;
	
	if (m_aConnected)
		delete[] m_aConnected;
}


void CMaze::Generate()
{
	m_Rooms = 0;

	// invasion / horde / extract coop-style layouts
	if (IsCoopMapGenGametype(g_Config.m_SvGametype))
	{
		int Level = g_Config.m_SvMapGenLevel;
		const int Theme = InvasionThemeFromLevel(Level);

		// Extraction: tighter snake-like maze with fewer wide shortcuts
		if (str_comp(g_Config.m_SvGametype, "extract") == 0)
		{
			const int Cols = 3;
			const int Rows = 3;
			for(int row = 0; row < Rows; row++)
			{
				for(int col = 0; col < Cols; col++)
				{
					float fx = 0.13f + (col + 0.5f) / Cols * 0.72f + (frandom()-frandom())*0.025f;
					float fy = 0.15f + (row + 0.5f) / Rows * 0.66f + (frandom()-frandom())*0.03f;
					m_aRoom[m_Rooms++] = vec2(m_W*fx, m_H*fy);
				}
			}

			// carve a single snake spine first so runs feel tighter and more directed
			for(int row = 0; row < Rows; row++)
			{
				const int RowStart = row * Cols;
				if((row & 1) == 0)
				{
					for(int col = 0; col + 1 < Cols; col++)
						Connect(m_aRoom[RowStart+col], m_aRoom[RowStart+col+1]);
				}
				else
				{
					for(int col = Cols - 1; col > 0; col--)
						Connect(m_aRoom[RowStart+col], m_aRoom[RowStart+col-1]);
				}

				if(row + 1 < Rows)
				{
					const int ExitCol = (row & 1) == 0 ? Cols - 1 : 0;
					Connect(m_aRoom[RowStart+ExitCol], m_aRoom[RowStart+Cols+ExitCol]);

					// keep one occasional vertical bypass, but much rarer than before
					if(frandom() < 0.18f)
					{
						const int MidCol = 1;
						Connect(m_aRoom[RowStart+MidCol], m_aRoom[RowStart+Cols+MidCol]);
					}
				}
			}

			// a few side pockets keep the maze readable without opening huge arenas
			const int Extra = 2 + rand() % 3;
			for(int i = 0; i < Extra; i++)
				GenerateRoom(true);

			for(int i = 0; i < 1; i++)
				ConnectRandomRooms();
			ConnectEverything();
			return;
		}

		// Acid-escape floors: vertical rising-acid escape tower (Invasion only)
		if (str_comp(g_Config.m_SvGametype, "coop") == 0 && Theme == INVASION_THEME_ACID_ESCAPE)
		{
			const int Floors = 7 + min(5, Level/15);
			const float yTop = 0.12f; // door / exit (up)
			const float yBot = 0.88f; // spawn (down)

			m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*yBot);
			m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*yTop);

			for (int i = 0; i < Floors; i++)
			{
				float t = (Floors <= 1) ? 0.0f : i / float(Floors-1);
				float y = yBot + (yTop - yBot) * t;
				float half = 0.10f + frandom()*0.06f;
				float cx = 0.50f + (frandom()-frandom())*0.12f;
				Connect(vec2(m_W*(cx-half), m_H*y), vec2(m_W*(cx+half), m_H*y));

				if (i > 0)
				{
					float yPrev = yBot + (yTop - yBot) * ((i-1) / float(Floors-1));
					float xv = cx + (frandom()-frandom())*0.08f;
					Connect(vec2(m_W*xv, m_H*yPrev), vec2(m_W*xv, m_H*y));
				}
			}

			// bottom alcove for the switch
			Connect(vec2(m_W*0.22f, m_H*(yBot-0.04f)), vec2(m_W*0.42f, m_H*(yBot-0.04f)));
			Connect(vec2(m_W*0.32f, m_H*(yBot-0.04f)), vec2(m_W*0.32f, m_H*yBot));

			// light side branches (keep tower readable)
			for (int i = 1; i < Floors-1; i += 2)
			{
				float t = i / float(Floors-1);
				float y = yBot + (yTop - yBot) * t;
				float side = (i%4==1) ? -1.0f : 1.0f;
				Connect(vec2(m_W*(0.5f), m_H*y), vec2(m_W*(0.5f+side*0.22f), m_H*y));
			}

			for (int i = 0; i < 2; i++)
				GenerateRoom();

			ConnectEverything();
			return;
		}

		// Purge arena (compact)
		if (Theme == INVASION_THEME_PURGE)
		{
			float s = 0.14f+frandom()*0.08f;
			Connect(vec2(m_W*(0.5f-s), m_H*0.5f), vec2(m_W*(0.5f+s), m_H*0.5f));
			Connect(vec2(m_W*(0.5f-s), m_H*(0.5f-s)), vec2(m_W*(0.5f+s), m_H*(0.5f-s)));
			Connect(vec2(m_W*(0.5f-s), m_H*(0.5f+s)), vec2(m_W*(0.5f+s), m_H*(0.5f+s)));
			Connect(vec2(m_W*(0.5f-s), m_H*(0.5f-s)), vec2(m_W*(0.5f-s), m_H*(0.5f+s)));
			Connect(vec2(m_W*(0.5f+s), m_H*(0.5f-s)), vec2(m_W*(0.5f+s), m_H*(0.5f+s)));
			for (int i = 0; i < min(8, Level/2); i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// Standard wave lanes
		if (Theme == INVASION_THEME_STANDARD_WAVE)
		{
			m_aRoom[m_Rooms++] = vec2(m_W*0.35f, m_H*0.5f);
			m_aRoom[m_Rooms++] = vec2(m_W*0.65f, m_H*0.5f);
			m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*0.35f);
			Connect(m_aRoom[0], m_aRoom[1]);
			Connect(m_aRoom[0], m_aRoom[2]);
			Connect(m_aRoom[1], m_aRoom[2]);
			for (int i = 0; i < min(12, Level/2); i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// Boss ring layout
		if (Theme == INVASION_THEME_BOSS_ASSAULT)
		{
			int r = min(20, Level/3);
			float s = 0.12f+frandom()*0.15f;
			float sy = 0.4f+frandom()*0.15f;
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f-s), m_H*(0.5f+s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f), m_H*(0.5f+s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f), m_H*(0.5f-s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f-s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f), m_H*(0.5f));
			for (int i = 0; i < m_Rooms - 1; i++)
				Connect(m_aRoom[i], m_aRoom[i+1]);
			for (int i = 0; i < r; i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// Dual-switch branch layout
		if (Theme == INVASION_THEME_DUAL_SWITCHES)
		{
			int r = min(20, Level/3);
			float s = 0.11f+frandom()*0.15f;
			float sy = 0.4f+frandom()*0.15f;
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f-s), m_H*(0.5f-s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f-s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f), m_H*(0.5f));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f), m_H*(0.5f+s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s*2), m_H*(0.5f+s*sy));
			for (int i = 0; i < m_Rooms - 1; i++)
				Connect(m_aRoom[i], m_aRoom[i+1]);
			for (int i = 0; i < r; i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// Timed survive — narrow platforms
		if (Theme == INVASION_THEME_TIMED_SURVIVE)
		{
			float s = 0.08f+frandom()*0.05f;
			for (int i = 0; i < 4; i++)
			{
				float y = 0.25f + i*0.15f;
				Connect(vec2(m_W*(0.5f-s), m_H*y), vec2(m_W*(0.5f+s), m_H*y));
			}
			for (int i = 0; i < 3; i++)
			{
				float y1 = 0.25f + i*0.15f;
				float y2 = 0.25f + (i+1)*0.15f;
				Connect(vec2(m_W*0.5f, m_H*y1), vec2(m_W*0.5f, m_H*y2));
			}
			for (int i = 0; i < min(8, Level/3); i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// Trap / W layout
		if (Theme == INVASION_THEME_TRAP_RUN)
		{
			int r = min(14, Level/3);
			float s = 0.12f+frandom()*0.15f;
			float sy = 0.4f+frandom()*0.15f;
			Connect(vec2(m_W*(0.3f-s), m_H*(0.5f+s*sy)), vec2(m_W*(0.5f+s), m_H*(0.5f+s*sy)));
			Connect(vec2(m_W*(0.5f+s), m_H*(0.5f+s*sy)), vec2(m_W*(0.5f+s), m_H*(0.5f)));
			Connect(vec2(m_W*(0.5f-s), m_H*(0.5f)), vec2(m_W*(0.5f+s), m_H*(0.5f)));
			Connect(vec2(m_W*(0.5f-s), m_H*(0.5f)), vec2(m_W*(0.5f-s), m_H*(0.5f-s*sy)));
			Connect(vec2(m_W*(0.5f-s), m_H*(0.5f-s*sy)), vec2(m_W*(0.5f+s), m_H*(0.5f-s*sy)));
			Connect(vec2(m_W*(0.5f), m_H*(0.5f-s*sy)), vec2(m_W*(0.5f), m_H*(0.5f-s*sy*2)));
			Connect(vec2(m_W*(0.5f), m_H*(0.5f-s*sy*2)), vec2(m_W*(0.8f+s), m_H*(0.5f-s*sy*2)));
			for (int i = 0; i < r; i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// Z terrain
		if (Theme == INVASION_THEME_Z_SECTOR)
		{
			int r = min(20, Level/3);
			float s = 0.12f+frandom()*0.15f;
			float sy = 0.4f+frandom()*0.15f;
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f-s), m_H*(0.5f+s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f+s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f-s), m_H*(0.5f));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f-s), m_H*(0.5f-s*sy));
			m_aRoom[m_Rooms++] = vec2(m_W*(0.5f+s), m_H*(0.5f-s*sy));
			for (int i = 0; i < m_Rooms - 1; i++)
				Connect(m_aRoom[i], m_aRoom[i+1]);
			for (int i = 0; i < r; i++)
				GenerateRoom();
			ConnectRooms();
			ConnectEverything();
			return;
		}

		// default coop layout
		m_aRoom[m_Rooms++] = vec2(m_W*0.4f, m_H*(0.05f+frandom()*0.8f));
		m_aRoom[m_Rooms++] = vec2(m_W*0.6f, m_aRoom[0].y);
		
		Connect(m_aRoom[0], m_aRoom[1]);
		
		int r = (Level < 15) ? min(Level/2 + 3, m_W*m_H / 600) : min(Level + 4, m_W*m_H / 600);
		
		for (int i = 0; i < r; i++)
			GenerateRoom(true);
		
		return;
	}
	else
	// ctf, tdm, dm, br
	{
		// dual way
		/*
		{
			m_aRoom[m_Rooms++] = vec2(m_W*0.1f, m_H*(0.2f + frandom()*0.6f));
			m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*0.15f);
			m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*0.85f);
			
			if (m_aRoom[0].y > m_H*0.5f)
			{
				m_aRoom[m_Rooms++] = vec2(m_W*0.1f, m_H*0.1f);
				Connect(m_aRoom[3], (m_aRoom[0]+m_aRoom[1])/2);
			}
			else
			{
				m_aRoom[m_Rooms++] = vec2(m_W*0.1f, m_H*0.9f);
				Connect(m_aRoom[3], (m_aRoom[0]+m_aRoom[2])/2);
			}
			
			Connect(m_aRoom[0], m_aRoom[1]);
			Connect(m_aRoom[0], m_aRoom[2]);
			
			if (frandom() < 0.5f)
				Connect(m_aRoom[1], m_aRoom[2]);
			
			if (frandom() < 0.5f)
				GenerateRoom(true, true);
			
			if (m_W > 200)
			{
				GenerateRoom(true, true);
				GenerateRoom(true, true);
			}
		}
		*/
		
		
		if (str_comp(g_Config.m_SvGametype, "dm") == 0)
		{
			// battle royale
			if (g_Config.m_SvSurvivalMode)
			{
				m_aRoom[m_Rooms++] = vec2(m_W*0.4f, m_H*0.6f);
				m_aRoom[m_Rooms++] = vec2(m_W*0.6f, m_H*0.6f);
				Connect(m_aRoom[0], m_aRoom[1]);
				Connect(m_aRoom[0], vec2(m_W*0.5f, m_H*0.1f));
				Connect(m_aRoom[1], vec2(m_W*0.5f, m_H*0.1f));
				
					
				for (int i = 0; i < 36; i++)
					GenerateRoom(true);
			}
			else
			{
				/*
				m_aRoom[m_Rooms++] = vec2(m_W*0.4f, m_H*0.6f);
				m_aRoom[m_Rooms++] = vec2(m_W*0.6f, m_H*0.6f);
				Connect(m_aRoom[0], m_aRoom[1]);
				*/
				
				m_aRoom[m_Rooms++] = vec2(m_W*(0.3f+frandom()*0.4f), m_H*(0.3f+frandom()*0.4f));
				
				for (int i = 0; i < (m_W*m_H)/2000; i++)
					GenerateRoom(true);
			}
		}
		else
		{
			m_aRoom[m_Rooms++] = vec2(m_W*0.1f, m_H*0.5f);
			m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*0.5f);
			Connect(m_aRoom[0], m_aRoom[1]);
			
			Connect(vec2(m_W*0.25f, m_H*0.2f), vec2(m_W*0.5f, m_H*0.2f));
			Connect(vec2(m_W*0.35f, m_H*0.8f), vec2(m_W*0.5f, m_H*0.8f));
			
			Connect(vec2(m_W*0.2f, m_H*0.5f), vec2(m_W*0.25f, m_H*0.2f));
			Connect(vec2(m_W*0.3f, m_H*0.5f), vec2(m_W*0.35f, m_H*0.8f));
			
			Connect(vec2(m_W*0.4f, m_H*0.5f), vec2(m_W*0.4f, m_H*0.2f));
			
			if (frandom() < 0.5f)
				Connect(vec2(m_W*0.5f, m_H*0.5f), vec2(m_W*0.5f, m_H*0.8f));
		}
		
		// cs test
	/*
		m_aRoom[m_Rooms++] = vec2(m_W*0.05f, m_H*0.5f);
		m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*0.5f);
		Connect(m_aRoom[0], m_aRoom[1]);
		
		m_aRoom[m_Rooms++] = vec2(m_W*0.1f, m_H*0.25f);
		m_aRoom[m_Rooms++] = vec2(m_W*0.5f, m_H*0.25f);
		Connect(m_aRoom[2], m_aRoom[3]);
		Connect(m_aRoom[1], m_aRoom[3]);
		
		m_aRoom[m_Rooms++] = vec2(m_W*0.1f, m_H*0.8f);
		Connect(m_aRoom[0], m_aRoom[4]);
		
		for (int i = 0; i < 15; i++)
			GenerateRoom(true, true);
		*/
	}
}


void CMaze::GenerateLinear(int Width, int Rooms)
{
	float y = 0.3f + frandom()*0.4f;
	Connect(vec2(m_W*0.5f-Width, m_H*y), vec2(m_W*0.5f+Width, m_H*y));
	
	if (Rooms > 0)
	{
		m_aRoom[m_Rooms++] = vec2(m_W*0.5f-Width*frandom(), m_H*y);
		m_aRoom[m_Rooms++] = vec2(m_W*0.5f+Width*frandom(), m_H*y);
		
		for (int i = 0; i < Rooms; i++)
			GenerateRoom();
	}
	
	ConnectEverything();
}



void CMaze::GenerateRoom(bool AutoConnect, bool MirrorMode)
{
	// find a free spot to the room
	
	bool Valid = false;
	int i = 0;
	
	while (!Valid && i++ < 2000)
	{
		Valid = true;
		vec2 p = vec2(2 + frandom()*(m_W-4), 2 + frandom()*(m_H-4));
		
		if (MirrorMode)
			p = vec2(2 + frandom()*(m_W*0.5f), 2 + frandom()*(m_H-4));
		
		if (m_Rooms > 0)
		{
			vec2 rp = vec2(-1, -1);
			
			float d = -1.0f;
			for (int r = 0; r < m_Rooms; r++)
				if (d < 0.0f || distance(vec2(p.x, p.y*2.25f), vec2(m_aRoom[r].x, m_aRoom[r].y*2.25f)) < d)
				{
					d = distance(vec2(p.x, p.y*2.25f), vec2(m_aRoom[r].x, m_aRoom[r].y*2.25f));
					rp = m_aRoom[r];
				}

			if (abs(p.x - rp.x) > 8 && abs(p.y - rp.y) > 8)
				Valid = false;
			
			if (d < 20.0f || d > 60.0f) // || d > 40.0f)
				Valid = false;
		}
				
		if (Valid)
		{
			if (AutoConnect)
				Connect(p, GetClosestRoom(p));
			
			m_aRoom[m_Rooms] = p;
			Open(m_aRoom[m_Rooms], 1 + rand()%4);
			
			//	Connect(p, m_aRoom[rand()%m_Rooms]);
			
			m_Rooms++;
			return;
		}
	}
}


void CMaze::ConnectRandomRooms()
{
	if (m_Rooms < 2)
		return;
	
	int r0 = rand()%(m_Rooms-1);
	int r1 = rand()%(m_Rooms-1);
	
	if (r0 != r1)
		Connect(m_aRoom[r0], m_aRoom[r1]);
}

	
void CMaze::ConnectRooms()
{
	if (m_Rooms < 2)
		return;
	
	//for (int r = 1; r < m_Rooms; r++)
	//	Connect(m_aRoom[r-1], m_aRoom[r]);
	
	// connect to closest room
	for (int r = 0; r < m_Rooms; r++)
	{
		vec2 Closest = vec2(-1000000, 0);
		
		for (int r2 = 0; r2 < m_Rooms; r2++)
			if (r2 != r)
				if (distance(m_aRoom[r], m_aRoom[r2]) < distance(m_aRoom[r], Closest))
					Closest = m_aRoom[r2];
		
		if (Closest.x >= 0.0f)
			Connect(m_aRoom[r], Closest);
	}
}


void CMaze::ConnectEverything()
{
	// find unconnected
	bool Looping = true;
	int i = 0;
	
	SetConnections(GetUnconnected());
	
	while (Looping && i++ < 1000)
	{
		ivec2 n = GetUnconnected();
		
		if (n.x <= 0)
			Looping = false;
		else
		{
			ivec2 np = GetClosestConnected(n);
			if (np.x > 0)
			{
				Connect(vec2(np.x, np.y), vec2(n.x, n.y));
				
				//for (int c = 0; c < m_W*m_H; c++)
				//	m_aConnected[c] = false;
			}

			SetConnections(n);
		}
	}
}


ivec2 CMaze::GetUnconnected()
{
	bool Looping = true;
	int i = 0;
	
	// check random spots
	while (Looping && i++ < 1000)
	{
		ivec2 p = ivec2(1+rand()%(m_W-2), 1+rand()%(m_H-2));
		if (m_aOpen[p.x + p.y*m_W] && !m_aConnected[p.x + p.y*m_W])
			return p;
	}
	
	// check everything if random failed
	for (int x = 1; x < m_W-1; x++)
		for (int y = 1; y < m_H-1; y++)
			if (m_aOpen[x + y*m_W] && !m_aConnected[x + y*m_W])
			 return ivec2(x, y);
		 
	return ivec2(-1, -1);
}


ivec2 CMaze::GetClosestConnected(ivec2 Pos)
{
	vec2 p0 = vec2(Pos.x, Pos.y);
	ivec2 Closest = ivec2(-1, -1);
	float d = 90000;

	for (int x = 1; x < m_W-1; x++)
		for (int y = 1; y < m_H-1; y++)
			if (m_aConnected[x + y*m_W] && m_aOpen[x + y*m_W])
				if (distance(p0, vec2(x, y)) < d)
				{
					Closest = ivec2(x, y);
					d = distance(p0, vec2(x, y));
				}
				
	return Closest;
}

vec2 CMaze::GetClosestRoom(vec2 Pos)
{
	if (m_Rooms < 1)
		return Pos;
	
	if (m_Rooms == 1)
		return m_aRoom[0];
	
	vec2 Closest = Pos;
	float ClosestDist = 90000;
	
	for (int i = 0; i < m_Rooms; i++)
	{
		float d = distance(m_aRoom[i], Pos);
		
		if (d < ClosestDist)
		{
			Closest = m_aRoom[i];
			ClosestDist = d;
		}
	}
	
	return Closest;
}


void CMaze::SetConnections(ivec2 Pos)
{
	if (Pos.x < 0 || Pos.y < 0 || Pos.x >= m_W || Pos.y >= m_H)
		return;
	
	if (m_aConnected[Pos.x + Pos.y*m_W] || !m_aOpen[Pos.x + Pos.y*m_W])
		return;
	
	m_aConnected[Pos.x + Pos.y*m_W] = true;
	
	SetConnections(Pos + ivec2(1, 0));
	SetConnections(Pos + ivec2(-1, 0));
	SetConnections(Pos + ivec2(0, 1));
	SetConnections(Pos + ivec2(0, -1));
}


void CMaze::Connect(vec2 Pos0, vec2 Pos1)
{
	float Distance = distance(Pos0, Pos1)*2;
	int End(Distance+1);
	const bool NarrowExtract = str_comp(g_Config.m_SvGametype, "extract") == 0;

	for(int i = 0; i < End; i++)
	{
		float a = i/Distance;
		vec2 Pos = mix(Pos0, Pos1, a);
		
		Open(Pos);
		if(!NarrowExtract)
		{
			Open(Pos+vec2(-1, -1));
			Open(Pos+vec2(1, -1));
			Open(Pos+vec2(1, 1));
			Open(Pos+vec2(-1, 1));
		}
	}
}


void CMaze::Open(vec2 Pos, int Size)
{
	Open(Pos.x, Pos.y);

	/*
	for (int x = -(Size-1); x < (Size-1); x++)
		for (int y = -(Size-1); y < (Size-1); y++)
			Open(Pos.x+x, Pos.y+y);
		*/
}

void CMaze::Open(int x, int y)
{
	// set pos within boundaries
	x = max(1, x);
	x = min(m_W-1, x);
	y = max(1, y);
	y = min(m_H-1, y);
	
	m_aOpen[x + y*m_W] = true;
}


void CMaze::OpenRooms(CRoom *pRoom)
{
	for (int x = 0; x < m_W; x++)
		for (int y = 0; y < m_H; y++)
			if (m_aOpen[x + y*m_W])
				pRoom->Open(x, y);
}
