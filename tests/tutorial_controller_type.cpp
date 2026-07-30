#include <type_traits>

#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <game/server/gamemodes/tutorial.h>
#include <game/server/gamemodes/invasion.h>

static_assert(std::is_base_of<IGameController, CGameControllerTutorial>::value,
			  "tutorial controller must use the common controller interface");
static_assert(!std::is_base_of<CGameControllerInvasion, CGameControllerTutorial>::value,
			  "tutorial controller must never inherit Invasion flow");

int main()
{
	return 0;
}
