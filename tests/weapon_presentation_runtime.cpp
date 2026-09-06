#include <game/weapons/weapon_presentation_runtime.h>

#include <assert.h>
#include <string.h>

class CHost : public IWeaponPresentationHost
{
  public:
	int m_aState[8]{};
	int m_TextCount = 0;
	int m_BarCount = 0;

	int PresentationStateGet(int Index) const override { return m_aState[Index]; }

	void PresentationText(const char *, int, int, int) override { ++m_TextCount; }

	void PresentationBar(int, int, int, int, int, int) override { ++m_BarCount; }
};

int main()
{
	char aError[256];
	CWeaponPresentationRuntime::Reset();
	const char *pScript = "presentation.on_hud('workshop:7:ui',function(ctx) "
						  "ctx:text('Charge',1,2,12);ctx:bar(ctx:state(0),3,1,2,3,4) end)";
	assert(CWeaponPresentationRuntime::LoadPackageScript(
		"7", "ui.weapon_presentation.lua", pScript, (int)strlen(pScript), aError, sizeof(aError)));

	CHost Host;
	Host.m_aState[0] = 2;
	assert(CWeaponPresentationRuntime::RenderHud("workshop:7:ui", &Host, aError, sizeof(aError)));
	assert(Host.m_TextCount == 1 && Host.m_BarCount == 1);

	CWeaponPresentationRuntime::Reset();
	return 0;
}
