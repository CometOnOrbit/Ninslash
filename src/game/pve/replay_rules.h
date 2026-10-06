#ifndef GAME_PVE_REPLAY_RULES_H
#define GAME_PVE_REPLAY_RULES_H

#include <base/deterministic_random.h>
#include <game/pve/pve_roguelite.h>

// Offer history is a deck, not a lifetime ban. Preserve an odd leftover before
// refilling, and prefer not to repeat the previous pair at a cycle boundary.
// The existing save-game mask and stable contract IDs remain unchanged.
inline bool PveContractOffers(int Mode, int &Used, const int *pPrevious, int *pOffers, CDeterministicRandom &Rng)
{
	int Available = 0;
	int Count = 0;
	int Previous = 0;
	for(int ID = 0; ID < NUM_PVE_CONTRACTS; ID++)
		if(PveContractAvailableInMode(ID, Mode))
		{
			Available |= 1 << ID;
			Count++;
		}
	if(Count < 2)
		return false;
	for(int i = 0; i < 2; i++)
		if(pPrevious[i] >= 0 && pPrevious[i] < NUM_PVE_CONTRACTS)
			Previous |= 1 << pPrevious[i];
	Used &= Available;
	int Offered = 0;
	bool Refilled = false;
	for(int Slot = 0; Slot < 2; Slot++)
	{
		int Pool = Available & ~Used & ~Offered;
		if(!Pool)
		{
			Used = Offered;
			Pool = Available & ~Used;
			Refilled = true;
		}
		if(Refilled && (Pool & ~Previous))
			Pool &= ~Previous;
		Count = 0;
		for(int ID = 0; ID < NUM_PVE_CONTRACTS; ID++)
			if(Pool & (1 << ID))
				Count++;
		int Pick = Rng.NextInt(Count);
		for(int ID = 0; ID < NUM_PVE_CONTRACTS; ID++)
			if((Pool & (1 << ID)) && Pick-- == 0)
			{
				pOffers[Slot] = ID;
				Offered |= 1 << ID;
				Used |= 1 << ID;
				break;
			}
	}
	return true;
}

// Filter -1 means any card, 0 means a general (non-weapon-specific) card.
// Keep the original rarity odds among eligible tiers. Recently declined cards
// are a soft exclusion: a small pool must never make a valid build unavailable.
inline int PveDrawReplayCard(
	const bool *pEligible, const bool *pExcluded, const bool *pRecent, int Specialization, CDeterministicRandom &Rng)
{
	for(int Pass = 0; Pass < 2; Pass++)
	{
		int aaCards[4][NUM_PVE_CARDS];
		int aCounts[4] = {0, 0, 0, 0};
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		{
			if(!pEligible[ID] || pExcluded[ID] || (Pass == 0 && pRecent[ID]))
				continue;
			const CPveCardDef *pDef = PveCardDef(ID);
			if(Specialization >= 0 && pDef->m_Specialization != Specialization)
				continue;
			aaCards[pDef->m_Rarity][aCounts[pDef->m_Rarity]++] = ID;
		}
		const int aWeights[4] = {55, 30, 10, 5};
		int Total = 0;
		for(int Rarity = 0; Rarity < 4; Rarity++)
			if(aCounts[Rarity])
				Total += aWeights[Rarity];
		if(!Total)
			continue;
		int Pick = Rng.NextInt(Total);
		for(int Rarity = 0; Rarity < 4; Rarity++)
		{
			if(!aCounts[Rarity])
				continue;
			if(Pick < aWeights[Rarity])
				return aaCards[Rarity][Rng.NextInt(aCounts[Rarity])];
			Pick -= aWeights[Rarity];
		}
	}
	return -1;
}

inline void PveReplayChoices(const bool *pEligible,
							 const int *pPrevious,
							 int LastChosen,
							 int Specialization,
							 int *pOffers,
							 CDeterministicRandom &Rng)
{
	bool aExcluded[NUM_PVE_CARDS] = {false};
	bool aRecent[NUM_PVE_CARDS] = {false};
	for(int Slot = 0; Slot < 3; Slot++)
		if(pPrevious[Slot] >= 0 && pPrevious[Slot] < NUM_PVE_CARDS && pPrevious[Slot] != LastChosen)
			aRecent[pPrevious[Slot]] = true;
	const int aFilters[3] = {
		Specialization > PVE_SPECIALIZATION_NONE ? Specialization : -1, PVE_SPECIALIZATION_NONE, -1};
	const int aFallbacks[3] = {PVE_SUPPLY_ARMOR, PVE_SUPPLY_AMMO, PVE_SUPPLY_KITS};
	for(int Slot = 0; Slot < 3; Slot++)
	{
		int ID = PveDrawReplayCard(pEligible, aExcluded, aRecent, aFilters[Slot], Rng);
		if(ID < 0 && aFilters[Slot] >= 0)
			ID = PveDrawReplayCard(pEligible, aExcluded, aRecent, -1, Rng);
		pOffers[Slot] = ID >= 0 ? ID : aFallbacks[Slot];
		if(ID >= 0)
			aExcluded[ID] = true;
	}
}

// A quiet wave between events remains mandatory. After that, preserve the
// old 35% chance, but cap dry streaks at four waves (first event by wave 5).
inline bool PveHordeEventDue(int Wave, int LastEventWave, float Roll)
{
	if(Wave < 3 || (LastEventWave >= 0 && Wave <= LastEventWave + 1))
		return false;
	return Wave - (LastEventWave >= 0 ? LastEventWave : 1) >= 4 || Roll < 0.35f;
}

inline int PveChooseRecentEvent(const int *pIDs, const int *pWeights, int Count, int Last, int Previous, CDeterministicRandom &Rng)
{
	for(int Pass = 0; Pass < 2; Pass++)
	{
		int Total = 0;
		for(int i = 0; i < Count; i++)
			if(pWeights[i] > 0 && (Pass || (pIDs[i] != Last && pIDs[i] != Previous)))
				Total += pWeights[i];
		if(Total <= 0)
			continue;
		int Roll = Rng.NextInt(Total);
		for(int i = 0; i < Count; i++)
		{
			if(pWeights[i] <= 0 || (!Pass && (pIDs[i] == Last || pIDs[i] == Previous)))
				continue;
			if(Roll < pWeights[i])
				return pIDs[i];
			Roll -= pWeights[i];
		}
	}
	return -1;
}

// Rejoining within the grace period cancels the pending empty-server reset.
inline int PveEmptyRunDeadline(int Now, int TickSpeed, bool HasHumans, int Deadline)
{
	return HasHumans ? 0 : (Deadline > 0 ? Deadline : Now + TickSpeed * 10);
}

#endif
