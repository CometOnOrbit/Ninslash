

#include <math.h>

#include <engine/config.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include <generated/game_data.h>
#include <generated/protocol.h>

#include <game/localization.h>
#include <game/version.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/client/components/countryflags.h>

#include "menus.h"
#include "pve_roguelite.h"

static const char *UI_FILTER_PRESETS_FILE = "ui_filters.cfg";

void CMenus::SnapshotConfigToFilterPreset(int Slot)
{
	if(Slot < UI_FILTER_PRESET_CUSTOM_START || Slot >= NUM_UI_FILTER_PRESETS)
		return;

	CUiFilterPreset *pPreset = &m_aFilterPresets[Slot];
	str_copy(pPreset->m_aFilterString, g_Config.m_BrFilterString, sizeof(pPreset->m_aFilterString));
	pPreset->m_FilterFull = g_Config.m_BrFilterFull;
	pPreset->m_FilterEmpty = g_Config.m_BrFilterEmpty;
	pPreset->m_FilterSpectators = g_Config.m_BrFilterSpectators;
	pPreset->m_FilterFriends = g_Config.m_BrFilterFriends;
	pPreset->m_FilterCountry = g_Config.m_BrFilterCountry;
	pPreset->m_FilterCountryIndex = g_Config.m_BrFilterCountryIndex;
	pPreset->m_FilterPw = g_Config.m_BrFilterPw;
	pPreset->m_FilterPing = g_Config.m_BrFilterPing;
	str_copy(pPreset->m_aFilterGametype, g_Config.m_BrFilterGametype, sizeof(pPreset->m_aFilterGametype));
	pPreset->m_FilterGametypeStrict = g_Config.m_BrFilterGametypeStrict;
	str_copy(
		pPreset->m_aFilterServerAddress, g_Config.m_BrFilterServerAddress, sizeof(pPreset->m_aFilterServerAddress));
	pPreset->m_FilterPure = g_Config.m_BrFilterPure;
	pPreset->m_FilterPureMap = g_Config.m_BrFilterPureMap;
	pPreset->m_FilterCompatversion = g_Config.m_BrFilterCompatversion;
}

void CMenus::ApplyFilterPresetToConfig(int Slot)
{
	if(Slot == UI_FILTER_PRESET_ALL)
	{
		g_Config.m_BrFilterString[0] = 0;
		g_Config.m_BrFilterFull = 0;
		g_Config.m_BrFilterEmpty = 0;
		g_Config.m_BrFilterSpectators = 0;
		g_Config.m_BrFilterFriends = 0;
		g_Config.m_BrFilterCountry = 0;
		g_Config.m_BrFilterCountryIndex = -1;
		g_Config.m_BrFilterPw = 0;
		g_Config.m_BrFilterPing = 999;
		g_Config.m_BrFilterGametype[0] = 0;
		g_Config.m_BrFilterGametypeStrict = 0;
		g_Config.m_BrFilterServerAddress[0] = 0;
		g_Config.m_BrFilterPure = 1;
		g_Config.m_BrFilterPureMap = 1;
		g_Config.m_BrFilterCompatversion = 1;

		if(g_Config.m_UiPage == PAGE_FAVORITES)
		{
			g_Config.m_UiPage = PAGE_INTERNET;
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		}
	}
	else if(Slot == UI_FILTER_PRESET_FAVORITES)
	{
		g_Config.m_BrFilterString[0] = 0;
		g_Config.m_BrFilterFull = 0;
		g_Config.m_BrFilterEmpty = 0;
		g_Config.m_BrFilterSpectators = 0;
		g_Config.m_BrFilterFriends = 0;
		g_Config.m_BrFilterCountry = 0;
		g_Config.m_BrFilterCountryIndex = -1;
		g_Config.m_BrFilterPw = 0;
		g_Config.m_BrFilterPing = 999;
		g_Config.m_BrFilterGametype[0] = 0;
		g_Config.m_BrFilterGametypeStrict = 0;
		g_Config.m_BrFilterServerAddress[0] = 0;
		g_Config.m_BrFilterPure = 0;
		g_Config.m_BrFilterPureMap = 0;
		g_Config.m_BrFilterCompatversion = 0;
		g_Config.m_UiPage = PAGE_FAVORITES;
		ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
	}
	else if(Slot >= UI_FILTER_PRESET_CUSTOM_START && Slot < NUM_UI_FILTER_PRESETS && m_aFilterPresets[Slot].m_Used)
	{
		const CUiFilterPreset *pPreset = &m_aFilterPresets[Slot];
		str_copy(g_Config.m_BrFilterString, pPreset->m_aFilterString, sizeof(g_Config.m_BrFilterString));
		g_Config.m_BrFilterFull = pPreset->m_FilterFull;
		g_Config.m_BrFilterEmpty = pPreset->m_FilterEmpty;
		g_Config.m_BrFilterSpectators = pPreset->m_FilterSpectators;
		g_Config.m_BrFilterFriends = pPreset->m_FilterFriends;
		g_Config.m_BrFilterCountry = pPreset->m_FilterCountry;
		g_Config.m_BrFilterCountryIndex = pPreset->m_FilterCountryIndex;
		g_Config.m_BrFilterPw = pPreset->m_FilterPw;
		g_Config.m_BrFilterPing = pPreset->m_FilterPing;
		str_copy(g_Config.m_BrFilterGametype, pPreset->m_aFilterGametype, sizeof(g_Config.m_BrFilterGametype));
		g_Config.m_BrFilterGametypeStrict = pPreset->m_FilterGametypeStrict;
		str_copy(g_Config.m_BrFilterServerAddress,
				 pPreset->m_aFilterServerAddress,
				 sizeof(g_Config.m_BrFilterServerAddress));
		g_Config.m_BrFilterPure = pPreset->m_FilterPure;
		g_Config.m_BrFilterPureMap = pPreset->m_FilterPureMap;
		g_Config.m_BrFilterCompatversion = pPreset->m_FilterCompatversion;

		if(g_Config.m_UiPage == PAGE_FAVORITES)
		{
			g_Config.m_UiPage = PAGE_INTERNET;
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		}
	}

	Client()->ServerBrowserUpdate();
}

void CMenus::SwitchFilterPreset(int NewSlot)
{
	if(NewSlot < 0 || NewSlot >= NUM_UI_FILTER_PRESETS)
		return;
	if(NewSlot >= UI_FILTER_PRESET_CUSTOM_START && !m_aFilterPresets[NewSlot].m_Used)
		return;
	if(NewSlot == m_ActiveFilterPreset)
		return;

	if(m_ActiveFilterPreset >= UI_FILTER_PRESET_CUSTOM_START && m_aFilterPresets[m_ActiveFilterPreset].m_Used)
		SnapshotConfigToFilterPreset(m_ActiveFilterPreset);

	m_ActiveFilterPreset = NewSlot;
	m_FilterPresetRenameSlot = -1;
	ApplyFilterPresetToConfig(NewSlot);
	SaveFilterPresets();
}

void CMenus::LoadFilterPresets()
{
	str_copy(
		m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName, "All", sizeof(m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName));
	str_copy(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName,
			 "Favorites",
			 sizeof(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName));

	IOHANDLE File = Storage()->OpenFile(UI_FILTER_PRESETS_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;

	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#')
			continue;

		const char *pSep = str_find(pLine, "=");
		if(!pSep)
			continue;

		char aKey[64];
		int KeyLen = (int)(pSep - pLine);
		if(KeyLen <= 0 || KeyLen >= (int)sizeof(aKey))
			continue;
		mem_copy(aKey, pLine, KeyLen);
		aKey[KeyLen] = 0;
		const char *pValue = pSep + 1;

		if(str_comp(aKey, "active") == 0)
		{
			int Active = str_toint(pValue);
			if(Active >= 0 && Active < NUM_UI_FILTER_PRESETS)
				m_ActiveFilterPreset = Active;
			continue;
		}

		int Slot = -1;
		char aField[48];
		if(str_length(aKey) < 6 || str_comp_num(aKey, "slot", 4) != 0)
			continue;
		const char *pSlotStart = aKey + 4;
		const char *pUnderscore = str_find(pSlotStart, "_");
		if(!pUnderscore)
			continue;
		Slot = str_toint(pSlotStart);
		str_copy(aField, pUnderscore + 1, sizeof(aField));
		if(Slot < UI_FILTER_PRESET_CUSTOM_START || Slot >= NUM_UI_FILTER_PRESETS)
			continue;

		CUiFilterPreset *pPreset = &m_aFilterPresets[Slot];
		if(str_comp(aField, "used") == 0)
			pPreset->m_Used = str_toint(pValue) != 0;
		else if(str_comp(aField, "name") == 0)
			str_copy(pPreset->m_aName, pValue, sizeof(pPreset->m_aName));
		else if(str_comp(aField, "br_filter_string") == 0)
			str_copy(pPreset->m_aFilterString, pValue, sizeof(pPreset->m_aFilterString));
		else if(str_comp(aField, "br_filter_full") == 0)
			pPreset->m_FilterFull = str_toint(pValue);
		else if(str_comp(aField, "br_filter_empty") == 0)
			pPreset->m_FilterEmpty = str_toint(pValue);
		else if(str_comp(aField, "br_filter_spectators") == 0)
			pPreset->m_FilterSpectators = str_toint(pValue);
		else if(str_comp(aField, "br_filter_friends") == 0)
			pPreset->m_FilterFriends = str_toint(pValue);
		else if(str_comp(aField, "br_filter_country") == 0)
			pPreset->m_FilterCountry = str_toint(pValue);
		else if(str_comp(aField, "br_filter_country_index") == 0)
			pPreset->m_FilterCountryIndex = str_toint(pValue);
		else if(str_comp(aField, "br_filter_pw") == 0)
			pPreset->m_FilterPw = str_toint(pValue);
		else if(str_comp(aField, "br_filter_ping") == 0)
			pPreset->m_FilterPing = str_toint(pValue);
		else if(str_comp(aField, "br_filter_gametype") == 0)
			str_copy(pPreset->m_aFilterGametype, pValue, sizeof(pPreset->m_aFilterGametype));
		else if(str_comp(aField, "br_filter_gametype_strict") == 0)
			pPreset->m_FilterGametypeStrict = str_toint(pValue);
		else if(str_comp(aField, "br_filter_serveraddress") == 0)
			str_copy(pPreset->m_aFilterServerAddress, pValue, sizeof(pPreset->m_aFilterServerAddress));
		else if(str_comp(aField, "br_filter_pure") == 0)
			pPreset->m_FilterPure = str_toint(pValue);
		else if(str_comp(aField, "br_filter_pure_map") == 0)
			pPreset->m_FilterPureMap = str_toint(pValue);
		else if(str_comp(aField, "br_filter_compatversion") == 0)
			pPreset->m_FilterCompatversion = str_toint(pValue);
	}

	io_close(File);
}

void CMenus::SaveFilterPresets()
{
	IOHANDLE File = Storage()->OpenFile(UI_FILTER_PRESETS_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	char aBuf[512];
	io_write(File, "# UI filter presets\n", 20);
	str_format(aBuf, sizeof(aBuf), "active=%d\n", m_ActiveFilterPreset);
	io_write(File, aBuf, str_length(aBuf));

	for(int Slot = UI_FILTER_PRESET_CUSTOM_START; Slot < NUM_UI_FILTER_PRESETS; Slot++)
	{
		const CUiFilterPreset *pPreset = &m_aFilterPresets[Slot];
		if(!pPreset->m_Used)
			continue;

		str_format(aBuf, sizeof(aBuf), "slot%d_used=1\n", Slot);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_name=%s\n", Slot, pPreset->m_aName);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_string=%s\n", Slot, pPreset->m_aFilterString);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_full=%d\n", Slot, pPreset->m_FilterFull);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_empty=%d\n", Slot, pPreset->m_FilterEmpty);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_spectators=%d\n", Slot, pPreset->m_FilterSpectators);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_friends=%d\n", Slot, pPreset->m_FilterFriends);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_country=%d\n", Slot, pPreset->m_FilterCountry);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_country_index=%d\n", Slot, pPreset->m_FilterCountryIndex);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_pw=%d\n", Slot, pPreset->m_FilterPw);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_ping=%d\n", Slot, pPreset->m_FilterPing);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_gametype=%s\n", Slot, pPreset->m_aFilterGametype);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_gametype_strict=%d\n", Slot, pPreset->m_FilterGametypeStrict);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_serveraddress=%s\n", Slot, pPreset->m_aFilterServerAddress);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_pure=%d\n", Slot, pPreset->m_FilterPure);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_pure_map=%d\n", Slot, pPreset->m_FilterPureMap);
		io_write(File, aBuf, str_length(aBuf));
		str_format(aBuf, sizeof(aBuf), "slot%d_br_filter_compatversion=%d\n", Slot, pPreset->m_FilterCompatversion);
		io_write(File, aBuf, str_length(aBuf));
	}

	io_close(File);
}

void CMenus::RenderFilterPresetBar(CUIRect View)
{
	DrawMenuInset(&View, CUI::CORNER_ALL);
	View.Margin(1.0f, &View);

	const char *pAddText = Localize("Add");
	const char *pRemoveText = Localize("Remove");
	const char *pRenameText = Localize("Rename");
	const float ButtonFontSize = min((View.h - 2.0f) * ms_FontmodHeight, 14.0f);
	const float AddButtonWidth = max(34.0f, TextRender()->TextWidth(0, ButtonFontSize, pAddText, -1) + 14.0f);
	const float RemoveButtonWidth = max(34.0f, TextRender()->TextWidth(0, ButtonFontSize, pRemoveText, -1) + 14.0f);
	const float RenameButtonWidth = max(34.0f, TextRender()->TextWidth(0, ButtonFontSize, pRenameText, -1) + 14.0f);
	const float DesiredButtonsWidth = AddButtonWidth + RemoveButtonWidth + RenameButtonWidth + 5.0f;
	const float ButtonsWidth = clamp(DesiredButtonsWidth, 105.0f, View.w * 0.45f);

	CUIRect Buttons, Tabs;
	View.VSplitRight(ButtonsWidth, &Tabs, &Buttons);
	Buttons.VSplitLeft(3.0f, 0, &Buttons);

	int VisibleCount = 0;
	for(int Slot = 0; Slot < NUM_UI_FILTER_PRESETS; Slot++)
	{
		if(Slot >= UI_FILTER_PRESET_CUSTOM_START && !m_aFilterPresets[Slot].m_Used)
			continue;
		VisibleCount++;
	}
	const float TabWidth = clamp(Tabs.w / max(1, VisibleCount) - 2.0f, 48.0f, 72.0f);
	for(int Slot = 0; Slot < NUM_UI_FILTER_PRESETS; Slot++)
	{
		if(Slot >= UI_FILTER_PRESET_CUSTOM_START && !m_aFilterPresets[Slot].m_Used)
			continue;
		if(Tabs.w < TabWidth)
			break;

		CUIRect Tab;
		Tabs.VSplitLeft(TabWidth, &Tab, &Tabs);
		Tab.VSplitRight(2.0f, &Tab, &Tabs);

		const bool Active = m_ActiveFilterPreset == Slot;
		const char *pTabName = m_aFilterPresets[Slot].m_aName;
		if(Slot == UI_FILTER_PRESET_ALL)
			pTabName = Localize("All");
		else if(Slot == UI_FILTER_PRESET_FAVORITES)
			pTabName = Localize("Favorites");

		static int s_aTabIds[NUM_UI_FILTER_PRESETS] = {0};
		if(m_FilterPresetRenameSlot == Slot)
		{
			static float s_RenameOffset = 0.0f;
			if(DoEditBox(&m_aFilterPresetRenameBuf,
						 &Tab,
						 m_aFilterPresetRenameBuf,
						 sizeof(m_aFilterPresetRenameBuf),
						 10.0f,
						 &s_RenameOffset))
			{
				str_copy(
					m_aFilterPresets[Slot].m_aName, m_aFilterPresetRenameBuf, sizeof(m_aFilterPresets[Slot].m_aName));
				m_FilterPresetRenameSlot = -1;
				SaveFilterPresets();
			}
		}
		else if(DoButton_MenuTab(&s_aTabIds[Slot], pTabName, Active, &Tab, CUI::CORNER_ALL))
		{
			SwitchFilterPreset(Slot);
		}
	}

	CUIRect AddButton, RemoveButton, RenameButton;
	const float ButtonWidthScale = (Buttons.w - 2.0f) / (AddButtonWidth + RemoveButtonWidth + RenameButtonWidth);
	Buttons.VSplitLeft(AddButtonWidth * ButtonWidthScale, &AddButton, &Buttons);
	Buttons.VSplitLeft(1.0f, 0, &Buttons);
	Buttons.VSplitLeft(RemoveButtonWidth * ButtonWidthScale, &RemoveButton, &Buttons);
	Buttons.VSplitLeft(1.0f, 0, &RenameButton);

	static int s_AddPresetButton = 0;
	if(DoButton_Menu(&s_AddPresetButton, pAddText, 0, &AddButton))
	{
		int Slot = -1;
		for(int i = UI_FILTER_PRESET_CUSTOM_START; i < NUM_UI_FILTER_PRESETS; i++)
		{
			if(!m_aFilterPresets[i].m_Used)
			{
				Slot = i;
				break;
			}
		}
		if(Slot >= 0)
		{
			if(m_ActiveFilterPreset >= UI_FILTER_PRESET_CUSTOM_START && m_aFilterPresets[m_ActiveFilterPreset].m_Used)
				SnapshotConfigToFilterPreset(m_ActiveFilterPreset);
			m_aFilterPresets[Slot].m_Used = true;
			str_format(m_aFilterPresets[Slot].m_aName,
					   sizeof(m_aFilterPresets[Slot].m_aName),
					   "Filter %d",
					   Slot - UI_FILTER_PRESET_CUSTOM_START + 1);
			SnapshotConfigToFilterPreset(Slot);
			SwitchFilterPreset(Slot);
		}
	}

	static int s_RemovePresetButton = 0;
	if(DoButton_Menu(&s_RemovePresetButton, pRemoveText, 0, &RemoveButton))
	{
		if(m_ActiveFilterPreset >= UI_FILTER_PRESET_CUSTOM_START && m_aFilterPresets[m_ActiveFilterPreset].m_Used)
		{
			m_aFilterPresets[m_ActiveFilterPreset].m_Used = false;
			SwitchFilterPreset(UI_FILTER_PRESET_ALL);
		}
	}

	static int s_RenamePresetButton = 0;
	if(DoButton_Menu(&s_RenamePresetButton, pRenameText, m_FilterPresetRenameSlot >= 0, &RenameButton))
	{
		if(m_ActiveFilterPreset >= UI_FILTER_PRESET_CUSTOM_START && m_aFilterPresets[m_ActiveFilterPreset].m_Used)
		{
			if(m_FilterPresetRenameSlot == m_ActiveFilterPreset)
				m_FilterPresetRenameSlot = -1;
			else
			{
				m_FilterPresetRenameSlot = m_ActiveFilterPreset;
				str_copy(m_aFilterPresetRenameBuf,
						 m_aFilterPresets[m_ActiveFilterPreset].m_aName,
						 sizeof(m_aFilterPresetRenameBuf));
			}
		}
	}
}

void CMenus::OnRelease()
{
	for(int i = 0; i < 128; i++)
	{
		if(m_aSteamAvatars[i].m_Texture >= 0)
			Graphics()->UnloadTexture(m_aSteamAvatars[i].m_Texture);
		m_aSteamAvatars[i].m_Texture = -1;
		m_aSteamAvatars[i].m_UserID = 0;
	}
	m_pClient->m_pPveRoguelite->FlushPersistentProgress();
	PumpCloudProfile(true);
	SaveFilterPresets();
}

void CMenus::RenderServerbrowserServerList(CUIRect View)
{
	CUIRect Headers;
	CUIRect Status;

	View.HSplitTop(ms_ListheaderHeight, &Headers, &View);
	View.HSplitBottom(28.0f, &View, &Status);

	// split of the scrollbar
	DrawSectionHeader(&Headers, CUI::CORNER_T);
	Headers.VSplitRight(20.0f, &Headers, 0);

	struct CColumn
	{
		int m_ID;
		int m_Sort;
		CLocConstString m_Caption;
		int m_Direction;
		float m_Width;
		int m_Flags;
		CUIRect m_Rect;
		CUIRect m_Spacer;
	};

	enum
	{
		FIXED = 1,
		SPACER = 2,

		COL_FLAG_LOCK = 0,
		COL_FLAG_PURE,
		COL_FLAG_FAV,
		COL_NAME,
		COL_GAMETYPE,
		COL_MAP,
		COL_PLAYERS,
		COL_PING,
		COL_VERSION,
	};

	static CColumn s_aCols[] = {
		{-1, -1, " ", -1, 2.0f, 0, {0}, {0}},
		{COL_FLAG_LOCK, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_FLAG_PURE, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_FLAG_FAV, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_NAME, IServerBrowser::SORT_NAME, "Name", 0, 300.0f, 0, {0}, {0}}, // Localize - these strings are localized
																			   // within CLocConstString
		{COL_GAMETYPE, IServerBrowser::SORT_GAMETYPE, "Type", 1, 50.0f, 0, {0}, {0}},
		{COL_MAP, IServerBrowser::SORT_MAP, "Map", 1, 100.0f, 0, {0}, {0}},
		{COL_PLAYERS, IServerBrowser::SORT_NUMPLAYERS, "Players", 1, 60.0f, 0, {0}, {0}},
		{-1, -1, " ", 1, 10.0f, 0, {0}, {0}},
		{COL_PING, IServerBrowser::SORT_PING, "Ping", 1, 40.0f, FIXED, {0}, {0}},
	};
	// This is just for scripts/update_localization.py to work correctly (all other strings are already Localize()'d
	// somewhere else). Don't remove! Localize("Type");

	int NumCols = sizeof(s_aCols) / sizeof(CColumn);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == -1)
		{
			Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

			if(i + 1 < NumCols)
			{
				// Cols[i].flags |= SPACER;
				Headers.VSplitLeft(2, &s_aCols[i].m_Spacer, &Headers);
			}
		}
	}

	for(int i = NumCols - 1; i >= 0; i--)
	{
		if(s_aCols[i].m_Direction == 1)
		{
			Headers.VSplitRight(s_aCols[i].m_Width, &Headers, &s_aCols[i].m_Rect);
			Headers.VSplitRight(2, &Headers, &s_aCols[i].m_Spacer);
		}
	}

	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == 0)
			s_aCols[i].m_Rect = Headers;
	}

	// do headers
	for(int i = 0; i < NumCols; i++)
	{
		// spacers / icon columns: draw only, unique IDs so they don't share hover state
		if(s_aCols[i].m_Sort == -1 && (!s_aCols[i].m_Caption[0] || s_aCols[i].m_Caption[0] == ' '))
			continue;
		if(DoButton_GridHeader(
			   &s_aCols[i], s_aCols[i].m_Caption, g_Config.m_BrSort == s_aCols[i].m_Sort, &s_aCols[i].m_Rect))
		{
			if(s_aCols[i].m_Sort != -1)
			{
				if(g_Config.m_BrSort == s_aCols[i].m_Sort)
					g_Config.m_BrSortOrder ^= 1;
				else
					g_Config.m_BrSortOrder = 0;
				g_Config.m_BrSort = s_aCols[i].m_Sort;
			}
		}
	}

	DrawMenuInset(&View, 0);

	CUIRect Scroll;
	View.VSplitRight(15, &View, &Scroll);

	int NumServers = ServerBrowser()->NumSortedServers();

	// display important messages in the middle of the screen so no
	// users misses it
	{
		CUIRect MsgBox = View;
		MsgBox.y += View.h / 3;

		if(m_ActivePage == PAGE_INTERNET && ServerBrowser()->IsRefreshingMasters())
			UI()->DoLabelScaled(&MsgBox, Localize("Refreshing master servers"), 16.0f, 0);
		else if(!ServerBrowser()->NumServers())
			UI()->DoLabelScaled(&MsgBox, Localize("No servers found"), 16.0f, 0);
		else if(ServerBrowser()->NumServers() && !NumServers)
			UI()->DoLabelScaled(&MsgBox, Localize("No servers match your filter criteria"), 16.0f, 0);
	}

	int Num = (int)(View.h / s_aCols[0].m_Rect.h) + 1;
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;
	static float s_ScrollTarget = 0;

	Scroll.HMargin(5.0f, &Scroll);

	int ScrollNum = NumServers - Num + 1;
	if(ScrollNum > 0)
	{
		if(m_ScrollOffset)
		{
			s_ScrollTarget = (float)(m_ScrollOffset) / ScrollNum;
			s_ScrollValue = s_ScrollTarget;
			m_ScrollOffset = 0;
		}
		if(Input()->KeyPresses(KEY_MOUSE_WHEEL_UP) && UI()->MouseInside(&View))
			s_ScrollTarget -= 2.0f / ScrollNum;
		if(Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN) && UI()->MouseInside(&View))
			s_ScrollTarget += 2.0f / ScrollNum;

		s_ScrollTarget = clamp(s_ScrollTarget, 0.0f, 1.0f);
		const float ScrollDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
		s_ScrollValue += (s_ScrollTarget - s_ScrollValue) * (1.0f - expf(-14.0f * ScrollDt));
		if(fabs(s_ScrollValue - s_ScrollTarget) < 0.0005f)
			s_ScrollValue = s_ScrollTarget;

		float BarValue = DoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);
		if(fabs(BarValue - s_ScrollValue) > 0.0001f)
		{
			s_ScrollValue = BarValue;
			s_ScrollTarget = BarValue;
		}
	}
	else
	{
		ScrollNum = 0;
		s_ScrollValue = 0;
		s_ScrollTarget = 0;
		DoScrollbarV(&s_ScrollBar, &Scroll, 0);
	}

	if(m_SelectedIndex > -1)
	{
		for(int i = 0; i < m_NumInputEvents; i++)
		{
			int NewIndex = -1;
			if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS)
			{
				if(m_aInputEvents[i].m_Key == KEY_DOWN)
					NewIndex = m_SelectedIndex + 1;
				if(m_aInputEvents[i].m_Key == KEY_UP)
					NewIndex = m_SelectedIndex - 1;
			}
			if(NewIndex > -1 && NewIndex < NumServers)
			{
				// scroll
				float IndexY =
					View.y - s_ScrollValue * ScrollNum * s_aCols[0].m_Rect.h + NewIndex * s_aCols[0].m_Rect.h;
				int Scroll = View.y > IndexY ? -1 : View.y + View.h < IndexY + s_aCols[0].m_Rect.h ? 1 : 0;
				if(Scroll)
				{
					if(Scroll < 0)
					{
						int NumScrolls = (View.y - IndexY + s_aCols[0].m_Rect.h - 1.0f) / s_aCols[0].m_Rect.h;
						s_ScrollTarget -= (1.0f / ScrollNum) * NumScrolls;
					}
					else
					{
						int NumScrolls =
							(IndexY + s_aCols[0].m_Rect.h - (View.y + View.h) + s_aCols[0].m_Rect.h - 1.0f) /
							s_aCols[0].m_Rect.h;
						s_ScrollTarget += (1.0f / ScrollNum) * NumScrolls;
					}
					s_ScrollTarget = clamp(s_ScrollTarget, 0.0f, 1.0f);
				}

				m_SelectedIndex = NewIndex;

				const CServerInfo *pItem = ServerBrowser()->SortedGet(m_SelectedIndex);
				str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress, sizeof(g_Config.m_UiServerAddress));
			}
		}
	}

	s_ScrollValue = clamp(s_ScrollValue, 0.0f, 1.0f);
	s_ScrollTarget = clamp(s_ScrollTarget, 0.0f, 1.0f);

	// set clipping
	UI()->ClipEnable(&View);

	CUIRect OriginalView = View;
	View.y -= s_ScrollValue * ScrollNum * s_aCols[0].m_Rect.h;

	int NewSelected = -1;
	int NumPlayers = 0;

	m_SelectedIndex = -1;

	// reset friend counter
	for(int i = 0; i < m_lFriends.size(); m_lFriends[i++].m_NumFound = 0)
		;

	for(int i = 0; i < NumServers; i++)
	{
		int ItemIndex = i;
		const CServerInfo *pItem = ServerBrowser()->SortedGet(ItemIndex);
		NumPlayers += g_Config.m_BrFilterSpectators ? pItem->m_NumPlayers : pItem->m_NumClients;
		CUIRect Row;
		CUIRect SelectHitBox;

		int Selected = str_comp(pItem->m_aAddress, g_Config.m_UiServerAddress) == 0; // selected_index==ItemIndex;

		View.HSplitTop(15.0f, &Row, &View);
		SelectHitBox = Row;

		if(Selected)
			m_SelectedIndex = i;

		// update friend counter
		if(pItem->m_FriendState != IFriends::FRIEND_NO)
		{
			for(int j = 0; j < pItem->m_NumClients; ++j)
			{
				if(pItem->m_aClients[j].m_FriendState != IFriends::FRIEND_NO)
				{
					unsigned NameHash = str_quickhash(pItem->m_aClients[j].m_aName);
					unsigned ClanHash = str_quickhash(pItem->m_aClients[j].m_aClan);
					for(int f = 0; f < m_lFriends.size(); ++f)
					{
						if(ClanHash == m_lFriends[f].m_pFriendInfo->m_ClanHash &&
						   (!m_lFriends[f].m_pFriendInfo->m_aName[0] ||
							NameHash == m_lFriends[f].m_pFriendInfo->m_NameHash))
						{
							m_lFriends[f].m_NumFound++;
							if(m_lFriends[f].m_pFriendInfo->m_aName[0])
								break;
						}
					}
				}
			}
		}

		// make sure that only those in view can be selected
		if(Row.y + Row.h > OriginalView.y && Row.y < OriginalView.y + OriginalView.h)
		{
			if(Selected)
			{
				CUIRect r = Row;
				r.Margin(0.5f, &r);
				RenderTools()->DrawUIRect(&r, vec4(0.12f, 0.13f, 0.16f, 0.58f), CUI::CORNER_ALL, ms_ControlRounding);
				DrawAccentUnderline(&r);
			}

			// clip the selection
			if(SelectHitBox.y < OriginalView.y) // top
			{
				SelectHitBox.h -= OriginalView.y - SelectHitBox.y;
				SelectHitBox.y = OriginalView.y;
			}
			else if(SelectHitBox.y + SelectHitBox.h > OriginalView.y + OriginalView.h) // bottom
				SelectHitBox.h = OriginalView.y + OriginalView.h - SelectHitBox.y;

			if(UI()->DoButtonLogic(pItem, "", Selected, &SelectHitBox))
			{
				NewSelected = ItemIndex;
			}
		}
		else
		{
			// reset active item, if not visible
			if(UI()->ActiveItem() == pItem)
				UI()->SetActiveItem(0);

			// don't render invisible items
			continue;
		}

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			char aTemp[64];
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = Row.y;
			Button.h = Row.h;
			Button.w = s_aCols[c].m_Rect.w;

			int ID = s_aCols[c].m_ID;

			if(ID == COL_FLAG_LOCK)
			{
				if(pItem->m_Flags & SERVER_FLAG_PASSWORD)
					DoButton_Icon(IMAGE_BROWSEICONS, SPRITE_BROWSE_LOCK, &Button);
			}
			else if(ID == COL_FLAG_PURE)
			{
				if(str_comp(pItem->m_aGameType, "DM") == 0 || str_comp(pItem->m_aGameType, "TDM") == 0 ||
				   str_comp(pItem->m_aGameType, "BALL") == 0 || str_comp(pItem->m_aGameType, "DEF") == 0 ||
				   str_comp(pItem->m_aGameType, "INF") == 0 || str_comp(pItem->m_aGameType, "INV") == 0 ||
				   str_comp(pItem->m_aGameType, "TUT") == 0 || str_comp(pItem->m_aGameType, "GUN") == 0 ||
				   str_comp(pItem->m_aGameType, "CTF") == 0 || str_comp(pItem->m_aGameType, "Deathmatch") == 0 ||
				   str_comp(pItem->m_aGameType, "Team deathmatch") == 0 || str_comp(pItem->m_aGameType, "Ball") == 0 ||
				   str_comp(pItem->m_aGameType, "Reactor Defense") == 0 ||
				   str_comp(pItem->m_aGameType, "Reactor Assault") == 0 ||
				   str_comp(pItem->m_aGameType, "Infection") == 0 || str_comp(pItem->m_aGameType, "Invasion") == 0 ||
				   str_comp(pItem->m_aGameType, "Tutorial") == 0 ||
				   str_comp(pItem->m_aGameType, "Capture the flag") == 0 ||
				   str_comp(pItem->m_aGameType, "Extraction") == 0 || str_comp(pItem->m_aGameType, "Horde") == 0 ||
				   str_comp(pItem->m_aGameType, "Roam") == 0 ||
				   str_comp(pItem->m_aGameType, "Nodes") == 0)
				{
					// pure server
				}
				else
				{
					// unpure
					DoButton_Icon(IMAGE_BROWSEICONS, SPRITE_BROWSE_UNPURE, &Button);
				}
			}
			else if(ID == COL_FLAG_FAV)
			{
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BROWSEICONS].m_Id);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, pItem->m_Favorite ? 1.0f : 0.35f);
				RenderTools()->SelectSprite(SPRITE_BROWSE_HEART);
				IGraphics::CQuadItem QuadItem(Button.x, Button.y, Button.w, Button.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
				if(UI()->DoButtonLogic((void *)((char *)pItem + 1), "fav", 0, &Button))
				{
					if(pItem->m_Favorite)
						ServerBrowser()->RemoveFavorite(pItem->m_NetAddr);
					else
						ServerBrowser()->AddFavorite(pItem->m_NetAddr);

					// keep favorites view in sync after star toggles
					if(g_Config.m_UiPage == PAGE_FAVORITES)
						ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
					else
						Client()->ServerBrowserUpdate();
				}
			}
			else if(ID == COL_NAME)
			{
				CTextCursor Cursor;
				TextRender()->SetCursor(
					&Cursor, Button.x, Button.y, 10.0f * UI()->Scale(), TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;
				if(pItem->m_HasPlatformMetadata)
				{
					const char *pCategory =
						pItem->m_Official ? "[OFFICIAL] " : (pItem->m_Modded ? "[COMMUNITY MODDED] " : "[COMMUNITY] ");
					TextRender()->TextColor(
						pItem->m_Official ? 0.35f : 0.65f, pItem->m_Official ? 0.85f : 0.7f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, pCategory, -1);
					const char *pAuth = pItem->m_AuthPolicy == 2   ? "[STEAM REQUIRED] "
										: pItem->m_AuthPolicy == 1 ? "[STEAM OPTIONAL] "
																   : "[OPEN] ";
					TextRender()->TextColor(
						pItem->m_AuthPolicy == 2 ? 1.0f : 0.55f, pItem->m_AuthPolicy == 2 ? 0.65f : 0.9f, 0.45f, 1.0f);
					TextRender()->TextEx(&Cursor, pAuth, -1);
					TextRender()->TextColor(1, 1, 1, 1);
				}

				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_SERVERNAME))
				{
					// highlight the parts that matches
					const char *pStr = str_find_nocase(pItem->m_aName, g_Config.m_BrFilterString);
					if(pStr)
					{
						TextRender()->TextEx(&Cursor, pItem->m_aName, (int)(pStr - pItem->m_aName));
						TextRender()->TextColor(0.95f, 0.58f, 0.18f, 1);
						TextRender()->TextEx(&Cursor, pStr, str_length(g_Config.m_BrFilterString));
						TextRender()->TextColor(1, 1, 1, 1);
						TextRender()->TextEx(&Cursor, pStr + str_length(g_Config.m_BrFilterString), -1);
					}
					else
						TextRender()->TextEx(&Cursor, pItem->m_aName, -1);
				}
				else
					TextRender()->TextEx(&Cursor, pItem->m_aName, -1);
			}
			else if(ID == COL_MAP)
			{
				CTextCursor Cursor;
				TextRender()->SetCursor(
					&Cursor, Button.x, Button.y, 10.0f * UI()->Scale(), TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;

				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_MAPNAME))
				{
					// highlight the parts that matches
					const char *pStr = str_find_nocase(pItem->m_aMap, g_Config.m_BrFilterString);
					if(pStr)
					{
						TextRender()->TextEx(&Cursor, pItem->m_aMap, (int)(pStr - pItem->m_aMap));
						TextRender()->TextColor(0.95f, 0.58f, 0.18f, 1);
						TextRender()->TextEx(&Cursor, pStr, str_length(g_Config.m_BrFilterString));
						TextRender()->TextColor(1, 1, 1, 1);
						TextRender()->TextEx(&Cursor, pStr + str_length(g_Config.m_BrFilterString), -1);
					}
					else
						TextRender()->TextEx(&Cursor, pItem->m_aMap, -1);
				}
				else
					TextRender()->TextEx(&Cursor, pItem->m_aMap, -1);
			}
			else if(ID == COL_PLAYERS)
			{
				CUIRect Icon;
				Button.VMargin(4.0f, &Button);
				if(pItem->m_FriendState != IFriends::FRIEND_NO)
				{
					Button.VSplitLeft(Button.h, &Icon, &Button);
					Icon.Margin(2.0f, &Icon);
					DoButton_Icon(IMAGE_BROWSEICONS, SPRITE_BROWSE_HEART, &Icon);
				}

				if(g_Config.m_BrFilterSpectators)
					str_format(aTemp, sizeof(aTemp), "%i/%i", pItem->m_NumPlayers, pItem->m_MaxPlayers);
				else
					str_format(aTemp, sizeof(aTemp), "%i/%i", pItem->m_NumClients, pItem->m_MaxClients);
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_PLAYER))
					TextRender()->TextColor(0.95f, 0.58f, 0.18f, 1);
				UI()->DoLabelScaled(&Button, aTemp, 10.0f, 1);
				TextRender()->TextColor(1, 1, 1, 1);
			}
			else if(ID == COL_PING)
			{
				str_format(aTemp, sizeof(aTemp), "%i", pItem->m_Latency);
				UI()->DoLabelScaled(&Button, aTemp, 10.0f, 1);
			}
			else if(ID == COL_VERSION)
			{
				const char *pVersion = pItem->m_aVersion;
				UI()->DoLabelScaled(&Button, pVersion, 10.0f, 1);
			}
			else if(ID == COL_GAMETYPE)
			{
				CTextCursor Cursor;
				TextRender()->SetCursor(
					&Cursor, Button.x, Button.y, 12.0f * UI()->Scale(), TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;
				TextRender()->TextEx(&Cursor, pItem->m_aGameType, -1);
			}
		}
	}

	UI()->ClipDisable();

	if(NewSelected != -1)
	{
		// select the new server
		const CServerInfo *pItem = ServerBrowser()->SortedGet(NewSelected);
		str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress, sizeof(g_Config.m_UiServerAddress));
		if(Input()->MouseDoubleClick())
			Client()->Connect(g_Config.m_UiServerAddress);
	}

	DrawMenuInset(&Status, CUI::CORNER_B);
	Status.Margin(5.0f, &Status);

	// render quick search
	CUIRect QuickSearch, Button, LabelRect;
	Status.VSplitLeft(260.0f, &QuickSearch, &Status);
	const char *pLabel = Localize("Quick search:");
	float LabelW = TextRender()->TextWidth(0, 10.0f, pLabel, -1) + 5.0f;
	QuickSearch.VSplitLeft(LabelW, &LabelRect, &QuickSearch);
	UI()->DoLabelScaled(&LabelRect, pLabel, 10.0f, -1);
	QuickSearch.VSplitRight(18.0f, &QuickSearch, &Button);
	static float Offset = 0.0f;
	if(DoEditBox(&g_Config.m_BrFilterString,
				 &QuickSearch,
				 g_Config.m_BrFilterString,
				 sizeof(g_Config.m_BrFilterString),
				 10.0f,
				 &Offset,
				 false,
				 CUI::CORNER_L))
		Client()->ServerBrowserUpdate();

	// clear button
	{
		static int s_ClearButton = 0;
		RenderTools()->DrawUIRect(&Button,
								  vec4(0.06f, 0.07f, 0.09f, 0.9f) * ButtonColorMul(&s_ClearButton),
								  CUI::CORNER_R,
								  ms_ControlRounding);
		UI()->DoLabel(&Button, "x", min(Button.h * ms_FontmodHeight, 11.0f), 0);
		if(UI()->DoButtonLogic(&s_ClearButton, "x", 0, &Button))
		{
			g_Config.m_BrFilterString[0] = 0;
			UI()->SetActiveItem(&g_Config.m_BrFilterString);
			Client()->ServerBrowserUpdate();
		}
	}

	// render status
	char aBuf[128];
	if(ServerBrowser()->IsRefreshing())
		str_format(aBuf, sizeof(aBuf), Localize("%d%% loaded"), ServerBrowser()->LoadingProgression());
	else
		str_format(aBuf,
				   sizeof(aBuf),
				   Localize("%d of %d servers, %d players"),
				   ServerBrowser()->NumSortedServers(),
				   ServerBrowser()->NumServers(),
				   NumPlayers);
	Status.VSplitRight(TextRender()->TextWidth(0, 11.0f, aBuf, -1), 0, &Status);
	UI()->DoLabelScaled(&Status, aBuf, 11.0f, -1);
}

void CMenus::RenderServerbrowserFilters(CUIRect View)
{
	CUIRect ServerFilter = View, FilterHeader;
	const float FontSize = 12.0f;
	ServerFilter.HSplitBottom(42.5f, &ServerFilter, 0);

	// server filter
	ServerFilter.HSplitTop(ms_ListheaderHeight, &FilterHeader, &ServerFilter);
	DrawSectionHeader(&FilterHeader, CUI::CORNER_T);
	DrawMenuInset(&ServerFilter, CUI::CORNER_B);
	UI()->DoLabelScaled(&FilterHeader, Localize("Server filter"), FontSize, 0);
	CUIRect Button;

	CUIRect ScrollArea, ResetButton;
	ServerFilter.HSplitBottom(ms_ButtonHeight - 2.0f + 5.0f, &ScrollArea, &ResetButton);
	ResetButton.HSplitTop(ms_ButtonHeight - 2.0f, &ResetButton, 0);

	static CScrollRegion s_FilterScrollRegion;
	CScrollRegionParams ScrollParams;
	ConfigureScrollRegion(&ScrollParams);
	ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	vec2 ScrollOffset;
	s_FilterScrollRegion.Begin(&ScrollArea, &ScrollOffset, &ScrollParams);

	CUIRect ServerFilterContent = ScrollArea;
	ServerFilterContent.y += ScrollOffset.y;
	ServerFilterContent.VSplitLeft(5.0f, 0, &ServerFilterContent);
	ServerFilterContent.Margin(3.0f, &ServerFilterContent);
	ServerFilterContent.VMargin(5.0f, &ServerFilterContent);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(&g_Config.m_BrFilterEmpty, Localize("Has people playing"), g_Config.m_BrFilterEmpty, &Button))
		g_Config.m_BrFilterEmpty ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(
		   &g_Config.m_BrFilterSpectators, Localize("Count players only"), g_Config.m_BrFilterSpectators, &Button))
		g_Config.m_BrFilterSpectators ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(&g_Config.m_BrFilterFull, Localize("Server not full"), g_Config.m_BrFilterFull, &Button))
		g_Config.m_BrFilterFull ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(
		   &g_Config.m_BrFilterFriends, Localize("Show friends only"), g_Config.m_BrFilterFriends, &Button))
		g_Config.m_BrFilterFriends ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(&g_Config.m_BrFilterPw, Localize("No password"), g_Config.m_BrFilterPw, &Button))
		g_Config.m_BrFilterPw ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox((char *)&g_Config.m_BrFilterCompatversion,
						 Localize("Compatible version"),
						 g_Config.m_BrFilterCompatversion,
						 &Button))
		g_Config.m_BrFilterCompatversion ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(
		   (char *)&g_Config.m_BrFilterPure, Localize("Standard gametype"), g_Config.m_BrFilterPure, &Button))
		g_Config.m_BrFilterPure ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox(
		   (char *)&g_Config.m_BrFilterPureMap, Localize("Standard map"), g_Config.m_BrFilterPureMap, &Button))
		g_Config.m_BrFilterPureMap ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(20.0f, &Button, &ServerFilterContent);
	if(DoButton_CheckBox((char *)&g_Config.m_BrFilterGametypeStrict,
						 Localize("Strict gametype filter"),
						 g_Config.m_BrFilterGametypeStrict,
						 &Button))
		g_Config.m_BrFilterGametypeStrict ^= 1;
	s_FilterScrollRegion.AddRect(Button);

	ServerFilterContent.HSplitTop(5.0f, 0, &ServerFilterContent);

	ServerFilterContent.HSplitTop(19.0f, &Button, &ServerFilterContent);
	UI()->DoLabelScaled(&Button, Localize("Game types:"), FontSize, -1);
	Button.VSplitRight(60.0f, 0, &Button);
	ServerFilterContent.HSplitTop(3.0f, 0, &ServerFilterContent);
	static float Offset = 0.0f;
	if(DoEditBox(&g_Config.m_BrFilterGametype,
				 &Button,
				 g_Config.m_BrFilterGametype,
				 sizeof(g_Config.m_BrFilterGametype),
				 FontSize,
				 &Offset))
		Client()->ServerBrowserUpdate();
	UI()->ClipEnable(&ScrollArea);
	s_FilterScrollRegion.AddRect(Button);

	{
		ServerFilterContent.HSplitTop(19.0f, &Button, &ServerFilterContent);
		CUIRect EditBox;
		Button.VSplitRight(60.0f, &Button, &EditBox);

		UI()->DoLabelScaled(&Button, Localize("Maximum ping:"), FontSize, -1);

		char aBuf[5];
		str_format(aBuf, sizeof(aBuf), "%d", g_Config.m_BrFilterPing);
		static float Offset = 0.0f;
		DoEditBox(&g_Config.m_BrFilterPing, &EditBox, aBuf, sizeof(aBuf), FontSize, &Offset);
		UI()->ClipEnable(&ScrollArea);
		g_Config.m_BrFilterPing = clamp(str_toint(aBuf), 0, 999);
		s_FilterScrollRegion.AddRect(Button);
		s_FilterScrollRegion.AddRect(EditBox);
	}

	// server address
	ServerFilterContent.HSplitTop(3.0f, 0, &ServerFilterContent);
	ServerFilterContent.HSplitTop(19.0f, &Button, &ServerFilterContent);
	UI()->DoLabelScaled(&Button, Localize("Server address:"), FontSize, -1);
	Button.VSplitRight(60.0f, 0, &Button);
	static float OffsetAddr = 0.0f;
	if(DoEditBox(&g_Config.m_BrFilterServerAddress,
				 &Button,
				 g_Config.m_BrFilterServerAddress,
				 sizeof(g_Config.m_BrFilterServerAddress),
				 FontSize,
				 &OffsetAddr))
		Client()->ServerBrowserUpdate();
	UI()->ClipEnable(&ScrollArea);
	s_FilterScrollRegion.AddRect(Button);

	// player country
	{
		CUIRect Rect;
		ServerFilterContent.HSplitTop(3.0f, 0, &ServerFilterContent);
		ServerFilterContent.HSplitTop(26.0f, &Button, &ServerFilterContent);
		Button.VSplitRight(60.0f, &Button, &Rect);
		Button.HMargin(3.0f, &Button);
		if(DoButton_CheckBox(
			   &g_Config.m_BrFilterCountry, Localize("Player country:"), g_Config.m_BrFilterCountry, &Button))
			g_Config.m_BrFilterCountry ^= 1;

		float OldWidth = Rect.w;
		Rect.w = Rect.h * 2;
		Rect.x += (OldWidth - Rect.w) / 2.0f;
		vec4 Color(1.0f, 1.0f, 1.0f, g_Config.m_BrFilterCountry ? 1.0f : 0.5f);
		m_pClient->m_pCountryFlags->Render(g_Config.m_BrFilterCountryIndex, &Color, Rect.x, Rect.y, Rect.w, Rect.h);

		if(g_Config.m_BrFilterCountry && UI()->DoButtonLogic(&g_Config.m_BrFilterCountryIndex, "", 0, &Rect))
			m_Popup = POPUP_COUNTRY;
		s_FilterScrollRegion.AddRect(Button);
		s_FilterScrollRegion.AddRect(Rect);
	}

	s_FilterScrollRegion.End();

	static int s_ClearButton = 0;
	if(DoButton_Menu(&s_ClearButton, Localize("Reset filter"), 0, &ResetButton))
	{
		g_Config.m_BrFilterString[0] = 0;
		g_Config.m_BrFilterFull = 0;
		g_Config.m_BrFilterEmpty = 0;
		g_Config.m_BrFilterSpectators = 0;
		g_Config.m_BrFilterFriends = 0;
		g_Config.m_BrFilterCountry = 0;
		g_Config.m_BrFilterCountryIndex = -1;
		g_Config.m_BrFilterPw = 0;
		g_Config.m_BrFilterPing = 999;
		g_Config.m_BrFilterGametype[0] = 0;
		g_Config.m_BrFilterGametypeStrict = 0;
		g_Config.m_BrFilterServerAddress[0] = 0;
		g_Config.m_BrFilterPure = 1;
		g_Config.m_BrFilterPureMap = 1;
		g_Config.m_BrFilterCompatversion = 1;
		Client()->ServerBrowserUpdate();
	}
}

void CMenus::RenderServerbrowserServerDetail(CUIRect View)
{
	CUIRect ServerDetails = View;
	CUIRect ServerScoreBoard, ServerHeader;

	const CServerInfo *pSelectedServer = ServerBrowser()->SortedGet(m_SelectedIndex);

	// split off a piece to use for scoreboard
	ServerDetails.HSplitTop(90.0f, &ServerDetails, &ServerScoreBoard);
	ServerDetails.HSplitBottom(2.5f, &ServerDetails, 0x0);

	// server details
	CTextCursor Cursor;
	const float FontSize = 12.0f;
	ServerDetails.HSplitTop(ms_ListheaderHeight, &ServerHeader, &ServerDetails);
	DrawSectionHeader(&ServerHeader, CUI::CORNER_T);
	DrawMenuInset(&ServerDetails, CUI::CORNER_B);
	UI()->DoLabelScaled(&ServerHeader, Localize("Server details"), FontSize, 0);

	if(pSelectedServer)
	{
		ServerDetails.VSplitLeft(5.0f, 0, &ServerDetails);
		ServerDetails.Margin(3.0f, &ServerDetails);

		CUIRect Row;
		static CLocConstString s_aLabels[] = {
			"Version", // Localize - these strings are localized within CLocConstString
			"Game type",
			"Ping"};

		CUIRect LeftColumn;
		CUIRect RightColumn;

		//
		{
			CUIRect Button;
			ServerDetails.HSplitBottom(20.0f, &ServerDetails, &Button);
			Button.VSplitLeft(5.0f, 0, &Button);
			static int s_AddFavButton = 0;
			if(DoButton_CheckBox(&s_AddFavButton, Localize("Favorite"), pSelectedServer->m_Favorite, &Button))
			{
				if(pSelectedServer->m_Favorite)
					ServerBrowser()->RemoveFavorite(pSelectedServer->m_NetAddr);
				else
					ServerBrowser()->AddFavorite(pSelectedServer->m_NetAddr);
			}
		}

		ServerDetails.VSplitLeft(5.0f, 0x0, &ServerDetails);
		ServerDetails.VSplitLeft(80.0f, &LeftColumn, &RightColumn);

		for(unsigned int i = 0; i < sizeof(s_aLabels) / sizeof(s_aLabels[0]); i++)
		{
			LeftColumn.HSplitTop(15.0f, &Row, &LeftColumn);
			UI()->DoLabelScaled(&Row, s_aLabels[i], FontSize, -1);
		}

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		TextRender()->SetCursor(&Cursor, Row.x, Row.y, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Row.w;
		TextRender()->TextEx(&Cursor, pSelectedServer->m_aVersion, -1);

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		TextRender()->SetCursor(&Cursor, Row.x, Row.y, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Row.w;
		TextRender()->TextEx(&Cursor, pSelectedServer->m_aGameType, -1);

		char aTemp[16];
		str_format(aTemp, sizeof(aTemp), "%d", pSelectedServer->m_Latency);
		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		TextRender()->SetCursor(&Cursor, Row.x, Row.y, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Row.w;
		TextRender()->TextEx(&Cursor, aTemp, -1);
	}

	// server scoreboard
	ServerScoreBoard.HSplitBottom(20.0f, &ServerScoreBoard, 0x0);
	ServerScoreBoard.HSplitTop(ms_ListheaderHeight, &ServerHeader, &ServerScoreBoard);
	DrawSectionHeader(&ServerHeader, CUI::CORNER_T);
	DrawMenuInset(&ServerScoreBoard, CUI::CORNER_B);
	UI()->DoLabelScaled(&ServerHeader, Localize("Scoreboard"), FontSize, 0);

	if(pSelectedServer)
	{
		static CScrollRegion s_ScoreboardScrollRegion;
		CScrollRegionParams ScrollParams;
		ConfigureScrollRegion(&ScrollParams);
		ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		vec2 ScrollOffset;
		s_ScoreboardScrollRegion.Begin(&ServerScoreBoard, &ScrollOffset, &ScrollParams);

		CUIRect ScoreboardContent = ServerScoreBoard;
		ScoreboardContent.y += ScrollOffset.y;
		ScoreboardContent.Margin(3.0f, &ScoreboardContent);

		for(int i = 0; i < pSelectedServer->m_NumClients; i++)
		{
			CUIRect Name, Clan, Score, Flag;
			ScoreboardContent.HSplitTop(25.0f, &Name, &ScoreboardContent);
			s_ScoreboardScrollRegion.AddRect(Name);
			if(s_ScoreboardScrollRegion.IsRectClipped(Name))
				continue;

			if(UI()->DoButtonLogic(&pSelectedServer->m_aClients[i], "", 0, &Name))
			{
				if(pSelectedServer->m_aClients[i].m_FriendState == IFriends::FRIEND_PLAYER)
					m_pClient->Friends()->RemoveFriend(pSelectedServer->m_aClients[i].m_aName,
													   pSelectedServer->m_aClients[i].m_aClan);
				else
					m_pClient->Friends()->AddFriend(pSelectedServer->m_aClients[i].m_aName,
													pSelectedServer->m_aClients[i].m_aClan);
				FriendlistOnUpdate();
				Client()->ServerBrowserUpdate();
			}

			vec4 Colour = pSelectedServer->m_aClients[i].m_FriendState == IFriends::FRIEND_NO
							  ? vec4(0.11f, 0.12f, 0.14f, (i % 2 + 1) * 0.08f)
							  : vec4(0.18f, 0.66f, 0.46f, 0.16f + (i % 2 + 1) * 0.05f);
			RenderTools()->DrawUIRect(&Name, Colour, CUI::CORNER_ALL, 4.0f);
			Name.VSplitLeft(5.0f, 0, &Name);
			Name.VSplitLeft(30.0f, &Score, &Name);
			Name.VSplitRight(34.0f, &Name, &Flag);
			Flag.HMargin(4.0f, &Flag);
			Name.HSplitTop(11.0f, &Name, &Clan);

			// score
			if(pSelectedServer->m_aClients[i].m_Player)
			{
				char aTemp[16];
				str_format(aTemp, sizeof(aTemp), "%d", pSelectedServer->m_aClients[i].m_Score);
				TextRender()->SetCursor(&Cursor,
										Score.x,
										Score.y + (Score.h - FontSize) / 4.0f,
										FontSize,
										TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Score.w;
				TextRender()->TextEx(&Cursor, aTemp, -1);
			}

			// name
			TextRender()->SetCursor(&Cursor, Name.x, Name.y, FontSize - 2, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Name.w;
			const char *pName = pSelectedServer->m_aClients[i].m_aName;
			if(g_Config.m_BrFilterString[0])
			{
				// highlight the parts that matches
				const char *s = str_find_nocase(pName, g_Config.m_BrFilterString);
				if(s)
				{
					TextRender()->TextEx(&Cursor, pName, (int)(s - pName));
					TextRender()->TextColor(0.95f, 0.58f, 0.18f, 1.0f);
					TextRender()->TextEx(&Cursor, s, str_length(g_Config.m_BrFilterString));
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, s + str_length(g_Config.m_BrFilterString), -1);
				}
				else
					TextRender()->TextEx(&Cursor, pName, -1);
			}
			else
				TextRender()->TextEx(&Cursor, pName, -1);

			// clan
			TextRender()->SetCursor(&Cursor, Clan.x, Clan.y, FontSize - 2, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Clan.w;
			const char *pClan = pSelectedServer->m_aClients[i].m_aClan;
			if(g_Config.m_BrFilterString[0])
			{
				// highlight the parts that matches
				const char *s = str_find_nocase(pClan, g_Config.m_BrFilterString);
				if(s)
				{
					TextRender()->TextEx(&Cursor, pClan, (int)(s - pClan));
					TextRender()->TextColor(0.95f, 0.58f, 0.18f, 1.0f);
					TextRender()->TextEx(&Cursor, s, str_length(g_Config.m_BrFilterString));
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, s + str_length(g_Config.m_BrFilterString), -1);
				}
				else
					TextRender()->TextEx(&Cursor, pClan, -1);
			}
			else
				TextRender()->TextEx(&Cursor, pClan, -1);

			// flag
			vec4 Color(1.0f, 1.0f, 1.0f, 0.5f);
			m_pClient->m_pCountryFlags->Render(
				pSelectedServer->m_aClients[i].m_Country, &Color, Flag.x, Flag.y, Flag.w, Flag.h);
		}

		s_ScoreboardScrollRegion.End();
	}
}

void CMenus::FriendlistOnUpdate()
{
	m_lFriends.clear();
	for(int i = 0; i < m_pClient->Friends()->NumFriends(); ++i)
	{
		CFriendItem Item;
		Item.m_pFriendInfo = m_pClient->Friends()->GetFriend(i);
		Item.m_NumFound = 0;
		m_lFriends.add_unsorted(Item);
	}
	m_lFriends.sort_range();
}

void CMenus::RenderServerbrowserFriends(CUIRect View)
{
	static int s_Inited = 0;
	if(!s_Inited)
	{
		FriendlistOnUpdate();
		s_Inited = 1;
	}

	CUIRect ServerFriends = View, FilterHeader;
	const float FontSize = 10.0f;

	// header
	ServerFriends.HSplitTop(ms_ListheaderHeight, &FilterHeader, &ServerFriends);
	DrawSectionHeader(&FilterHeader, CUI::CORNER_T);
	DrawMenuInset(&ServerFriends, CUI::CORNER_B);
	UI()->DoLabelScaled(&FilterHeader, Localize("Friends"), FontSize, 0);
	CUIRect Button, List;

	ServerFriends.Margin(3.0f, &ServerFriends);
	ServerFriends.VMargin(3.0f, &ServerFriends);
	ServerFriends.HSplitBottom(100.0f, &List, &ServerFriends);

	// friends list(remove friend)
	static float s_ScrollValue = 0;
	if(m_FriendlistSelectedIndex >= m_lFriends.size())
		m_FriendlistSelectedIndex = m_lFriends.size() - 1;
	UiDoListboxStart(&m_lFriends, &List, 30.0f, "", "", m_lFriends.size(), 1, m_FriendlistSelectedIndex, s_ScrollValue);

	m_lFriends.sort_range();
	int JoinFriendIndex = -1;
	for(int i = 0; i < m_lFriends.size(); ++i)
	{
		CListboxItem Item = UiDoListboxNextItem(&m_lFriends[i]);

		if(Item.m_Visible)
		{
			Item.m_Rect.Margin(1.5f, &Item.m_Rect);
			CUIRect OnState, JoinRect;
			Item.m_Rect.VSplitRight(30.0f, &Item.m_Rect, &OnState);
			Item.m_Rect.VSplitRight(45.0f, &Item.m_Rect, &JoinRect);
			RenderTools()->DrawUIRect(&Item.m_Rect, vec4(0.12f, 0.13f, 0.16f, 0.14f), CUI::CORNER_L, 4.0f);

			Item.m_Rect.VMargin(2.5f, &Item.m_Rect);
			Item.m_Rect.HSplitTop(12.0f, &Item.m_Rect, &Button);
			UI()->DoLabelScaled(&Item.m_Rect, m_lFriends[i].m_pFriendInfo->m_aName, FontSize, -1);
			UI()->DoLabelScaled(&Button, m_lFriends[i].m_pFriendInfo->m_aClan, FontSize, -1);

			// status indicator
			RenderTools()->DrawUIRect(&OnState,
									  m_lFriends[i].m_NumFound ? vec4(0.18f, 0.66f, 0.46f, 0.28f)
															   : vec4(0.92f, 0.24f, 0.30f, 0.28f),
									  CUI::CORNER_R,
									  4.0f);
			OnState.HMargin((OnState.h - FontSize) / 3, &OnState);
			OnState.VMargin(5.0f, &OnState);
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%i", m_lFriends[i].m_NumFound);
			UI()->DoLabelScaled(&OnState, aBuf, FontSize + 2, 1);

			// join button
			if(m_lFriends[i].m_NumFound > 0)
			{
				RenderTools()->DrawUIRect(&JoinRect, vec4(0.95f, 0.58f, 0.18f, 0.5f), CUI::CORNER_ALL, 3.0f);
				UI()->DoLabelScaled(&JoinRect, Localize("Join"), FontSize, 1);
				if(UI()->DoButtonLogic(&m_lFriends[i], "Join", 0, &JoinRect))
					JoinFriendIndex = i;
			}
		}
	}

	bool Activated = false;
	m_FriendlistSelectedIndex = UiDoListboxEnd(&s_ScrollValue, &Activated);

	// activate found server with friend
	if(Activated && !m_EnterPressed && m_lFriends[m_FriendlistSelectedIndex].m_NumFound)
	{
		bool Found = false;
		int NumServers = ServerBrowser()->NumSortedServers();
		for(int i = 0; i < NumServers && !Found; i++)
		{
			int ItemIndex = m_SelectedIndex != -1 ? (m_SelectedIndex + i + 1) % NumServers : i;
			const CServerInfo *pItem = ServerBrowser()->SortedGet(ItemIndex);
			if(pItem->m_FriendState != IFriends::FRIEND_NO)
			{
				for(int j = 0; j < pItem->m_NumClients && !Found; ++j)
				{
					if(pItem->m_aClients[j].m_FriendState != IFriends::FRIEND_NO &&
					   str_quickhash(pItem->m_aClients[j].m_aClan) ==
						   m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_ClanHash &&
					   (!m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName[0] ||
						str_quickhash(pItem->m_aClients[j].m_aName) ==
							m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_NameHash))
					{
						str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress, sizeof(g_Config.m_UiServerAddress));
						m_ScrollOffset = ItemIndex;
						m_SelectedIndex = ItemIndex;
						Found = true;
					}
				}
			}
		}
	}

	// join friend's server
	if(JoinFriendIndex >= 0 && JoinFriendIndex < m_lFriends.size() && m_lFriends[JoinFriendIndex].m_NumFound)
	{
		bool Found = false;
		int NumServers = ServerBrowser()->NumSortedServers();
		for(int i = 0; i < NumServers && !Found; i++)
		{
			const CServerInfo *pItem = ServerBrowser()->SortedGet(i);
			if(pItem->m_FriendState != IFriends::FRIEND_NO)
			{
				for(int j = 0; j < pItem->m_NumClients && !Found; ++j)
				{
					if(pItem->m_aClients[j].m_FriendState != IFriends::FRIEND_NO &&
					   str_quickhash(pItem->m_aClients[j].m_aClan) ==
						   m_lFriends[JoinFriendIndex].m_pFriendInfo->m_ClanHash &&
					   (!m_lFriends[JoinFriendIndex].m_pFriendInfo->m_aName[0] ||
						str_quickhash(pItem->m_aClients[j].m_aName) ==
							m_lFriends[JoinFriendIndex].m_pFriendInfo->m_NameHash))
					{
						Client()->Connect(pItem->m_aAddress);
						Found = true;
					}
				}
			}
		}
	}

	ServerFriends.HSplitTop(2.5f, 0, &ServerFriends);
	ServerFriends.HSplitTop(20.0f, &Button, &ServerFriends);
	if(m_FriendlistSelectedIndex != -1)
	{
		static int s_RemoveButton = 0;
		if(DoButton_Menu(&s_RemoveButton, Localize("Remove"), 0, &Button))
			m_Popup = POPUP_REMOVE_FRIEND;
	}

	// add friend
	if(m_pClient->Friends()->NumFriends() < IFriends::MAX_FRIENDS)
	{
		ServerFriends.HSplitTop(10.0f, 0, &ServerFriends);
		ServerFriends.HSplitTop(19.0f, &Button, &ServerFriends);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
		UI()->DoLabelScaled(&Button, aBuf, FontSize, -1);
		Button.VSplitLeft(80.0f, 0, &Button);
		static char s_aName[MAX_NAME_LENGTH] = {0};
		static float s_OffsetName = 0.0f;
		DoEditBox(&s_aName, &Button, s_aName, sizeof(s_aName), FontSize, &s_OffsetName);

		ServerFriends.HSplitTop(3.0f, 0, &ServerFriends);
		ServerFriends.HSplitTop(19.0f, &Button, &ServerFriends);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
		UI()->DoLabelScaled(&Button, aBuf, FontSize, -1);
		Button.VSplitLeft(80.0f, 0, &Button);
		static char s_aClan[MAX_CLAN_LENGTH] = {0};
		static float s_OffsetClan = 0.0f;
		DoEditBox(&s_aClan, &Button, s_aClan, sizeof(s_aClan), FontSize, &s_OffsetClan);

		ServerFriends.HSplitTop(3.0f, 0, &ServerFriends);
		ServerFriends.HSplitTop(20.0f, &Button, &ServerFriends);
		static int s_AddButton = 0;
		if(DoButton_Menu(&s_AddButton, Localize("Add Friend"), 0, &Button))
		{
			m_pClient->Friends()->AddFriend(s_aName, s_aClan);
			FriendlistOnUpdate();
			Client()->ServerBrowserUpdate();
		}
	}
}

void CMenus::RenderServerbrowser(CUIRect MainView)
{
	CUIRect ServerList, ToolBox, BottomBox, SidebarToggle;

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(6.0f, &MainView);

	MainView.HSplitBottom(36.0f, &MainView, &BottomBox);
	MainView.HSplitBottom(4.0f, &MainView, 0);

	MainView.VSplitRight(20.0f, &ServerList, &SidebarToggle);
	if(g_Config.m_UiSidebar)
	{
		ServerList.VSplitRight(4.0f, &ServerList, 0);
		ServerList.VSplitRight(180.0f, &ServerList, &ToolBox);
		ServerList.VSplitRight(4.0f, &ServerList, 0);
	}

	{
		CUIRect FilterBar, ServerListArea;
		ServerList.HSplitTop(20.0f, &FilterBar, &ServerListArea);
		FilterBar.HSplitBottom(2.0f, &FilterBar, 0);
		RenderFilterPresetBar(FilterBar);
		RenderServerbrowserServerList(ServerListArea);
	}

	{
		CUIRect ToggleBtn;
		SidebarToggle.HMargin(max(0.0f, (SidebarToggle.h - 48.0f) * 0.5f), &ToggleBtn);
		static int s_SidebarToggle = 0;
		if(DoButton_Menu(&s_SidebarToggle, g_Config.m_UiSidebar ? "<" : ">", 0, &ToggleBtn))
			g_Config.m_UiSidebar ^= 1;
	}

	int ToolboxPage = g_Config.m_UiToolboxPage;
	if(g_Config.m_UiSidebar)
	{
		CUIRect TabRow, Content;
		ToolBox.HSplitTop(24.0f, &TabRow, &Content);
		CUIRect Tab0, Tab1, Tab2;
		TabRow.VSplitLeft(TabRow.w / 3.0f, &Tab0, &TabRow);
		TabRow.VSplitLeft(TabRow.w / 2.0f, &Tab1, &Tab2);
		Tab0.VSplitRight(1.0f, &Tab0, 0);
		Tab1.VSplitRight(1.0f, &Tab1, 0);

		static int s_FiltersTab = 0;
		if(DoButton_MenuTab(&s_FiltersTab, Localize("Filter"), ToolboxPage == 0, &Tab0, CUI::CORNER_TL))
			ToolboxPage = 0;
		static int s_InfoTab = 0;
		if(DoButton_MenuTab(&s_InfoTab, Localize("Info"), ToolboxPage == 1, &Tab1, 0))
			ToolboxPage = 1;
		static int s_FriendsTab = 0;
		if(DoButton_MenuTab(&s_FriendsTab, Localize("Friends"), ToolboxPage == 2, &Tab2, CUI::CORNER_TR))
			ToolboxPage = 2;
		g_Config.m_UiToolboxPage = ToolboxPage;

		DrawMenuInset(&Content, CUI::CORNER_B);
		Content.Margin(2.0f, &Content);
		if(ToolboxPage == 0)
			RenderServerbrowserFilters(Content);
		else if(ToolboxPage == 1)
			RenderServerbrowserServerDetail(Content);
		else
			RenderServerbrowserFriends(Content);
	}

	{
		DrawMenuInset(&BottomBox, CUI::CORNER_ALL);
		BottomBox.Margin(4.0f, &BottomBox);

		CUIRect AddrLabel, AddrBox, RefreshBtn, ConnectBtn, Info;
		BottomBox.VSplitRight(90.0f, &BottomBox, &ConnectBtn);
		BottomBox.VSplitRight(4.0f, &BottomBox, 0);
		BottomBox.VSplitRight(90.0f, &BottomBox, &RefreshBtn);
		BottomBox.VSplitRight(8.0f, &BottomBox, 0);
		BottomBox.VSplitLeft(70.0f, &AddrLabel, &BottomBox);
		BottomBox.VSplitLeft(4.0f, 0, &BottomBox);
		BottomBox.VSplitLeft(200.0f, &AddrBox, &Info);
		Info.VSplitLeft(8.0f, 0, &Info);

		UI()->DoLabelScaled(&AddrLabel, Localize("Host address"), 10.0f, -1);
		static float Offset = 0.0f;
		DoEditBox(&g_Config.m_UiServerAddress,
				  &AddrBox,
				  g_Config.m_UiServerAddress,
				  sizeof(g_Config.m_UiServerAddress),
				  10.0f,
				  &Offset);

		static int s_RefreshButton = 0;
		if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshBtn))
		{
			if(g_Config.m_UiPage == PAGE_INTERNET)
				ServerBrowser()->Refresh(m_PlayTab == 2 ? IServerBrowser::TYPE_LAN : IServerBrowser::TYPE_INTERNET);
			else if(g_Config.m_UiPage == PAGE_LAN)
				ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
			else if(g_Config.m_UiPage == PAGE_FAVORITES)
				ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
		}

		static int s_JoinButton = 0;
		if(DoButton_Menu(&s_JoinButton, Localize("Connect"), 0, &ConnectBtn, BUTTONSTYLE_ACCENT) || m_EnterPressed)
		{
			Client()->Connect(g_Config.m_UiServerAddress);
			m_EnterPressed = false;
		}

		char aBuf[128];
		if(str_comp(Client()->LatestVersion(), "0") != 0)
		{
			str_format(aBuf,
					   sizeof(aBuf),
					   Localize("Ninslash %s is out! Download it at www.ninslash.com!"),
					   Client()->LatestVersion());
			TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		}
		else
			str_format(aBuf,
					   sizeof(aBuf),
					   Localize("Current version: %s  |  Servers: %d"),
					   GAME_VERSION,
					   ServerBrowser()->NumServers());
		UI()->DoLabelScaled(&Info, aBuf, 10.0f, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CMenus::ConchainFriendlistUpdate(IConsole::IResult *pResult,
									  void *pUserData,
									  IConsole::FCommandCallback pfnCallback,
									  void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 2 && ((CMenus *)pUserData)->Client()->State() == IClient::STATE_OFFLINE)
	{
		((CMenus *)pUserData)->FriendlistOnUpdate();
		((CMenus *)pUserData)->Client()->ServerBrowserUpdate();
	}
}

void CMenus::ConchainServerbrowserUpdate(IConsole::IResult *pResult,
										 void *pUserData,
										 IConsole::FCommandCallback pfnCallback,
										 void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && g_Config.m_UiPage == PAGE_FAVORITES &&
	   ((CMenus *)pUserData)->Client()->State() == IClient::STATE_OFFLINE)
		((CMenus *)pUserData)->ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
}
