#ifndef GAME_SERVER_MAPGEN_ROOM_H
#define GAME_SERVER_MAPGEN_ROOM_H

class CRoomGenerated
{
	enum
	{
		ROOM_SPLIT = 0,
		ROOM_LEAF = 1,
	};

  private:
	CRoomGenerated *m_pChild1, *m_pChild2;
	int m_X, m_Y, m_W, m_H;

	bool m_Open;

	static CRoomGenerated *Join(int x, int y, int w, int h, CRoomGenerated *pA, CRoomGenerated *pB);

  public:
	CRoomGenerated(int x, int y, int w, int h, int Kind = ROOM_SPLIT);
	~CRoomGenerated();

	// One unsplit center leaf, normal BSP in the margins. Same rect the boss maze opens.
	static CRoomGenerated *CreateBoss(int RoomX, int RoomY, int RoomW, int RoomH);

	int MinSize() const;
	bool TooSmall() const;

	bool Open(int x, int y);

	void Split(bool Vertical);

	void Generate(class CGenLayer *pTiles);

	void Fill(class CGenLayer *pTiles, int Index, int x, int y, int w, int h);
};

inline bool BossArenaRect(int RoomX, int RoomY, int RoomW, int RoomH, int *pX, int *pY, int *pW, int *pH)
{
	if(RoomW < 32 || RoomH < 28)
		return false;
	int ArenaW = RoomW - 16;
	int ArenaH = RoomH - 16;
	if(ArenaW > 36)
		ArenaW = 36;
	if(ArenaH > 12)
		ArenaH = 12;
	if(ArenaW < 16 || ArenaH < 8)
		return false;
	*pX = RoomX + (RoomW - ArenaW) / 2;
	*pY = RoomY + (RoomH - ArenaH) / 2;
	*pW = ArenaW;
	*pH = ArenaH;
	return true;
}

#endif
