#ifndef GAME_CLIENT_COMPONENTS_WEAPON_PRESENTATION_H
#define GAME_CLIENT_COMPONENTS_WEAPON_PRESENTATION_H

#include <game/client/component.h>
#include <game/weapon_presentation_runtime.h>

class CWeaponPresentation : public CComponent, public IWeaponPresentationHost
{
  public:
	void OnRender() override;
	int PresentationStateGet(int Index) const override;
	void PresentationText(const char *pText, int X, int Y, int Size) override;
	void PresentationBar(int Value, int Maximum, int X, int Y, int Width, int Height) override;

  private:
	const struct CNetObj_WeaponRuntime *m_pRuntime = 0;
};

#endif
