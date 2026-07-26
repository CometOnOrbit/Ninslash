#ifndef ENGINE_PLATFORM_SERVICES_H
#define ENGINE_PLATFORM_SERVICES_H

#include "kernel.h"

class IPlatformServices : public IInterface
{
	MACRO_INTERFACE("platformservices", 0)

public:
	virtual bool Init() = 0;
	virtual void Shutdown() = 0;
	virtual void RunCallbacks() = 0;
	virtual bool Available() const = 0;
	virtual const char *PlatformName() const = 0;
	virtual unsigned long long LocalUserID() const = 0;
	// The caller owns no ticket memory. A zero-length ticket is a valid
	// standalone response and must never be used to bypass a required server.
	virtual int GetAuthSessionTicket(void *pBuffer, int BufferSize) = 0;
	virtual void SetRichPresence(const char *pStatus, const char *pConnect) = 0;
	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize) = 0;
};

IPlatformServices *CreatePlatformServices();

#endif
