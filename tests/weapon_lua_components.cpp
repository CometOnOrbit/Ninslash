#include <assert.h>
#include <string.h>

#include <string>

// Compile the loader into this isolated test executable so fault injection can
// exercise anonymous-namespace state without adding a production reset API.
#include "../src/game/weapons/weapon_lua.cpp"

namespace
{
void ExpectOfficialFailure(const char *pCase, const std::string &Source, const char *pExpectedError = 0)
{
	char aError[512];
	ResetOfficialRegistry();
	assert(!InitializeOfficialSource(Source.data(), static_cast<int>(Source.size()), aError, sizeof(aError)));
	assert(aError[0] != '\0');
	if(pExpectedError)
	{
		if(!strstr(aError, pExpectedError))
			fprintf(stderr, "%s: expected error containing '%s', got '%s'\n", pCase, pExpectedError, aError);
		assert(strstr(aError, pExpectedError) != 0);
	}
	assert(gs_EntryCount == 0 && !gs_Initialized);
}

std::string ReplaceOnce(std::string Source, const std::string &Needle, const std::string &Replacement)
{
	const size_t Position = Source.find(Needle);
	assert(Position != std::string::npos);
	Source.replace(Position, Needle.size(), Replacement);
	return Source;
}
} // namespace

int main()
{
	int SourceSize = 0;
	const char *pSource = reinterpret_cast<const char *>(gs_aOfficialWeaponsLua);
	SourceSize = gs_aOfficialWeaponsLuaSize;
	assert(pSource && SourceSize > 0);
	const std::string Official(pSource, SourceSize);
	const std::string FirstComponent =
		"weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, name = \"Ballistic frame\"}";

	// A missing component must fail completeness validation. Passing a null error
	// buffer also verifies that read-only initialization failure cannot crash.
	const std::string Missing = ReplaceOnce(Official, FirstComponent, "");
	ResetOfficialRegistry();
	assert(!InitializeOfficialSource(Missing.data(), static_cast<int>(Missing.size()), 0, 0));
	assert(gs_EntryCount == 0 && !gs_Initialized);

	ExpectOfficialFailure("duplicate component",
						  ReplaceOnce(Official, FirstComponent, FirstComponent + "\n" + FirstComponent),
						  "duplicated");
	ExpectOfficialFailure("invalid slot",
						  ReplaceOnce(Official,
									  FirstComponent,
									  "weapon.component {slot = 'invalid', id = weapon.part1.base1, name = 'Name'}"),
						  "frame or part");
	ExpectOfficialFailure("invalid id",
						  ReplaceOnce(Official,
									  FirstComponent,
									  "weapon.component {slot = weapon.component_slot.frame, id = 0, name = 'Name'}"),
						  "invalid or duplicated");
	ExpectOfficialFailure("fractional id",
						  ReplaceOnce(Official,
									  FirstComponent,
									  "weapon.component {slot = weapon.component_slot.frame, id = 1.5, name = 'Name'}"),
						  "integer");
	ExpectOfficialFailure(
		"numeric name",
		ReplaceOnce(Official,
					FirstComponent,
					"weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, name = 42}"),
		"1..63 bytes");
	ExpectOfficialFailure(
		"blank name",
		ReplaceOnce(Official,
					FirstComponent,
					"weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, name = '   '}"),
		"cannot be blank");
	ExpectOfficialFailure("unknown field",
						  ReplaceOnce(Official,
									  FirstComponent,
									  "weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, "
									  "name = 'Name', extra = true}"),
						  "unknown component");
	ExpectOfficialFailure(
		"invalid utf8",
		ReplaceOnce(
			Official,
			FirstComponent,
			"weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, name = string.char(255)}"),
		"1..63 bytes");
	ExpectOfficialFailure("embedded nul",
						  ReplaceOnce(Official,
									  FirstComponent,
									  "weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, "
									  "name = 'A' .. string.char(0) .. 'B'}"),
						  "1..63 bytes");
	ExpectOfficialFailure("long name",
						  ReplaceOnce(Official,
									  FirstComponent,
									  "weapon.component {slot = weapon.component_slot.frame, id = weapon.part1.base1, "
									  "name = '1234567890123456789012345678901234567890123456789012345678901234'}"),
						  "1..63 bytes");

	ExpectOfficialFailure("duplicate static identity",
						  ReplaceOnce(Official, "static_type = weapon.static.gun1", "static_type = weapon.static.tool"),
						  "profile count mismatch");
	ExpectOfficialFailure("duplicate modular identity",
						  ReplaceOnce(Official,
									  "part1 = weapon.part1.melee,\n  part2 = weapon.part2.melee1",
									  "part1 = weapon.part1.spin,\n  part2 = weapon.part2.melee1"),
						  "profile count mismatch");
	ExpectOfficialFailure("invalid modular identity",
						  ReplaceOnce(Official,
									  "part1 = weapon.part1.melee,\n  part2 = weapon.part2.melee1",
									  "part1 = weapon.part1.melee,\n  part2 = weapon.part2.barrel1"),
						  "profile count mismatch");

	// Every failed attempt above must leave the singleton clean and retryable.
	char aError[512];
	ResetOfficialRegistry();
	assert(InitializeOfficialSource(Official.data(), static_cast<int>(Official.size()), aError, sizeof(aError)));
	assert(strcmp(WeaponLuaPart1NameKey(PART1_BASE1), "Ballistic frame") == 0);
	assert(strcmp(WeaponLuaPart2NameKey(PART2_MELEE6), "Charged blade") == 0);
	assert(WeaponLuaDefinitionCount() == WEAPON_DEFINITION_COUNT);
	return 0;
}
