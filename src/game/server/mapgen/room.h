#ifndef GAME_SERVER_MAPGEN_ROOM_H
#define GAME_SERVER_MAPGEN_ROOM_H

class CRoom
{
private:
	CRoom *m_pChild1, *m_pChild2;
	int m_X, m_Y, m_W, m_H;
	
	bool m_Open;
	
public:
	CRoom(int x, int y, int w, int h);
	~CRoom();
	
	int MinSize() const;
	bool TooSmall() const;
	
	bool Open(int x, int y);
	
	void Split(bool Vertical);
	
	void Generate(class CGenLayer *pTiles);
	
	void Fill(class CGenLayer *pTiles, int Index, int x, int y, int w, int h);
};


#endif
