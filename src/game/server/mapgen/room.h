#ifndef GAME_SERVER_MAPGEN_ROOM_H
#define GAME_SERVER_MAPGEN_ROOM_H

class CRoomGenerated
{
private:
	CRoomGenerated *m_pChild1, *m_pChild2;
	int m_X, m_Y, m_W, m_H;
	
	bool m_Open;
	
public:
	CRoomGenerated(int x, int y, int w, int h);
	~CRoomGenerated();
	
	int MinSize() const;
	bool TooSmall() const;
	
	bool Open(int x, int y);
	
	void Split(bool Vertical);
	
	void Generate(class CGenLayer *pTiles);
	
	void Fill(class CGenLayer *pTiles, int Index, int x, int y, int w, int h);
};


#endif
