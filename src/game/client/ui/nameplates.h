

#ifndef GAME_CLIENT_UI_NAMEPLATES_H
#define GAME_CLIENT_UI_NAMEPLATES_H
#include <game/client/core/component.h>

class CNamePlates : public CComponent
{
	void RenderNameplate(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CNetObj_PlayerInfo *pPlayerInfo
	);

public:
	virtual void OnRender();
};

#endif
