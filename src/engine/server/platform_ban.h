#ifndef ENGINE_SERVER_PLATFORM_BAN_H
#define ENGINE_SERVER_PLATFORM_BAN_H

class IStorage;

class CPlatformBanList
{
  public:
	enum
	{
		MAX_BANS = 1024
	};
	struct CBan
	{
		unsigned long long m_SteamID;
		int m_Expires;
		char m_aReason[128];
	};

  private:
	CBan m_aBans[MAX_BANS];
	int m_Count;
	IStorage *m_pStorage;
	void PurgeExpired();

  public:
	CPlatformBanList();
	void Init(IStorage *pStorage);
	bool Load();
	bool Save();
	bool Ban(unsigned long long SteamID, int Seconds, const char *pReason);
	bool Unban(unsigned long long SteamID);
	bool IsBanned(unsigned long long SteamID, char *pReason, int ReasonSize);
	int Count() const { return m_Count; }
	const CBan *Get(int Index) const { return Index >= 0 && Index < m_Count ? &m_aBans[Index] : 0; }
};

#endif
