#include "platform_ban.h"

#include <base/system.h>
#include <engine/storage.h>
#include <engine/shared/linereader.h>
#include <stdio.h>

CPlatformBanList::CPlatformBanList() : m_Count(0), m_pStorage(0)
{
	mem_zero(m_aBans, sizeof(m_aBans));
}
void CPlatformBanList::Init(IStorage *pStorage)
{
	m_pStorage = pStorage;
	Load();
}

void CPlatformBanList::PurgeExpired()
{
	const int Now = time_timestamp();
	for(int i = 0; i < m_Count;)
	{
		if(m_aBans[i].m_Expires > 0 && m_aBans[i].m_Expires <= Now)
			m_aBans[i] = m_aBans[--m_Count];
		else
			i++;
	}
}

bool CPlatformBanList::Load()
{
	m_Count = 0;
	if(!m_pStorage)
		return false;
	IOHANDLE File = m_pStorage->OpenFile("steam_bans.cfg", IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return true;
	CLineReader Reader;
	Reader.Init(File);
	for(char *pLine = Reader.Get(); pLine && m_Count < MAX_BANS; pLine = Reader.Get())
	{
		unsigned long long SteamID = 0;
		int Expires = 0, Offset = 0;
		char Trailing = 0;
		if(sscanf(pLine, "%llu %d %n%c", &SteamID, &Expires, &Offset, &Trailing) < 2 || !SteamID)
			continue;
		CBan &Ban = m_aBans[m_Count++];
		Ban.m_SteamID = SteamID;
		Ban.m_Expires = Expires;
		str_copy(Ban.m_aReason, pLine[Offset] ? pLine + Offset : "Banned", sizeof(Ban.m_aReason));
	}
	Reader.Shutdown();
	PurgeExpired();
	return true;
}

bool CPlatformBanList::Save()
{
	if(!m_pStorage)
		return false;
	PurgeExpired();
	IOHANDLE File = m_pStorage->OpenFile("steam_bans.cfg.tmp", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	for(int i = 0; i < m_Count; i++)
	{
		char aLine[256];
		str_format(
			aLine, sizeof(aLine), "%llu %d %s\n", m_aBans[i].m_SteamID, m_aBans[i].m_Expires, m_aBans[i].m_aReason);
		io_write(File, aLine, str_length(aLine));
	}
	io_close(File);
	m_pStorage->RemoveFile("steam_bans.cfg", IStorage::TYPE_SAVE);
	return m_pStorage->RenameFile("steam_bans.cfg.tmp", "steam_bans.cfg", IStorage::TYPE_SAVE);
}

bool CPlatformBanList::Ban(unsigned long long SteamID, int Seconds, const char *pReason)
{
	if(!SteamID || Seconds < 0)
		return false;
	PurgeExpired();
	int Index = -1;
	for(int i = 0; i < m_Count; i++)
		if(m_aBans[i].m_SteamID == SteamID)
			Index = i;
	if(Index < 0)
	{
		if(m_Count >= MAX_BANS)
			return false;
		Index = m_Count++;
	}
	m_aBans[Index].m_SteamID = SteamID;
	m_aBans[Index].m_Expires = Seconds ? time_timestamp() + Seconds : 0;
	str_copy(m_aBans[Index].m_aReason, pReason && pReason[0] ? pReason : "Banned", sizeof(m_aBans[Index].m_aReason));
	return Save();
}

bool CPlatformBanList::Unban(unsigned long long SteamID)
{
	for(int i = 0; i < m_Count; i++)
		if(m_aBans[i].m_SteamID == SteamID)
		{
			m_aBans[i] = m_aBans[--m_Count];
			return Save();
		}
	return false;
}

bool CPlatformBanList::IsBanned(unsigned long long SteamID, char *pReason, int ReasonSize)
{
	PurgeExpired();
	for(int i = 0; i < m_Count; i++)
		if(m_aBans[i].m_SteamID == SteamID)
		{
			if(pReason && ReasonSize > 0)
				str_copy(pReason, m_aBans[i].m_aReason, ReasonSize);
			return true;
		}
	return false;
}
