#ifndef GAME_EDITOR_ROOM_H
#define GAME_EDITOR_ROOM_H

class CEditorRoom
{
  private:
	CEditorRoom *m_pChild1, *m_pChild2;
	int m_X, m_Y, m_W, m_H;

	bool m_Open;

  public:
	CEditorRoom(int x, int y, int w, int h);
	~CEditorRoom();

	bool TooSmall()
	{
		if(m_W < 5 || m_H < 5)
			return true;

		return false;
	}

	bool Open(int x, int y);

	void Split(bool Vertical);

	void Generate(class CLayerTiles *pLayer);

	void Fill(class CLayerTiles *pLayer, int Index, int x, int y, int w, int h);
};

#endif
