#include <cassert>
#include <cstdio>
#include <game/pve/replay_rules.h>

static int ContractMask(int Mode)
{
	int Mask = 0;
	for(int ID = 0; ID < NUM_PVE_CONTRACTS; ID++)
		if(PveContractAvailableInMode(ID, Mode))
			Mask |= 1 << ID;
	return Mask;
}

static void TestContracts()
{
	for(int Mode = PVE_MODE_INVASION; Mode <= PVE_MODE_EXTRACTION; Mode++)
	{
		const int Available = ContractMask(Mode);
		CDeterministicRandom Rng(1337 + Mode);
		int Used = 0;
		int Previous[2] = {-1, -1};
		int InitialSeen = 0;
		bool FirstCycle = true;
		for(int Round = 0; Round < 1000; Round++)
		{
			int Offers[2] = {-1, -1};
			const bool Full = (Used & Available) == Available;
			assert(PveContractOffers(Mode, Used, Previous, Offers, Rng));
			assert(Offers[0] != Offers[1]);
			for(int Slot = 0; Slot < 2; Slot++)
			{
				const int ID = Offers[Slot];
				assert(ID >= 0 && ID < NUM_PVE_CONTRACTS);
				assert(PveContractAvailableInMode(ID, Mode));
				if(FirstCycle)
				{
					assert(!(InitialSeen & (1 << ID)));
					InitialSeen |= 1 << ID;
					FirstCycle = InitialSeen != Available;
				}
				if(Full)
					assert(ID != Previous[0] && ID != Previous[1]);
			}
			Previous[0] = Offers[0];
			Previous[1] = Offers[1];
		}
		assert(InitialSeen == Available);
		// An old/exhausted saved mask must not permanently disable voting.
		Used = (1 << NUM_PVE_CONTRACTS) - 1;
		int Offers[2];
		assert(PveContractOffers(Mode, Used, Previous, Offers, Rng));
		assert(!(Used & ~Available));
		// A single remaining card is not silently discarded on refill.
		int Last = 0;
		while(!(Available & (1 << Last)))
			Last++;
		Used = Available & ~(1 << Last);
		assert(PveContractOffers(Mode, Used, Previous, Offers, Rng));
		assert(Offers[0] == Last && Offers[1] != Last);
		// Same seed/state, including in-place history, produces the same offers.
		int U1 = 0, U2 = 0, A[2] = {-1, -1}, B[2] = {-1, -1};
		CDeterministicRandom R1(91), R2(91);
		for(int Round = 0; Round < 100; Round++)
		{
			assert(PveContractOffers(Mode, U1, A, A, R1));
			assert(PveContractOffers(Mode, U2, B, B, R2));
			assert(U1 == U2 && A[0] == B[0] && A[1] == B[1]);
		}
	}
}

static void TestCards()
{
	CDeterministicRandom Rng(827);
	for(int Mode = PVE_MODE_INVASION; Mode <= PVE_MODE_EXTRACTION; Mode++)
	{
		bool Eligible[NUM_PVE_CARDS];
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			Eligible[ID] = PveCardDef(ID)->m_Mode == PVE_MODE_ANY || PveCardDef(ID)->m_Mode == Mode;
		for(int Spec = PVE_SPECIALIZATION_FIREARM; Spec <= PVE_SPECIALIZATION_MELEE; Spec++)
		{
			int Previous[3] = {-1, -1, -1};
			int LastChosen = -1;
			for(int Round = 0; Round < 250; Round++)
			{
				int Offers[3];
				PveReplayChoices(Eligible, Previous, LastChosen, Spec, Offers, Rng);
				assert(PveCardDef(Offers[0])->m_Specialization == Spec);
				assert(PveCardDef(Offers[1])->m_Specialization == PVE_SPECIALIZATION_NONE);
				for(int i = 0; i < 3; i++)
				{
					assert(Offers[i] >= 0 && Offers[i] < NUM_PVE_CARDS && Eligible[Offers[i]]);
					for(int j = 0; j < i; j++)
						assert(Offers[j] != Offers[i]);
					for(int j = 0; j < 3; j++)
						assert(Previous[j] == LastChosen || Offers[i] != Previous[j]);
				}
				LastChosen = Offers[Round % 3];
				for(int i = 0; i < 3; i++)
					Previous[i] = Offers[i];
			}
		}
	}
	// All gating (research, stacks, legendary, drone chassis) is supplied by
	// CardEligible. An empty/tiny eligible pool must stay valid and terminate.
	bool Eligible[NUM_PVE_CARDS] = {false};
	int Previous[3] = {-1, -1, -1};
	int Offers[3];
	PveReplayChoices(Eligible, Previous, -1, PVE_SPECIALIZATION_FIREARM, Offers, Rng);
	assert(Offers[0] == PVE_SUPPLY_ARMOR && Offers[1] == PVE_SUPPLY_AMMO && Offers[2] == PVE_SUPPLY_KITS);
	Eligible[PVE_CARD_COMBAT_TRAINING] = true;
	Previous[0] = PVE_CARD_COMBAT_TRAINING;
	PveReplayChoices(Eligible, Previous, -1, PVE_SPECIALIZATION_FIREARM, Offers, Rng);
	assert(Offers[0] == PVE_CARD_COMBAT_TRAINING);
	assert(Offers[1] == PVE_SUPPLY_AMMO && Offers[2] == PVE_SUPPLY_KITS);
	Eligible[PVE_CARD_REINFORCED_PLATES] = true;
	bool Excluded[NUM_PVE_CARDS] = {false};
	bool Recent[NUM_PVE_CARDS] = {false};
	Recent[PVE_CARD_COMBAT_TRAINING] = true;
	for(int n = 0; n < 100; n++)
		assert(PveDrawReplayCard(Eligible, Excluded, Recent, -1, Rng) == PVE_CARD_REINFORCED_PLATES);
	// No recent exclusion => either card can recur for an existing stack build.
	Recent[PVE_CARD_COMBAT_TRAINING] = false;
	int Seen = 0;
	for(int n = 0; n < 100; n++)
		Seen |= 1 << PveDrawReplayCard(Eligible, Excluded, Recent, -1, Rng);
	assert((Seen & 3) == 3);
	Excluded[PVE_CARD_COMBAT_TRAINING] = true;
	assert(PveDrawReplayCard(Eligible, Excluded, Recent, -1, Rng) == PVE_CARD_REINFORCED_PLATES);
}

static void TestHorde()
{
	assert(!PveHordeEventDue(1, -1, 0.0f));
	assert(!PveHordeEventDue(2, -1, 0.0f));
	assert(PveHordeEventDue(3, -1, 0.0f));
	assert(!PveHordeEventDue(4, 3, 0.0f));
	assert(PveHordeEventDue(5, -1, 0.999f));
	assert(!PveHordeEventDue(6, 3, 0.999f));
	assert(PveHordeEventDue(7, 3, 0.999f));
	int IDs[10], Weights[10];
	for(int i = 0; i < 10; i++)
	{
		IDs[i] = i + 1;
		Weights[i] = 14 - i;
	}
	CDeterministicRandom Rng(1337);
	int Last = 0, Previous = 0, LastWave = -1, Events = 0;
	for(int Wave = 1; Wave <= 10000; Wave++)
	{
		if(!PveHordeEventDue(Wave, LastWave, Rng.NextFloat()))
			continue;
		assert(LastWave < 0 || (Wave - LastWave >= 2 && Wave - LastWave <= 4));
		const int Event = PveChooseRecentEvent(IDs, Weights, 10, Last, Previous, Rng);
		assert(Event > 0 && Event <= 10 && Event != Last && Event != Previous);
		Previous = Last;
		Last = Event;
		LastWave = Wave;
		Events++;
	}
	assert(Events >= 2499 && Events <= 5000);
	assert(PveChooseRecentEvent(IDs, Weights, 1, IDs[0], 0, Rng) == IDs[0]);
	assert(PveChooseRecentEvent(IDs, Weights, 0, 0, 0, Rng) == -1);
	Weights[0] = 0;
	assert(PveChooseRecentEvent(IDs, Weights, 1, 0, 0, Rng) == -1);
	int Deadline = PveEmptyRunDeadline(100, 50, false, 0);
	assert(Deadline == 600);
	assert(PveEmptyRunDeadline(200, 50, false, Deadline) == 600);
	assert(PveEmptyRunDeadline(599, 50, true, Deadline) == 0);
	assert(PveEmptyRunDeadline(601, 50, true, Deadline) == 0);
	assert(PveEmptyRunDeadline(700, 50, false, 0) == 1200);
	std::printf("horde: %d varied events across 10000 waves\n", Events);
}

int main()
{
	char Error[256] = {0};
	if(!PveValidateDefinitions(Error, sizeof(Error)))
	{
		std::fprintf(stderr, "%s\n", Error);
		return 1;
	}
	TestContracts();
	TestCards();
	TestHorde();
	std::puts("PASS: contract cycling, card choices, event variety/pacing, empty-server rejoin");
	return 0;
}
