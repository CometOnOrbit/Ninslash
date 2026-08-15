#include <game/client/menu_home.h>

#include <cstdio>

static bool Expect(bool Condition, const char *pMessage)
{
	if(!Condition)
		std::fprintf(stderr, "menu home: %s\n", pMessage);
	return Condition;
}

int main()
{
	bool Ok = true;
	Ok &= Expect(ResolveMenuHomePrimary({false, true, false, true, 3}).m_Action == MENU_HOME_JOIN_LOCAL,
				 "running server wins over tutorial");
	Ok &= Expect(ResolveMenuHomePrimary({false, true, true, true, 3}).m_Action == MENU_HOME_SHOW_LOCAL,
				 "connected server continues");
	Ok &= Expect(ResolveMenuHomePrimary({true, false, false, true, 2}).m_Action == MENU_HOME_SHOW_LOCAL,
				 "starting server wins over tutorial");
	const CMenuHomePrimary Tutorial = ResolveMenuHomePrimary({false, false, false, true, 9});
	Ok &= Expect(Tutorial.m_Action == MENU_HOME_CONTINUE_TUTORIAL && Tutorial.m_Chapter == 6,
				 "tutorial chapter is sanitized");
	Ok &= Expect(ResolveMenuHomePrimary({false, false, false, false, 0}).m_Action == MENU_HOME_EXPEDITION,
				 "idle state opens expedition");
	Ok &= Expect(ResolveMenuHomePrimary({true, true, false, false, 0}).m_Action == MENU_HOME_JOIN_LOCAL,
				 "running state is stable during exceptional transition");
	return Ok ? 0 : 1;
}
