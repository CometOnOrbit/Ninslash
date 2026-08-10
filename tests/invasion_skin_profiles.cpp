#include <cassert>
#include <cstdio>

#include <game/questinfo.h>
#include <game/weapon_catalog.h>
#include <game/server/ai/inv/invasion_profile.h>

static void CheckWeapon(const CWeaponSpec &Spec, bool RequireAttackRange)
{
	assert(Spec.IsValid());
	CResolvedWeaponProfile Resolved;
	assert(CWeaponCatalog::TryResolve(Spec, &Resolved));
	if(RequireAttackRange)
		assert(Resolved.m_Combat.m_AiAttackRange > 0);
}

int main()
{
	char aError[256];
	if(!CWeaponCatalog::Initialize(aError, sizeof(aError)))
	{
		std::fprintf(stderr, "weapon initialization failed: %s\n", aError);
		return 1;
	}

	for(int Id = 0; Id < NUM_INVASION_SKINS; ++Id)
	{
		const EInvasionSkinId ProfileId = static_cast<EInvasionSkinId>(Id);
		const CInvasionSkinProfile &Profile = InvasionSkinProfile(ProfileId);
		assert(Profile.m_Id == ProfileId);
		assert(Profile.m_PrimaryCount > 0);
		assert(Profile.m_PrimaryCount <= INVASION_PROFILE_MAX_PRIMARY_CHOICES);
		assert(Profile.m_UtilityCount >= 0 && Profile.m_UtilityCount <= INVASION_PROFILE_MAX_UTILITY_WEAPONS);
		assert(Profile.m_PreferredRange > 0);
		assert(Profile.m_RetreatRange > 0);
		for(int Choice = 0; Choice < Profile.m_PrimaryCount; ++Choice)
			CheckWeapon(Profile.m_aPrimaryChoices[Choice], true);
		for(int Choice = 0; Choice < Profile.m_UtilityCount; ++Choice)
			CheckWeapon(Profile.m_aUtilityWeapons[Choice], false);
	}

	for(int Wave = WAVE_ALIENS; Wave <= WAVE_CYBORGS; ++Wave)
	{
		for(int Level = 0; Level < 8; ++Level)
		{
			const EInvasionSkinId Normal = InvasionSkinForWave(Wave, Level, false);
			const EInvasionSkinId Elite = InvasionSkinForWave(Wave, Level, true);
			assert(IsValidInvasionSkinProfile(Normal));
			assert(IsValidInvasionSkinProfile(Elite));
			assert(InvasionSkinProfile(Normal).m_PrimaryCount > 0);
			assert(InvasionSkinProfile(Elite).m_PrimaryCount > 0);
		}
	}

	assert(!IsValidInvasionSkinProfile(INVASION_SKIN_INVALID));
	const CInvasionSkinProfile &Fallback = InvasionSkinProfile(INVASION_SKIN_INVALID);
	assert(Fallback.m_PrimaryCount > 0);
	CheckWeapon(Fallback.m_aPrimaryChoices[0], true);
	return 0;
}
