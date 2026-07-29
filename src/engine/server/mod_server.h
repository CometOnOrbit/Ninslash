#ifndef ENGINE_SERVER_MOD_SERVER_H
#define ENGINE_SERVER_MOD_SERVER_H

#include <engine/shared/mod_api.h>

class CModServerRuntime
{
	struct CImpl;
	CImpl *m_pImpl;
public:
	CModServerRuntime();
	~CModServerRuntime();
	bool Load(const char *pWorkshopRoot,const char *pRootIDs,const char *pProtocol,const char *pExpectedHash,char *pError,int ErrorSize);
	void Unload();
	bool Active() const;
	void Dispatch(EModEvent Event,int ClientID,int Value);
};

#endif
