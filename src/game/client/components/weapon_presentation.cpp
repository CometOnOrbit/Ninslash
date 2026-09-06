#include "weapon_presentation.h"

#include <engine/graphics.h>
#include <generated/protocol.h>
#include <game/localization.h>
#include <game/weapons/weapon_catalog.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

int CWeaponPresentation::PresentationStateGet(int Index) const
{
	if(!m_pRuntime || Index < 0 || Index >= 8)
		return 0;
	const int *pState = &m_pRuntime->m_State0;
	return pState[Index];
}

void CWeaponPresentation::PresentationText(const char *pText, int X, int Y, int Size)
{
	if(!pText)
		return;
	CUIRect Rect = {(float)X, (float)Y, 320.0f, max(8.0f, (float)Size + 4.0f)};
	UI()->DoLabelScaled(&Rect, Localize(pText), clamp((float)Size, 8.0f, 32.0f), -1);
}

void CWeaponPresentation::PresentationBar(int Value, int Maximum, int X, int Y, int Width, int Height)
{
	if(Maximum <= 0 || Width <= 0 || Height <= 0)
		return;
	CUIRect Background = {(float)X, (float)Y, (float)min(Width, 500), (float)min(Height, 80)};
	Background.Draw(vec4(0.0f, 0.0f, 0.0f, 0.55f), 1.0f, CUI::CORNER_ALL);
	CUIRect Fill = Background;
	Fill.w *= clamp(Value / (float)Maximum, 0.0f, 1.0f);
	Fill.Draw(vec4(0.2f, 0.85f, 1.0f, 0.9f), 1.0f, CUI::CORNER_ALL);
}

void CWeaponPresentation::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;
	const int ClientId = m_pClient->m_Snap.m_LocalClientID;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	m_pRuntime = m_pClient->m_Snap.m_apWeaponRuntimes[ClientId];
	if(!m_pRuntime)
		return;
	CWeaponSpec Weapon;
	if(!CWeaponCatalog::TryFromProtocol(m_pRuntime->m_WeaponDefinitionId, m_pRuntime->m_WeaponLevel, &Weapon) ||
	   !CWeaponCatalog::IsCustom(Weapon))
		return;
	Graphics()->BlendNormal();
	UI()->ClipDisable();
	Graphics()->MapScreen(0.0f, 0.0f, 300.0f * Graphics()->ScreenAspect(), 300.0f);
	char aError[128];
	if(!CWeaponPresentationRuntime::RenderHud(CWeaponCatalog::StableId(Weapon), this, aError, sizeof(aError)))
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "weapon-presentation", aError);
	m_pRuntime = 0;
}
