#ifndef GAME_WEAPON_PRESENTATION_RUNTIME_H
#define GAME_WEAPON_PRESENTATION_RUNTIME_H

class IWeaponPresentationHost
{
  public:
	virtual ~IWeaponPresentationHost() {}
	virtual int PresentationStateGet(int Index) const = 0;
	virtual void PresentationText(const char *pText, int X, int Y, int Size) = 0;
	virtual void PresentationBar(int Value, int Maximum, int X, int Y, int Width, int Height) = 0;
};

class CWeaponPresentationRuntime
{
  public:
	static void Reset();
	static void BeginReload();
	static void CommitReload();
	static void RollbackReload();
	static bool LoadPackageScript(
		const char *pPackageId, const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize);
	static bool HasHudHandler(const char *pStableId);
	static bool RenderHud(const char *pStableId, IWeaponPresentationHost *pHost, char *pError = 0, int ErrorSize = 0);
};

#endif
