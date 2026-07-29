#include <game/client/components/radar.h>
#include <game/server/entities/radar.h>

#include <type_traits>

// The client and embedded server are linked into the same executable. Server
// entities must not reuse client component class names or their weak destructor
// and vtable symbols can be coalesced by the linker.
static_assert(!std::is_same<CRadar, CServerRadar>::value, "Client and server radar types must remain distinct");

int main()
{
	return 0;
}
