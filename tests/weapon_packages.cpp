#include <game/weapon_catalog.h>
#include <game/forge.h>
#include <game/weapon_packages.h>
#include <game/weapon_script_runtime.h>
#include <game/weapon_presentation_runtime.h>
#include <engine/shared/content_package.h>
#include <engine/shared/content_package_index.h>
#include <engine/shared/content_collection.h>
#include <base/system.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

namespace
{
class CScriptHost : public IWeaponScriptHost
{
	int m_aState[8]{};

  public:
	int ScriptStateGet(int Index) const override { return m_aState[Index]; }
	void ScriptStateSet(int Index, int Value) override { m_aState[Index] = Value; }
	uint32_t ScriptRandom() override { return 1; }
	bool ScriptCommand(const CWeaponScriptCommand &) override { return true; }
};

class CPresentationHost : public IWeaponPresentationHost
{
  public:
	int PresentationStateGet(int) const override { return 1; }
	void PresentationText(const char *, int, int, int) override {}
	void PresentationBar(int, int, int, int, int, int) override {}
};

void WriteFile(const char *pPath, const char *pText)
{
	IOHANDLE File = io_open(pPath, IOFLAG_WRITE);
	assert(File);
	const int Length = str_length(pText);
	assert(io_write(File, pText, Length) == (unsigned)Length);
	io_close(File);
}

void ReadFile(const char *pPath, char *pBuffer, int BufferSize)
{
	IOHANDLE File = io_open(pPath, IOFLAG_READ);
	assert(File);
	const long Length = io_length(File);
	assert(Length >= 0 && Length < BufferSize);
	assert(io_read(File, pBuffer, (unsigned)Length) == (unsigned)Length);
	io_close(File);
	pBuffer[Length] = 0;
}
} // namespace

int main()
{
	const char *pProtocol = "0.5.1 abc123-luaweapons6-pvp-spectator-challenge";
	char aError[256];
	CContentManifest SourceManifest;
	assert(ContentPackageValidate(
		"examples/workshop_weapon", "9000000001", pProtocol, &SourceManifest, aError, sizeof(aError)));
	char aRoot[256], aWorkshop[300], aStaged[512];
	str_format(aRoot, sizeof(aRoot), "/tmp/ninslash-weapon-package-%lld", (long long)time_get());
	assert(fs_makedir(aRoot) == 0);
	str_format(aWorkshop, sizeof(aWorkshop), "%s/workshop", aRoot);
	CContentManifest StagedManifest;
	assert(ContentPackageStage("examples/workshop_weapon",
							   aWorkshop,
							   "9000000001",
							   pProtocol,
							   &StagedManifest,
							   aStaged,
							   sizeof(aStaged),
							   aError,
							   sizeof(aError)));
	assert(strcmp(StagedManifest.m_aContentHash, SourceManifest.m_aContentHash) == 0);
	char aFriendly[512];
	str_format(aFriendly, sizeof(aFriendly), "%s/plasma-carbine-example", aWorkshop);
	assert(fs_rename(aStaged, aFriendly) == 0);
	str_copy(aStaged, aFriendly, sizeof(aStaged));
	CContentPackageIndex Index;
	assert(Index.Scan(aWorkshop, pProtocol, aError, sizeof(aError)));
	const CContentPackageIndex::CEntry *pIndexed = Index.Find("9000000001");
	assert(pIndexed && strcmp(pIndexed->m_aDirectory, "plasma-carbine-example") == 0);
	CContentCollection Collection;
	assert(Collection.AddManifest(StagedManifest, aStaged, aError, sizeof(aError)));
	const char *apRoots[] = {"9000000001"};
	int aOrder[2], OrderCount = 0;
	char aCollectionHash[65];
	assert(Collection.Resolve(apRoots, 1, aOrder, &OrderCount, aCollectionHash, aError, sizeof(aError)));
	char aResolvedHash[65];
	assert(WeaponPackagesResolveCollectionHash(
		aWorkshop, "9000000001", pProtocol, aResolvedHash, sizeof(aResolvedHash), aError, sizeof(aError)));
	assert(strcmp(aResolvedHash, aCollectionHash) == 0);
	assert(WeaponPackagesResolveCollectionHash(
		aWorkshop, "", pProtocol, aResolvedHash, sizeof(aResolvedHash), aError, sizeof(aError)));
	assert(aResolvedHash[0] == 0);
	assert(!WeaponPackagesLoadCollection(aWorkshop, "9000000001", pProtocol, "", aError, sizeof(aError)));
	const bool Loaded =
		WeaponPackagesLoadCollection(aWorkshop, "9000000001", pProtocol, aCollectionHash, aError, sizeof(aError));
	if(!Loaded)
		fprintf(stderr, "%s\n", aError);
	assert(Loaded);
	CWeaponSpec Spec;
	assert(CWeaponCatalog::TryFromStableId("workshop:9000000001:plasma-carbine", 4, &Spec));
	assert((int)Spec.m_DefinitionId == WEAPON_DEFINITION_COUNT + 1);
	CWeaponSpec Variant;
	assert(CWeaponCatalog::TryFromStableId("workshop:9000000001:plasma-variant", 0, &Variant));
	assert((int)Variant.m_DefinitionId == WEAPON_DEFINITION_COUNT + 2);
	CWeaponDefinition Definition;
	assert(CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, &Definition));
	assert(strcmp(Definition.m_aNameKey, "Plasma Carbine") == 0);
	assert(strcmp(Definition.m_aHeldImage, "workshop:9000000001:resources/plasma_carbine.png") == 0);
	assert(CWeaponScriptRuntime::HasHandler("workshop:9000000001:plasma-carbine", EWeaponScriptEvent::Fire));
	assert(CWeaponPresentationRuntime::HasHudHandler("workshop:9000000001:plasma-carbine"));
	CScriptHost ScriptHost;
	CPresentationHost PresentationHost;
	assert(CWeaponScriptRuntime::Dispatch(
		"workshop:9000000001:plasma-carbine", EWeaponScriptEvent::Fire, &ScriptHost, aError, sizeof(aError)));
	assert(CWeaponPresentationRuntime::RenderHud(
		"workshop:9000000001:plasma-carbine", &PresentationHost, aError, sizeof(aError)));
	assert(WeaponPackagesLoadCollection(aWorkshop, "9000000001", pProtocol, aCollectionHash, aError, sizeof(aError)));
	assert(CWeaponScriptRuntime::Dispatch(
		"workshop:9000000001:plasma-carbine", EWeaponScriptEvent::Fire, &ScriptHost, aError, sizeof(aError)));
	assert(CWeaponPresentationRuntime::RenderHud(
		"workshop:9000000001:plasma-carbine", &PresentationHost, aError, sizeof(aError)));
	CResolvedWeaponProfile Profile;
	assert(CWeaponCatalog::TryResolve(Spec, &Profile));
	assert(Profile.m_Combat.m_ProjectileDamage == 22.0f);
	CWeaponSpec Combination;
	for(int DefinitionIndex = WEAPON_DEFINITION_COUNT; DefinitionIndex < CWeaponCatalog::DefinitionCount();
		++DefinitionIndex)
	{
		CWeaponDefinition Candidate;
		if(CWeaponCatalog::TryGetDefinitionByIndex(DefinitionIndex, &Candidate) &&
		   strcmp(Candidate.m_aComposeId, "plasma-combination") == 0)
		{
			Combination = {Candidate.m_Id, 0};
			break;
		}
	}
	assert(Combination.IsValid());
	const CForgeRecipe Recipe =
		CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 2), Combination, 5, 10, 3, 7);
	assert(Recipe.m_Result == FORGERESULT_SUCCESS && Recipe.m_Operation == 3 && Recipe.m_Cost == 16);

	// A replacement may pass manifest/hash validation and still fail while its
	// Lua definitions are loaded. The active collection must survive that failure.
	char aDefinitionPath[1024], aManifestPath[1024];
	str_format(aDefinitionPath, sizeof(aDefinitionPath), "%s/plasma.weapon.lua", aStaged);
	str_format(aManifestPath, sizeof(aManifestPath), "%s/ninslash_content.json", aStaged);
	WriteFile(aDefinitionPath, "this is not valid Lua");
	char aInvalidContentHash[65];
	assert(ContentPackageComputeHash(aStaged, StagedManifest, aInvalidContentHash, aError, sizeof(aError)));
	char aManifestJson[32768];
	ReadFile(aManifestPath, aManifestJson, sizeof(aManifestJson));
	char *pOldHash = strstr(aManifestJson, StagedManifest.m_aContentHash);
	assert(pOldHash && strlen(StagedManifest.m_aContentHash) == strlen(aInvalidContentHash));
	mem_copy(pOldHash, aInvalidContentHash, strlen(aInvalidContentHash));
	WriteFile(aManifestPath, aManifestJson);
	assert(ContentPackageValidate(aStaged, "9000000001", pProtocol, &StagedManifest, aError, sizeof(aError)));
	char aInvalidCollectionHash[65];
	assert(WeaponPackagesResolveCollectionHash(aWorkshop,
											   "9000000001",
											   pProtocol,
											   aInvalidCollectionHash,
											   sizeof(aInvalidCollectionHash),
											   aError,
											   sizeof(aError)));
	assert(!WeaponPackagesLoadCollection(
		aWorkshop, "9000000001", pProtocol, aInvalidCollectionHash, aError, sizeof(aError)));
	assert(CWeaponCatalog::TryFromStableId("workshop:9000000001:plasma-carbine", 4, &Spec));
	assert(CWeaponCatalog::TryResolve(Spec, &Profile));
	assert(Profile.m_Combat.m_ProjectileDamage == 22.0f);
	assert(CWeaponScriptRuntime::HasHandler("workshop:9000000001:plasma-carbine", EWeaponScriptEvent::Fire));
	assert(CWeaponPresentationRuntime::HasHudHandler("workshop:9000000001:plasma-carbine"));
	assert(CWeaponScriptRuntime::Dispatch(
		"workshop:9000000001:plasma-carbine", EWeaponScriptEvent::Fire, &ScriptHost, aError, sizeof(aError)));
	assert(CWeaponPresentationRuntime::RenderHud(
		"workshop:9000000001:plasma-carbine", &PresentationHost, aError, sizeof(aError)));

	char aDuplicate[512];
	assert(ContentPackageStage(
		aStaged, aWorkshop, "9000000001", pProtocol, 0, aDuplicate, sizeof(aDuplicate), aError, sizeof(aError)));
	assert(Index.Scan(aWorkshop, pProtocol, aError, sizeof(aError)));
	assert(Index.Find("9000000001") == 0);
	assert(Index.Count() == 2);
	assert(!Index.Get(0)->m_Valid && !Index.Get(1)->m_Valid);
	assert(!WeaponPackagesLoadCollection(
		aWorkshop, "9000000001", pProtocol, aInvalidCollectionHash, aError, sizeof(aError)));
	assert(CWeaponCatalog::TryFromStableId("workshop:9000000001:plasma-carbine", 4, &Spec));
	assert(CWeaponScriptRuntime::HasHandler("workshop:9000000001:plasma-carbine", EWeaponScriptEvent::Fire));
	assert(CWeaponPresentationRuntime::HasHudHandler("workshop:9000000001:plasma-carbine"));
	CWeaponCatalog::ResetCustomDefinitions();
	CWeaponScriptRuntime::Reset();
	CWeaponPresentationRuntime::Reset();
	return 0;
}
