#ifndef ENGINE_SHARED_MOD_RUNTIME_H
#define ENGINE_SHARED_MOD_RUNTIME_H

#include "mod_api.h"

class ILuaModRuntime : public IModRuntime, public IModEventSink
{
  public:
	virtual bool LoadScript(const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize) = 0;
	virtual void SetRandomSeed(unsigned int Seed) = 0;
	virtual int MemoryUsed() const = 0;
};

ILuaModRuntime *CreateLuaModRuntime();

#endif
