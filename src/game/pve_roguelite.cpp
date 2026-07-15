#include <base/system.h>

#include "pve_roguelite.h"

namespace
{
#define CARD0(Id, Name, Desc, Rarity, Stacks, Cost, Base, Legendary, Keywords, Tab, Branch, Tier, Mode, Spec) \
	{Id, Name, Desc, Rarity, Stacks, Cost, Base, Legendary, Keywords, {-1, -1, -1}, 0, Tab, Branch, Tier, Mode, Spec}
#define CARD1(Id, Name, Desc, Rarity, Stacks, Cost, Base, Legendary, Keywords, Prereq, Tab, Branch, Tier, Mode, Spec) \
	{Id, Name, Desc, Rarity, Stacks, Cost, Base, Legendary, Keywords, {Prereq, -1, -1}, 1, Tab, Branch, Tier, Mode, Spec}
#define CARD3(Id, Name, Desc, Rarity, Stacks, Cost, Base, Legendary, Keywords, Prereq0, Prereq1, Prereq2, Tab, Branch, Tier, Mode, Spec) \
	{Id, Name, Desc, Rarity, Stacks, Cost, Base, Legendary, Keywords, {Prereq0, Prereq1, Prereq2}, 3, Tab, Branch, Tier, Mode, Spec}

const CPveCardDef gs_aCards[NUM_PVE_CARDS] = {
	// Original seven base cards (IDs 0-6).
	CARD0(PVE_CARD_COMBAT_TRAINING, "Combat Training", "All damage +8% per stack (maximum 3).", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 0, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_REINFORCED_PLATES, "Reinforced Plates", "Gain 8 armor at the start of each stage per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 1, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_FIELD_SUPPLIES, "Field Supplies", "Gain 2 kits at the start of each stage per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 2, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_SCAVENGER, "Scavenger", "Gold income +12% per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 2, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_AMMO_RESERVE, "Ammo Reserve", "Restore 15% ammunition at the start of each stage per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 2, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_FIRST_AID, "First Aid", "Health and armor recovery +20% per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 1, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_QUICK_HANDS, "Quick Hands", "Weapon attack cooldown -8% per stack (maximum -30%).", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 0, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),

	// Original research (IDs 7-39).
	CARD0(PVE_CARD_FINISHER, "Finisher", "Deal +20% damage per stack to enemies below 30% health.", PVE_RARITY_COMMON, 3, 1, false, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 0, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_BOSS_HUNTER, "Boss Hunter", "Deal +25% damage to bosses.", PVE_RARITY_RARE, 1, 2, false, false, PVE_KEYWORD_NONE, PVE_CARD_FINISHER, PVE_TAB_CORE, 0, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_KILL_CHAIN, "Kill Chain", "Kills briefly grant +4% damage, stacking up to five times.", PVE_RARITY_EPIC, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_BOSS_HUNTER, PVE_TAB_CORE, 0, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_GLASS_EDGE, "Glass Edge", "Deal +35% damage, but take +20% damage.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_KILL_CHAIN, PVE_TAB_CORE, 0, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_DAMAGE_DAMPENER, "Damage Dampener", "Damage taken -8% per stack (total reduction capped at 50%).", PVE_RARITY_COMMON, 3, 1, false, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 1, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_EMERGENCY_PLATING, "Emergency Plating", "Dropping below 35% health grants 15 armor once per stage.", PVE_RARITY_RARE, 1, 2, false, false, PVE_KEYWORD_NONE, PVE_CARD_DAMAGE_DAMPENER, PVE_TAB_CORE, 1, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_GUARDIAN, "Guardian", "Allies near you take 12% less damage.", PVE_RARITY_EPIC, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_EMERGENCY_PLATING, PVE_TAB_CORE, 1, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_LAST_STAND, "Last Stand", "Once per stage, survive lethal damage with 1 health.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_GUARDIAN, PVE_TAB_CORE, 1, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_QUARTERMASTER, "Quartermaster", "Shop prices -10% per stack (maximum -40%).", PVE_RARITY_COMMON, 3, 1, false, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 2, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_PREMIUM_STOCK, "Premium Stock", "Shops offer one additional high-tier item.", PVE_RARITY_RARE, 1, 2, false, false, PVE_KEYWORD_NONE, PVE_CARD_QUARTERMASTER, PVE_TAB_CORE, 2, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_ENGINEER, "Engineer", "Building costs -20% and repairs +25%.", PVE_RARITY_EPIC, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_PREMIUM_STOCK, PVE_TAB_CORE, 2, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_RECYCLER, "Recycler", "Destroyed friendly buildings refund 40% of their kit cost.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_ENGINEER, PVE_TAB_CORE, 2, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_HOLLOW_POINT, "Hollow Point", "Firearm damage +12% per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_WEAPON, 0, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_SUSTAINED_FIRE, "Sustained Fire", "Continuous firearm hits build up to +25% damage.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_HOLLOW_POINT, PVE_TAB_WEAPON, 0, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_GUNSLINGER, "Gunslinger", "Firearms reload 20% faster and gain improved handling.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_SUSTAINED_FIRE, PVE_TAB_WEAPON, 0, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD0(PVE_CARD_DEMOLITION, "Demolition", "Explosive damage +12% per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_WEAPON, 1, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_WIDE_BLAST, "Wide Blast", "Explosion radius +25% (maximum +50%).", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_DEMOLITION, PVE_TAB_WEAPON, 1, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_CHAIN_REACTION, "Chain Reaction", "Explosive kills have a chance to trigger a secondary blast.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_WIDE_BLAST, PVE_TAB_WEAPON, 1, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD0(PVE_CARD_OVERCHARGE, "Overcharge", "Electric damage +12% per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_WEAPON, 2, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_ARC_CONDUCTOR, "Arc Conductor", "Electric hits arc to one nearby enemy for 35% damage.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_OVERCHARGE, PVE_TAB_WEAPON, 2, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_CAPACITOR, "Capacitor", "Every fifth electric hit deals double damage.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_ARC_CONDUCTOR, PVE_TAB_WEAPON, 2, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD0(PVE_CARD_BERSERKER, "Berserker", "Melee damage +12% per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_WEAPON, 3, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_BLOOD_DRIVE, "Blood Drive", "Melee kills restore 4 health.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_BERSERKER, PVE_TAB_WEAPON, 3, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_SHOCKWAVE, "Shockwave", "Heavy melee hits release a damaging shockwave.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_BLOOD_DRIVE, PVE_TAB_WEAPON, 3, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD0(PVE_CARD_DELVER, "Delver", "Invasion stage-start supplies +25% per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_MODE, 0, 1, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_OBJECTIVE_SPECIALIST, "Objective Specialist", "Deal +20% damage to Invasion objective targets.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_DELVER, PVE_TAB_MODE, 0, 2, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_ADAPTATION, "Adaptation", "After each Invasion floor, gain 4% damage for the run.", PVE_RARITY_EPIC, 1, 5, false, false, PVE_KEYWORD_NONE, PVE_CARD_OBJECTIVE_SPECIALIST, PVE_TAB_MODE, 0, 3, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_BREATHING_ROOM, "Breathing Room", "Horde intermissions restore 20% health and ammunition per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_MODE, 1, 1, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_REAPER, "Reaper", "Horde multikills grant +15% damage for 5 seconds.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_BREATHING_ROOM, PVE_TAB_MODE, 1, 2, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_SIEGE_MASTER, "Siege Master", "Buildings deal +30% damage during Horde.", PVE_RARITY_EPIC, 1, 5, false, false, PVE_KEYWORD_NONE, PVE_CARD_REAPER, PVE_TAB_MODE, 1, 3, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_SABOTEUR, "Saboteur", "Extraction switch and objective interactions are 25% faster per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_TAB_MODE, 2, 1, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_ESCAPE_ARTIST, "Escape Artist", "Gain +15% movement speed while evacuating.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_SABOTEUR, PVE_TAB_MODE, 2, 2, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_NO_ONE_LEFT, "No One Left", "Reviving an ally grants both players 20 armor.", PVE_RARITY_EPIC, 1, 5, false, false, PVE_KEYWORD_NONE, PVE_CARD_ESCAPE_ARTIST, PVE_TAB_MODE, 2, 3, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),

	// Additional base cards (IDs 40-51).
	CARD0(PVE_CARD_MARKING_ROUNDS, "Marking Rounds", "Six direct hits on one enemy apply Vulnerable for 4 seconds; each stack adds 3% damage taken, capped at 25%.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_VULNERABLE, PVE_TAB_CORE, 0, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_BARRIER_CELL, "Barrier Cell", "Gain 6 Barrier per stack at stage start. Barrier absorbs damage before armor and is capped at 30.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_BARRIER, PVE_TAB_CORE, 1, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_SECOND_WIND, "Second Wind", "Every 5 kills restores 2 health per stack, up to three triggers per stage.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 1, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_SALVAGE_INSTINCT, "Salvage Instinct", "Every 8 kills grants 1 kit per stack, capped at 6 kits per stage.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 2, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_RESERVE_FUND, "Reserve Fund", "Stage completion pays 5% interest on current gold per stack, capped at 30 gold per stage.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_CORE, 2, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_FIELD_RELAY, "Field Relay", "Allies within 350 range receive 6% more health, armor and Barrier recovery per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_BARRIER, PVE_TAB_CORE, 1, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_DRONE_CHASSIS, "Drone Chassis", "Deploy a support drone; each stack increases current module efficiency by 10%, capped at 30%.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_DRONE, PVE_TAB_CORE, 3, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_FOCUS_DRILL, "Focus Drill", "Firearm hits build Focus. At 10, the next shot consumes it for +15% damage per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_FOCUS, PVE_TAB_WEAPON, 0, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD0(PVE_CARD_BLAST_BATTERY, "Blast Battery", "Explosions gain one charge per enemy hit. At 5, the next blast consumes five for +8% damage per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_BLAST_CHARGE, PVE_TAB_WEAPON, 1, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD0(PVE_CARD_VOLTAGE_BANK, "Voltage Bank", "Electric hits gain Voltage. At 10, the next hit consumes it and arcs to one extra target per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_VOLTAGE, PVE_TAB_WEAPON, 2, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD0(PVE_CARD_FURY_METER, "Fury Meter", "Melee hits gain Fury. At 10, the next melee hit consumes it for +20% damage per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_FURY, PVE_TAB_WEAPON, 3, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD0(PVE_CARD_OBJECTIVE_CACHE, "Objective Cache", "The first objective completed each stage restores 10% ammo and grants 3 armor per stack.", PVE_RARITY_COMMON, 3, 0, true, false, PVE_KEYWORD_NONE, PVE_TAB_MODE, 0, 0, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),

	// Core and drone research (IDs 52-63).
	CARD1(PVE_CARD_PREDATOR, "Predator", "Deal +15% damage to Vulnerable targets.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_VULNERABLE, PVE_CARD_GLASS_EDGE, PVE_TAB_CORE, 0, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_APEX_EXECUTION, "Apex Execution", "Every 10th direct hit deals +75% damage and refreshes Vulnerable.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_VULNERABLE, PVE_CARD_PREDATOR, PVE_TAB_CORE, 0, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_BARRIER_REFIT, "Barrier Refit", "After 5 seconds without taking damage, restore 2 Barrier each second.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_BARRIER, PVE_CARD_LAST_STAND, PVE_TAB_CORE, 1, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_AEGIS_LOOP, "Aegis Loop", "When Barrier breaks, gain 15 armor once per stage.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_BARRIER, PVE_CARD_BARRIER_REFIT, PVE_TAB_CORE, 1, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_LIQUID_ASSETS, "Liquid Assets", "Gain +2% damage per 50 gold held, capped at +20%.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_RECYCLER, PVE_TAB_CORE, 2, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_WAR_ECONOMY, "War Economy", "Every 25 gold spent grants 5 Barrier, up to 15 Barrier per purchase.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_BARRIER, PVE_CARD_LIQUID_ASSETS, PVE_TAB_CORE, 2, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD0(PVE_CARD_SERVO_LINK, "Servo Link", "Drone attack and repair intervals are 8% shorter per stack, subject to the 30% cooldown cap.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_DRONE, PVE_TAB_CORE, 3, 1, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_ASSAULT_MODULE, "Assault Module", "Drone attacks the nearest enemy every 1.2 seconds for 20% of current weapon base damage.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_DRONE, PVE_CARD_SERVO_LINK, PVE_TAB_CORE, 3, 2, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_GUARDIAN_MODULE, "Guardian Module", "Drone gives allies within 280 range 10% damage reduction.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_DRONE, PVE_CARD_SERVO_LINK, PVE_TAB_CORE, 3, 3, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_REPAIR_MODULE, "Repair Module", "Each second, restore 2 armor or durability to the lowest-armored ally or friendly building.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_DRONE, PVE_CARD_SERVO_LINK, PVE_TAB_CORE, 3, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD3(PVE_CARD_COORDINATED_FIRMWARE, "Coordinated Firmware", "Increase current drone module efficiency by 25%.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_DRONE, PVE_CARD_ASSAULT_MODULE, PVE_CARD_GUARDIAN_MODULE, PVE_CARD_REPAIR_MODULE, PVE_TAB_CORE, 3, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_AUTONOMOUS_CORE, "Autonomous Core", "Increase current module efficiency by another 50%; a disabled drone recovers after 5 seconds.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_DRONE, PVE_CARD_COORDINATED_FIRMWARE, PVE_TAB_CORE, 3, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_NONE),

	// Firearm research (IDs 64-69).
	CARD1(PVE_CARD_CALIBRATION, "Calibration", "Reduce the Focus trigger threshold by 1 per stack, to a minimum of 7.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_FOCUS, PVE_CARD_GUNSLINGER, PVE_TAB_WEAPON, 0, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_PINPOINT_BURST, "Pinpoint Burst", "Consuming Focus applies Vulnerable for 5 seconds.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_FOCUS | PVE_KEYWORD_VULNERABLE, PVE_CARD_CALIBRATION, PVE_TAB_WEAPON, 0, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_PERFECT_SEQUENCE, "Perfect Sequence", "After consuming Focus, the first 6 shots of the next magazine deal +35% damage.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_FOCUS, PVE_CARD_PINPOINT_BURST, PVE_TAB_WEAPON, 0, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_ARMOR_PIERCER, "Armor Piercer", "Deal +8% firearm damage to Vulnerable targets per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_VULNERABLE, PVE_CARD_HOLLOW_POINT, PVE_TAB_WEAPON, 0, 7, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_TACTICAL_RELOAD, "Tactical Reload", "After a full reload, the first 5 shots deal +15% damage.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_FOCUS, PVE_CARD_ARMOR_PIERCER, PVE_TAB_WEAPON, 0, 8, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),
	CARD1(PVE_CARD_CROSSFIRE, "Crossfire", "Against the assault drone's target, player damage is +15% and drone damage is +50%.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_DRONE, PVE_CARD_TACTICAL_RELOAD, PVE_TAB_WEAPON, 0, 9, PVE_MODE_ANY, PVE_SPECIALIZATION_FIREARM),

	// Explosive research (IDs 70-75).
	CARD1(PVE_CARD_PACKED_CHARGE, "Packed Charge", "Reduce the empowered blast threshold by 1 per stack, to a minimum of 3.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_BLAST_CHARGE, PVE_CARD_CHAIN_REACTION, PVE_TAB_WEAPON, 1, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_BREACH_CHARGE, "Breach Charge", "Empowered explosions apply Vulnerable for 5 seconds.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_BLAST_CHARGE | PVE_KEYWORD_VULNERABLE, PVE_CARD_PACKED_CHARGE, PVE_TAB_WEAPON, 1, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_CATACLYSM, "Cataclysm", "Every 3rd empowered explosion creates a delayed secondary explosion for 60% damage.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_BLAST_CHARGE, PVE_CARD_BREACH_CHARGE, PVE_TAB_WEAPON, 1, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_SHRAPNEL, "Shrapnel", "Empowered explosions apply 1 Bleed stack per card stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_BLEED, PVE_CARD_DEMOLITION, PVE_TAB_WEAPON, 1, 7, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_CONTROLLED_FUSE, "Controlled Fuse", "Take 50% less self-explosion damage; empowered blast radius is 20% larger.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_BLAST_CHARGE, PVE_CARD_SHRAPNEL, PVE_TAB_WEAPON, 1, 8, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),
	CARD1(PVE_CARD_SIEGE_PAYLOAD, "Siege Payload", "Deal +30% explosive damage to bosses, objectives and buildings.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_BLAST_CHARGE, PVE_CARD_CONTROLLED_FUSE, PVE_TAB_WEAPON, 1, 9, PVE_MODE_ANY, PVE_SPECIALIZATION_EXPLOSIVE),

	// Electric research (IDs 76-81).
	CARD1(PVE_CARD_CHARGE_COIL, "Charge Coil", "Gain 1 extra Voltage per stack when charging, up to 3 Voltage per hit.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_VOLTAGE, PVE_CARD_CAPACITOR, PVE_TAB_WEAPON, 2, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_CONDUCTIVE, "Conductive", "Consuming Voltage makes the target Conductive for 5 seconds; later arc damage is +25%.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_VOLTAGE, PVE_CARD_CHARGE_COIL, PVE_TAB_WEAPON, 2, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_THUNDERHEAD, "Thunderhead", "Every 3rd full Voltage release calls a lightning strike for 100% damage.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_VOLTAGE, PVE_CARD_CONDUCTIVE, PVE_TAB_WEAPON, 2, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_FEEDBACK, "Feedback", "Electric kills refund 2 Voltage per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_VOLTAGE, PVE_CARD_OVERCHARGE, PVE_TAB_WEAPON, 2, 7, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_STATIC_SHIELD, "Static Shield", "Consuming Voltage grants 10 Barrier.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_VOLTAGE | PVE_KEYWORD_BARRIER, PVE_CARD_FEEDBACK, PVE_TAB_WEAPON, 2, 8, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),
	CARD1(PVE_CARD_GRID_LINK, "Grid Link", "While Voltage exceeds 5, current drone module efficiency is +35%.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_VOLTAGE | PVE_KEYWORD_DRONE, PVE_CARD_STATIC_SHIELD, PVE_TAB_WEAPON, 2, 9, PVE_MODE_ANY, PVE_SPECIALIZATION_ELECTRIC),

	// Melee research (IDs 82-87).
	CARD1(PVE_CARD_FURY_ENGINE, "Fury Engine", "Gain 1 extra Fury per stack on hit, up to 4 Fury per hit.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_FURY, PVE_CARD_SHOCKWAVE, PVE_TAB_WEAPON, 3, 4, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_OPEN_WOUND, "Open Wound", "Consuming Fury applies 3 Bleed stacks.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_FURY | PVE_KEYWORD_BLEED, PVE_CARD_FURY_ENGINE, PVE_TAB_WEAPON, 3, 5, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_AVATAR_OF_WAR, "Avatar of War", "After consuming full Fury, gain +40% melee damage and -20% damage taken for 6 seconds.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_FURY, PVE_CARD_OPEN_WOUND, PVE_TAB_WEAPON, 3, 6, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_GUARD_BREAKER, "Guard Breaker", "Heavy hits add 2 seconds of Vulnerable per stack, up to 6 seconds.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_VULNERABLE, PVE_CARD_BERSERKER, PVE_TAB_WEAPON, 3, 7, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_BLOOD_TEMPER, "Blood Temper", "When a bleeding enemy dies, restore 5 health, capped at 15 per event.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_BLEED, PVE_CARD_GUARD_BREAKER, PVE_TAB_WEAPON, 3, 8, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),
	CARD1(PVE_CARD_KINETIC_RETURN, "Kinetic Return", "Each enemy hit by a shockwave grants 1 Fury and 2 Barrier.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_FURY | PVE_KEYWORD_BARRIER, PVE_CARD_BLOOD_TEMPER, PVE_TAB_WEAPON, 3, 9, PVE_MODE_ANY, PVE_SPECIALIZATION_MELEE),

	// Invasion, Horde and Extraction research (IDs 88-99).
	CARD1(PVE_CARD_CARTOGRAPHER, "Cartographer", "Objectives always show direction and distance; interaction speed is +15%.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_ADAPTATION, PVE_TAB_MODE, 0, 4, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_DEEP_SOVEREIGN, "Deep Sovereign", "The final objective on each floor takes +50% damage; completion grants the team 20 Barrier.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_BARRIER, PVE_CARD_CARTOGRAPHER, PVE_TAB_MODE, 0, 5, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_RELIC_SCANNER, "Relic Scanner", "Each completed objective grants 4 gold per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_CARD_DELVER, PVE_TAB_MODE, 0, 6, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_FLOOR_MEMORY, "Floor Memory", "A deathless floor grants +4% damage on later floors, capped at +20%.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_NONE, PVE_CARD_RELIC_SCANNER, PVE_TAB_MODE, 0, 7, PVE_MODE_INVASION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_WAVE_DIVIDEND, "Wave Dividend", "A wave completed without player deaths grants 8 gold to each player.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_SIEGE_MASTER, PVE_TAB_MODE, 1, 4, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_ENDLESS_ENGINE, "Endless Engine", "Consecutive deathless waves grant +5% player and building damage, capped at +25%; a death resets it.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_NONE, PVE_CARD_WAVE_DIVIDEND, PVE_TAB_MODE, 1, 5, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_FORTIFIED_CYCLE, "Fortified Cycle", "At wave start, repair every friendly building by 5% maximum durability per stack.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_CARD_BREATHING_ROOM, PVE_TAB_MODE, 1, 6, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_HOLD_THE_LINE, "Hold the Line", "Inside the defense area, take 15% less damage and gain +25% drone efficiency.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_DRONE, PVE_CARD_FORTIFIED_CYCLE, PVE_TAB_MODE, 1, 7, PVE_MODE_HORDE, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_COURIER, "Courier", "While carrying an objective item, move 10% faster and take 10% less damage.", PVE_RARITY_RARE, 1, 3, false, false, PVE_KEYWORD_NONE, PVE_CARD_NO_ONE_LEFT, PVE_TAB_MODE, 2, 4, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_FINAL_DEPARTURE, "Final Departure", "After the exit opens, the team gains +30% damage and +15% speed, but health recovery stops.", PVE_RARITY_LEGENDARY, 1, 5, false, true, PVE_KEYWORD_NONE, PVE_CARD_COURIER, PVE_TAB_MODE, 2, 5, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_SIGNAL_HACKER, "Signal Hacker", "Switch and objective interactions are 10% faster per stack; total interaction bonus is capped at 60%.", PVE_RARITY_COMMON, 3, 2, false, false, PVE_KEYWORD_NONE, PVE_CARD_SABOTEUR, PVE_TAB_MODE, 2, 6, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),
	CARD1(PVE_CARD_CLEAN_EXIT, "Clean Exit", "Entering the evacuation zone restores 30% ammo and grants 20 Barrier once per evacuation.", PVE_RARITY_EPIC, 1, 4, false, false, PVE_KEYWORD_BARRIER, PVE_CARD_SIGNAL_HACKER, PVE_TAB_MODE, 2, 7, PVE_MODE_EXTRACTION, PVE_SPECIALIZATION_NONE),
};

#undef CARD0
#undef CARD1
#undef CARD3

const CPveContractDef gs_aContracts[NUM_PVE_CONTRACTS] = {
	{PVE_CONTRACT_GLASS_CANNON, "Glass Cannon", "Team damage +25%; damage taken +35%.", "Extreme incoming damage", PVE_MODE_ANY},
	{PVE_CONTRACT_ELITE_HUNT, "Elite Hunt", "An additional boss spawns two levels above the stage.", "Extra high-level boss", PVE_MODE_ANY},
	{PVE_CONTRACT_RESOURCE_DROUGHT, "Resource Drought", "Gold income is halved and shop prices are increased by 50%.", "Severely limited economy", PVE_MODE_ANY},
	{PVE_CONTRACT_SPEED_CLEAR, "Speed Clear", "Complete the Invasion floor within 150 seconds.", "Strict time limit", PVE_MODE_INVASION},
	{PVE_CONTRACT_FLAWLESS, "Flawless", "Complete the floor without any player dying.", "One death fails the contract", PVE_MODE_INVASION},
	{PVE_CONTRACT_PURGE_PROTOCOL, "Purge Protocol", "Enemy count is increased by 35% for this floor.", "Much larger enemy force", PVE_MODE_INVASION},
	{PVE_CONTRACT_DOUBLE_WAVE, "Double Wave", "Enemy count is increased by 50% for the next four waves.", "Four reinforced waves", PVE_MODE_HORDE},
	{PVE_CONTRACT_BOSS_RUSH, "Boss Rush", "An additional boss appears in waves two and four of the section.", "Two additional bosses", PVE_MODE_HORDE},
	{PVE_CONTRACT_NO_RESPAWN, "No Respawn", "Players cannot respawn during the next four waves.", "Deaths last for the section", PVE_MODE_HORDE},
	{PVE_CONTRACT_TIGHT_DEADLINE, "Tight Deadline", "The Extraction time limit is reduced by 30%.", "Less evacuation time", PVE_MODE_EXTRACTION},
	{PVE_CONTRACT_BLACK_BOX, "Black Box", "Recover the marked objective by remaining at it for 3 seconds.", "Additional recovery objective", PVE_MODE_EXTRACTION},
	{PVE_CONTRACT_OVERRUN, "Overrun", "Evacuation reinforcement size and active cap are doubled.", "Overwhelming evacuation pressure", PVE_MODE_EXTRACTION},
	{PVE_CONTRACT_ATTRITION, "Attrition", "Health, armor and Barrier recovery are reduced by 50%.", "Halved recovery", PVE_MODE_ANY},
	{PVE_CONTRACT_OVERCLOCKED_HOSTILES, "Overclocked Hostiles", "Enemies move 15% faster and deal 25% more damage.", "Faster, harder-hitting enemies", PVE_MODE_ANY},
	{PVE_CONTRACT_SEALED_SUPPLIES, "Sealed Supplies", "Shops and stage supplies are disabled for the current floor.", "No resupply", PVE_MODE_INVASION},
	{PVE_CONTRACT_ELITE_GUARD, "Elite Guard", "Each objective gains one elite guard a level above it; all guards must be killed.", "Additional objective guards", PVE_MODE_INVASION},
	{PVE_CONTRACT_FORTIFICATION_TAX, "Fortification Tax", "Building and repair costs are increased by 75% for the next four waves.", "Expensive fortifications", PVE_MODE_HORDE},
	{PVE_CONTRACT_RISING_TIDE, "Rising Tide", "Enemy count rises by 15%, 30%, 45% and 60%; final-wave enemies gain 25% health.", "Escalating four-wave assault", PVE_MODE_HORDE},
	{PVE_CONTRACT_HEAVY_CARGO, "Heavy Cargo", "Carry spawned cargo into extraction; the carrier moves 20% slower and may drop it.", "Slower cargo carrier", PVE_MODE_EXTRACTION},
	{PVE_CONTRACT_LOCKED_ROUTE, "Locked Route", "Activate two additional radar-marked switches before the exit opens.", "Two extra route switches", PVE_MODE_EXTRACTION},
};

const CPveOperationDef gs_aOperations[NUM_PVE_OPERATIONS] = {
	{PVE_OPERATION_CIRCUIT_BREAKER, "Circuit Breaker", "Disable hostile control loops. Enemy pressure is lighter and objective damage is stronger.", PVE_MODE_INVASION, 0.90f, 0.95f, 1.00f, 1.00f, 1.00f, 1.00f, 1.05f},
	{PVE_OPERATION_FOUNDRY_SHUTDOWN, "Foundry Shutdown", "Cut production lines. Fewer enemies arrive and team repairs are more efficient.", PVE_MODE_INVASION, 0.85f, 1.00f, 0.95f, 1.05f, 1.00f, 1.20f, 1.00f},
	{PVE_OPERATION_FIRE_CONTROL_PURGE, "Fire-Control Purge", "Scrub targeting systems. Enemies are faster, but reinforcements are reduced.", PVE_MODE_INVASION, 0.90f, 1.00f, 1.10f, 0.95f, 0.90f, 1.00f, 1.00f},
	{PVE_OPERATION_SIEGE_LINE, "Siege Line", "Anchor the front. Defensive repairs improve and hostile numbers stay lower.", PVE_MODE_HORDE, 0.95f, 1.00f, 0.95f, 1.00f, 1.00f, 1.35f, 0.95f},
	{PVE_OPERATION_ASSEMBLY_SURGE, "Assembly Surge", "Reroute salvage to the crew. Repairs are better and enemy armor is a little thinner.", PVE_MODE_HORDE, 1.00f, 0.92f, 1.00f, 1.00f, 1.10f, 1.20f, 1.00f},
	{PVE_OPERATION_GRID_STORM, "Grid Storm", "Overload the field. Enemies move faster, but the horde is slightly smaller.", PVE_MODE_HORDE, 0.92f, 0.96f, 1.08f, 1.00f, 0.95f, 1.00f, 1.05f},
	{PVE_OPERATION_CORE_RECOVERY, "Core Recovery", "Restore the route core. You get more time and lighter reinforcement pressure.", PVE_MODE_EXTRACTION, 1.00f, 0.95f, 1.00f, 1.15f, 0.85f, 1.00f, 1.00f},
	{PVE_OPERATION_LOCKDOWN_BREAK, "Lockdown Break", "Crack the route seals. Switches are easier and the exit timer is kinder.", PVE_MODE_EXTRACTION, 0.90f, 1.00f, 1.00f, 1.10f, 1.00f, 1.10f, 1.10f},
	{PVE_OPERATION_SIEGE_ROUTE, "Siege Route", "Take the hard corridor. More enemies come through, but rewards scale up.", PVE_MODE_EXTRACTION, 1.05f, 1.05f, 1.00f, 1.10f, 1.05f, 1.00f, 1.15f},
};

bool ValidatePrerequisiteDfs(int ID, int *pState)
{
	if(pState[ID] == 1)
		return false;
	if(pState[ID] == 2)
		return true;
	pState[ID] = 1;
	const CPveCardDef &Def = gs_aCards[ID];
	for(int i = 0; i < Def.m_NumPrerequisites; i++)
	{
		const int Prerequisite = Def.m_aPrerequisites[i];
		if(Prerequisite < 0 || Prerequisite >= NUM_PVE_CARDS || !ValidatePrerequisiteDfs(Prerequisite, pState))
			return false;
	}
	pState[ID] = 2;
	return true;
}
}

CPveResearchMask::CPveResearchMask(unsigned long long Low, unsigned long long High)
{
	m_aWords[0] = Low;
	m_aWords[1] = High;
}

bool CPveResearchMask::Test(int CardID) const
{
	return CardID >= 0 && CardID < 128 && (m_aWords[CardID / 64] & (1ULL << (CardID % 64))) != 0;
}

void CPveResearchMask::Set(int CardID, bool Value)
{
	if(CardID < 0 || CardID >= 128)
		return;
	const unsigned long long Bit = 1ULL << (CardID % 64);
	if(Value)
		m_aWords[CardID / 64] |= Bit;
	else
		m_aWords[CardID / 64] &= ~Bit;
}

bool CPveResearchMask::PrerequisitesMet(int CardID) const
{
	const CPveCardDef *pDef = PveCardDef(CardID);
	if(!pDef)
		return false;
	for(int i = 0; i < pDef->m_NumPrerequisites; i++)
		if(!PveCardIsUnlocked(pDef->m_aPrerequisites[i], *this))
			return false;
	return true;
}

void CPveResearchMask::Sanitize()
{
	for(int ID = 0; ID < 128; ID++)
		if(ID >= NUM_PVE_CARDS || (ID < NUM_PVE_CARDS && PveCardIsBase(ID)))
			Set(ID, false);
	for(int Pass = 0; Pass < NUM_PVE_CARDS; Pass++)
	{
		bool Changed = false;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			if(Test(ID) && !PrerequisitesMet(ID))
			{
				Set(ID, false);
				Changed = true;
			}
		if(!Changed)
			break;
	}
}

bool CPveResearchMask::operator==(const CPveResearchMask &Other) const
{
	return m_aWords[0] == Other.m_aWords[0] && m_aWords[1] == Other.m_aWords[1];
}

const CPveCardDef *PveCardDef(int ID)
{
	return ID >= 0 && ID < NUM_PVE_CARDS ? &gs_aCards[ID] : 0;
}

const CPveContractDef *PveContractDef(int ID)
{
	return ID >= 0 && ID < NUM_PVE_CONTRACTS ? &gs_aContracts[ID] : 0;
}

const CPveOperationDef *PveOperationDef(int ID)
{
	return ID >= 0 && ID < NUM_PVE_OPERATIONS ? &gs_aOperations[ID] : 0;
}

const char *PveChoiceName(int ID)
{
	const CPveCardDef *pDef = PveCardDef(ID);
	if(pDef)
		return pDef->m_pName;
	if(ID == PVE_SUPPLY_ARMOR)
		return "Emergency Armor";
	if(ID == PVE_SUPPLY_AMMO)
		return "Full Ammunition";
	if(ID == PVE_SUPPLY_KITS)
		return "Emergency Kits";
	return "Unknown perk";
}

const char *PveChoiceDescription(int ID)
{
	const CPveCardDef *pDef = PveCardDef(ID);
	if(pDef)
		return pDef->m_pDescription;
	if(ID == PVE_SUPPLY_ARMOR)
		return "Gain 25 armor immediately.";
	if(ID == PVE_SUPPLY_AMMO)
		return "Refill all weapon ammunition immediately.";
	if(ID == PVE_SUPPLY_KITS)
		return "Gain 5 kits immediately.";
	return "";
}

const char *PveRarityName(int Rarity)
{
	if(Rarity == PVE_RARITY_RARE)
		return "Rare";
	if(Rarity == PVE_RARITY_EPIC)
		return "Epic";
	if(Rarity == PVE_RARITY_LEGENDARY)
		return "Legendary";
	return "Common";
}

const char *PveRewardReasonName(int Reason)
{
	if(Reason == PVE_REWARD_HORDE_SECTION)
		return "Horde section cleared";
	if(Reason == PVE_REWARD_EXTRACTION)
		return "Extraction completed";
	if(Reason == PVE_REWARD_CONTRACT)
		return "Contract completed";
	return "Invasion depth cleared";
}

const char *PveOperationName(int ID)
{
	const CPveOperationDef *pDef = PveOperationDef(ID);
	return pDef ? pDef->m_pName : "Unknown operation";
}

const char *PveOperationDescription(int ID)
{
	const CPveOperationDef *pDef = PveOperationDef(ID);
	return pDef ? pDef->m_pDescription : "";
}

bool PveCardIsBase(int ID)
{
	const CPveCardDef *pDef = PveCardDef(ID);
	return pDef && pDef->m_Base;
}

bool PveCardIsUnlocked(int ID, const CPveResearchMask &ResearchMask)
{
	return PveCardIsBase(ID) || (ID >= 0 && ID < NUM_PVE_CARDS && ResearchMask.Test(ID));
}

CPveResearchMask PveSanitizeResearchMask(CPveResearchMask ResearchMask)
{
	ResearchMask.Sanitize();
	return ResearchMask;
}

bool PveResearchMaskIsValid(const CPveResearchMask &ResearchMask)
{
	return PveSanitizeResearchMask(ResearchMask) == ResearchMask;
}

bool PveContractAvailableInMode(int ContractID, int Mode)
{
	const CPveContractDef *pDef = PveContractDef(ContractID);
	return pDef && (pDef->m_Mode == PVE_MODE_ANY || pDef->m_Mode == Mode);
}

bool PveOperationAvailableInMode(int OperationID, int Mode)
{
	const CPveOperationDef *pDef = PveOperationDef(OperationID);
	return pDef && (pDef->m_Mode == PVE_MODE_ANY || pDef->m_Mode == Mode);
}

bool PveValidateDefinitions(char *pError, int ErrorSize)
{
	bool aCardIDs[NUM_PVE_CARDS] = {false};
	int BaseCards = 0;
	int NewBaseCards = 0;
	int LegendaryCards = 0;
	int NewResearchCost = 0;
	for(int i = 0; i < NUM_PVE_CARDS; i++)
	{
		const CPveCardDef &Def = gs_aCards[i];
		if(Def.m_ID < 0 || Def.m_ID >= NUM_PVE_CARDS || aCardIDs[Def.m_ID] || Def.m_ID != i)
		{
			str_format(pError, ErrorSize, "invalid or duplicate card id %d", Def.m_ID);
			return false;
		}
		aCardIDs[Def.m_ID] = true;
		if(Def.m_MaxStacks < 1 || Def.m_MaxStacks > 3 || (Def.m_Rarity != PVE_RARITY_COMMON && Def.m_MaxStacks != 1) || Def.m_Legendary != (Def.m_Rarity == PVE_RARITY_LEGENDARY))
		{
			str_format(pError, ErrorSize, "invalid rarity or stack rule for card %d", i);
			return false;
		}
		if(Def.m_Base)
		{
			BaseCards++;
			NewBaseCards += i >= 40 && i <= 51;
			if(Def.m_ResearchCost != 0 || Def.m_NumPrerequisites != 0)
			{
				str_format(pError, ErrorSize, "base card %d has research metadata", i);
				return false;
			}
		}
		else if(i >= PVE_CARD_PREDATOR)
			NewResearchCost += Def.m_ResearchCost;
		LegendaryCards += Def.m_Legendary;
		if(Def.m_NumPrerequisites < 0 || Def.m_NumPrerequisites > 3)
		{
			str_format(pError, ErrorSize, "invalid prerequisite count at card %d", i);
			return false;
		}
	}
	int aState[NUM_PVE_CARDS] = {0};
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!ValidatePrerequisiteDfs(ID, aState))
		{
			str_format(pError, ErrorSize, "cyclic or invalid prerequisite at card %d", ID);
			return false;
		}
	if(BaseCards != 19 || NewBaseCards != 12 || LegendaryCards != 8 || NewResearchCost < 150 || NewResearchCost > 165)
	{
		str_format(pError, ErrorSize, "invalid content totals: base=%d newbase=%d legendary=%d cost=%d", BaseCards, NewBaseCards, LegendaryCards, NewResearchCost);
		return false;
	}

	bool aContractIDs[NUM_PVE_CONTRACTS] = {false};
	for(int i = 0; i < NUM_PVE_CONTRACTS; i++)
	{
		const int ID = gs_aContracts[i].m_ID;
		if(ID < 0 || ID >= NUM_PVE_CONTRACTS || aContractIDs[ID] || ID != i)
		{
			str_format(pError, ErrorSize, "invalid or duplicate contract id %d", ID);
			return false;
		}
		aContractIDs[ID] = true;
	}

	if(pError && ErrorSize > 0)
		pError[0] = 0;
	return true;
}

static_assert(PVE_CARD_NO_ONE_LEFT == 39, "legacy card IDs must remain stable");
static_assert(PVE_CARD_MARKING_ROUNDS == 40, "new base card IDs must start at 40");
static_assert(PVE_CARD_PREDATOR == 52, "new research card IDs must start at 52");
static_assert(NUM_PVE_CARDS == 100, "PvE Roguelite must contain exactly 100 cards");
static_assert(NUM_PVE_CONTRACTS == 20, "PvE Roguelite must contain exactly 20 contracts");
