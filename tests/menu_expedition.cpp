#include <game/client/menu_expedition.h>

#include <cstdio>
#include <cstring>

static bool Expect(bool Condition, const char *pMessage)
{
	if(!Condition)
		std::fprintf(stderr, "menu expedition: %s\n", pMessage);
	return Condition;
}

int main()
{
	bool Ok = true;
	CExpeditionPlayer aPlayers[4];
	std::memset(aPlayers, 0, sizeof(aPlayers));
	std::strcpy(aPlayers[0].m_aName, "Ada");
	std::strcpy(aPlayers[1].m_aName, "Bao");
	std::strcpy(aPlayers[2].m_aName, "Cy");
	std::strcpy(aPlayers[3].m_aName, "Dot");

	CExpeditionSlotCard Empty;
	FillExpeditionSlotCard(2, EXPEDITION_LOAD_MISSING, 9, 2, aPlayers, &Empty);
	Ok &= Expect(Empty.m_Slot == 2 && !Empty.m_Occupied && !Empty.m_Corrupt, "empty slot");
	Ok &= Expect(Empty.m_Floor == 1 && Empty.m_NumPlayers == 0 && Empty.m_aPlayerLine[0] == 0, "empty has no squad");

	CExpeditionSlotCard Occupied;
	FillExpeditionSlotCard(1, EXPEDITION_LOAD_OK, 7, 2, aPlayers, &Occupied);
	Ok &= Expect(Occupied.m_Occupied && Occupied.m_Floor == 7 && Occupied.m_NumPlayers == 2, "occupied slot");
	Ok &= Expect(std::strcmp(Occupied.m_aPlayerLine, "Ada, Bao") == 0, "two names");

	CExpeditionSlotCard Many;
	FillExpeditionSlotCard(3, EXPEDITION_LOAD_OK, 12, 4, aPlayers, &Many);
	Ok &= Expect(std::strcmp(Many.m_aPlayerLine, "Ada, Bao, Cy +") == 0, "name list is capped");

	CExpeditionSlotCard Corrupt;
	FillExpeditionSlotCard(1, EXPEDITION_LOAD_CORRUPT, 4, 1, aPlayers, &Corrupt);
	Ok &= Expect(Corrupt.m_Corrupt && !Corrupt.m_Occupied, "corrupt is not playable");
	return Ok ? 0 : 1;
}
