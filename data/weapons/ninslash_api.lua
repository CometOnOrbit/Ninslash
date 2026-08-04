---@meta NinslashWeaponAPI

-- Lua Language Server declarations for Ninslash weapon definitions, deterministic
-- combat scripts, and client-only presentation scripts. This file is editor-only;
-- the game does not load it and it is intentionally absent from the official manifest.

---@alias WeaponKind 'static'|'modular'
---@alias WeaponComponentSlot 'frame'|'part'
---@alias WeaponAttackKind 'droid'|'droid_death'|'building'
---@alias WeaponBehavior
---| 'tool'
---| 'claw'
---| 'chainsaw'
---| 'flamer'
---| 'grenade_timed'
---| 'grenade_laser'
---| 'grenade_drop'
---| 'upgrade'
---| 'controller_activate'
---| 'electrowall'
---| 'area_shield'
---| 'shuriken'
---| 'bomb'
---| 'ball'
---| 'cluster'
---| 'bazooka'
---| 'electric_gun'
---| 'compact_gun_hands'
---| 'charged_burst'
---| 'spin_reflect'
---| 'impact_spark'
---| 'explosion_smoke'
---| 'green_explosion'
---| 'activate_invis'
---| 'activate_shield'
---| 'activate_respawner'
---| 'melee'
---| 'charged_blade'
---| 'capacitor'
---| 'rail'
---| 'hammer_impact'

---@class WeaponNumberCurve: table<integer, number>
---@class WeaponIntegerCurve: table<integer, integer>
---@class WeaponBooleanCurve: table<integer, boolean>
---@alias WeaponNumber number|WeaponNumberCurve
---@alias WeaponInteger integer|WeaponIntegerCurve
---@alias WeaponBoolean boolean|WeaponBooleanCurve
---@alias WeaponVector { [1]: WeaponNumber, [2]: WeaponNumber }

---@class WeaponCombat
---@field firing_type? WeaponInteger Use a value from `weapon.combat`.
---@field fire_rate? WeaponNumber
---@field full_auto? WeaponBoolean
---@field max_ammo? WeaponInteger
---@field uses_ammo? WeaponBoolean
---@field shot_spread? WeaponInteger
---@field projectile_spread? WeaponNumber
---@field projectile_speed? WeaponNumber
---@field projectile_curvature? WeaponNumber
---@field projectile_life? WeaponNumber Positive infinity is accepted only for this field.
---@field projectile_damage? WeaponNumber
---@field projectile_knockback? WeaponNumber
---@field explosion_size? WeaponNumber
---@field explosion_damage? WeaponNumber
---@field melee_hit_radius? WeaponNumber
---@field weapon_knockback? WeaponNumber
---@field burst_count? WeaponInteger
---@field burst_reload? WeaponNumber
---@field ai_attack_range? WeaponInteger
---@field valid_for_turret? WeaponBoolean
---@field throw_force? WeaponNumber
---@field flame_amount? WeaponNumber
---@field electro_amount? WeaponNumber
---@field explosive_projectile? WeaponBoolean
---@field laser_weapon? WeaponBoolean
---@field cursor_weapon? WeaponInteger
---@field cost? WeaponInteger
---@field aimline? WeaponBoolean
---@field projectile_pos_type? WeaponInteger Use a value from `weapon.path`.
---@field laser_range? WeaponInteger
---@field laser_charge? WeaponInteger
---@field projectile_bounces? WeaponInteger
---@field auto_pick? WeaponBoolean
---@field charge_damage_min? WeaponNumber
---@field charge_damage_max? WeaponNumber
---@field charge_range_min? WeaponNumber
---@field charge_range_max? WeaponNumber
---@field charge_power_min? WeaponNumber
---@field charge_power_max? WeaponNumber
---@field projectile_penetration? WeaponInteger `-1` means unlimited penetration.
---@field charge_penetration_max? WeaponInteger
---@field charge_controls_laser? WeaponBoolean
---@field direct_melee? WeaponBoolean

---@class WeaponPvp
---@field damage_scale? WeaponNumber Multiplier applied only in non-cooperative PvP modes.
---@field explosion_damage_scale? WeaponNumber
---@field fire_rate_scale? WeaponNumber Cooldown multiplier; values below 1 fire faster.
---@field ammo_scale? WeaponNumber
---@field projectile_speed_scale? WeaponNumber
---@field melee_range_scale? WeaponNumber
---@field knockback_scale? WeaponNumber

---@class WeaponVisuals
---@field render_type? WeaponInteger Use a value from `weapon.visual`.
---@field visual_size? WeaponVector Shorthand for `visual_size_x` and `visual_size_y`.
---@field visual_size_x? WeaponInteger
---@field visual_size_y? WeaponInteger
---@field visual_size2? WeaponVector
---@field visual_size2_x? WeaponInteger
---@field visual_size2_y? WeaponInteger
---@field render_offset? WeaponVector
---@field render_offset_x? WeaponNumber
---@field render_offset_y? WeaponNumber
---@field muzzle_offset? WeaponVector
---@field muzzle_offset_x? WeaponNumber
---@field muzzle_offset_y? WeaponNumber
---@field projectile_offset? WeaponVector
---@field projectile_offset_x? WeaponNumber
---@field projectile_offset_y? WeaponNumber
---@field hand_offset? WeaponVector
---@field hand_offset_x? WeaponNumber
---@field hand_offset_y? WeaponNumber
---@field color_swap? WeaponVector
---@field color_swap_x? WeaponNumber
---@field color_swap_y? WeaponNumber
---@field render_recoil? WeaponNumber
---@field projectile_size? WeaponNumber
---@field projectile_sprite? WeaponNumber
---@field projectile_trace_type? WeaponInteger
---@field trace_threshold? WeaponNumber
---@field explosion_sprite? WeaponInteger
---@field explosion_sound? WeaponInteger
---@field fire_sound? WeaponInteger
---@field fire_sound2? WeaponInteger
---@field muzzle_type? WeaponInteger
---@field muzzle_amount? WeaponInteger
---@field screenshake_amount? WeaponNumber
---@field impact_effect? WeaponInteger Use a value from `weapon.impact`.

---@class WeaponAssets
---@field held_image? string Package-relative PNG resource path.
---@field projectile_image? string Package-relative PNG resource path.
---@field muzzle_image? string Package-relative PNG resource path.
---@field fire_sound? string Package-relative WV resource path.
---@field fire_sound2? string Package-relative WV resource path.
---@field explosion_sound? string Package-relative WV resource path.

---@class WeaponLocalization
---@field name string Source localization key.
---@field description? string Source localization key.

---@class WeaponNameLocalization
---@field name string Source localization key.

---@class WeaponDefinition
---@field id string Local ID for Workshop content; full stable ID for embedded official content.
---@field schema 4
---@field inherits? string Stable ID of an earlier definition. Cannot be combined with templates or identity fields.
---@field kind? WeaponKind Required for standalone definitions.
---@field static_type? integer Required for standalone static definitions; use `weapon.static`.
---@field part1? integer Required for standalone modular definitions; use `weapon.part1`.
---@field part2? integer Required for standalone modular definitions; use `weapon.part2`.
---@field max_level? integer Integer in `0..15`; required for standalone definitions.
---@field behavior? WeaponBehavior[] Required for standalone definitions and inherited otherwise.
---@field combat_template? integer Definition DSL convenience field; use `weapon.combat`.
---@field visual_template? integer Definition DSL convenience field; use `weapon.visual`.
---@field combat? WeaponCombat Sparse for inherited definitions, complete after template expansion otherwise.
---@field pvp? WeaponPvp Optional PvP-only combat multipliers; omitted fields default to 1.
---@field visuals? WeaponVisuals Sparse for inherited definitions, complete after template expansion otherwise.
---@field assets? WeaponAssets Workshop-only resource overrides.
---@field localization? WeaponLocalization Workshop-only localization keys.

---@class WeaponComponentDefinition
---@field slot WeaponComponentSlot Use `weapon.component_slot.frame` or `.part`.
---@field id integer Component numeric ID from `weapon.part1` or `weapon.part2`.
---@field name string UTF-8 display/localization key, 1..63 bytes.

---@class WeaponModuleDefinition
---@field schema 1
---@field id string Local module ID.
---@field slot WeaponComponentSlot Use `weapon.module_slot.frame` or `.part`.
---@field tags? string[] Lowercase selector tags.
---@field localization WeaponNameLocalization

---@class WeaponModuleContext
---@field stable_id string
---@field name string
---@field tags string[]

---@class WeaponComposeContext
---@field frame WeaponModuleContext
---@field part WeaponModuleContext

---@class WeaponComposedDefinition
---@field max_level integer Integer in `0..15`.
---@field behavior WeaponBehavior[]
---@field combat_template? integer Use a value from `weapon.combat`.
---@field visual_template? integer Use a value from `weapon.visual`.
---@field combat WeaponCombat
---@field pvp? WeaponPvp Optional PvP-only combat multipliers.
---@field visuals WeaponVisuals
---@field assets? WeaponAssets
---@field localization WeaponLocalization

---@class WeaponComposeDefinition
---@field schema 1
---@field id string Local compose declaration ID.
---@field frames string[] Dense array of 1..32 unique local IDs, visible stable module IDs, or `tag:<name>` selectors.
---@field parts string[] Dense array of 1..32 unique local IDs, visible stable module IDs, or `tag:<name>` selectors.
---@field build fun(ctx: WeaponComposeContext): WeaponComposedDefinition Must not capture locals; the context and global API are immutable.

---@class ForgeWeaponContext
---@field stable_id string
---@field frame_module? string
---@field part_module? string
---@field tags string[]
---@field level integer
---@field ammo integer
---@field max_ammo integer

---@class ForgeContext
---@field target ForgeWeaponContext
---@field material ForgeWeaponContext
---@field base_cost integer
---@field level_cost integer

---@class ForgeResult
---@field product string Stable weapon ID visible to this package.
---@field level integer Integer in `0..15`.
---@field cost integer Integer in `0..999`.

---@class ForgeRecipeDefinition
---@field schema 1
---@field id string Local recipe ID.
---@field priority integer Highest unique matching priority wins.
---@field targets string[] Dense array of 1..32 unique local IDs, visible stable weapon IDs, or `tag:<name>` selectors.
---@field materials string[] Dense array of 1..32 unique local IDs, visible stable weapon IDs, compose IDs, or `tag:<name>` selectors.
---@field localization WeaponNameLocalization
---@field resolve fun(ctx: ForgeContext): ForgeResult? Must not capture locals; the context and global API are immutable.

---@class WeaponAttackDefinition
---@field schema 4
---@field kind WeaponAttackKind
---@field type integer
---@field name? string Used by official generation helpers; ignored by the native schema.
---@field combat_template? integer Use a value from `weapon.combat`.
---@field visual_template? integer Use a value from `weapon.visual`.
---@field combat WeaponCombat
---@field visuals WeaponVisuals

---@class WeaponSpawn
---@field speed? integer Default 900; range `0..4000`. For rays, use `range` instead.
---@field range? integer Ray range; default 600 and bounded to `0..4000`.
---@field life? integer Lifetime in ticks; default 100 (3 for rays), range `1..1800`.
---@field damage? integer Default 1; range `0..1000`.
---@field radius? integer Default 6; range `0..512`.
---@field bounces? integer Default 0; range `0..32`.
---@field gravity? integer Default 0; range `-1000..1000`.
---@field count? integer Default 1; range `1..16`.

---@class WeaponContext
local WeaponContext = {}

---@param index integer State slot in `0..7`.
---@return integer
function WeaponContext:state_get(index) end

---@param index integer State slot in `0..7`.
---@param value integer
function WeaponContext:state_set(index, value) end

---Returns deterministic state-derived random data. With no arguments it returns the raw value;
---with one argument that value is both interval bounds; with two it uses `low..high`.
---@overload fun(self: WeaponContext): integer
---@overload fun(self: WeaponContext, high: integer): integer
---@param low? integer
---@param high? integer
---@return integer
function WeaponContext:random(low, high) end

---@param spawn WeaponSpawn
---@return boolean accepted
function WeaponContext:spawn_projectile(spawn) end

---@param spawn WeaponSpawn
---@return boolean accepted
function WeaponContext:spawn_ray(spawn) end

---@param spawn WeaponSpawn
---@return boolean accepted
function WeaponContext:spawn_area(spawn) end

---@param spawn WeaponSpawn
---@return boolean accepted
function WeaponContext:spawn_summon(spawn) end

---@param kind integer
---@param value? integer
function WeaponContext:visual(kind, value) end

---@param ... integer At most eight bounded integer arguments; interpretation is host-defined.
---@return boolean accepted
function WeaponContext:timer_set(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:ammo_add(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:charge_set(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:explode(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:release_weapon(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:controller_trigger(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:drop_pickup(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:create_electrowall(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:sound(...) end
---@param ... integer
---@return boolean accepted
function WeaponContext:bomb_trigger(...) end

---@alias WeaponEventHandler fun(ctx: WeaponContext)

---@class WeaponCurveApi
---@field levels fun(values: table<integer, number|boolean>): WeaponNumberCurve|WeaponBooleanCurve Exactly 16 values.
---@field linear fun(base: number, amount: number, max_level: integer): WeaponNumberCurve
---@field integer_linear fun(base: number, amount: number, max_level: integer): WeaponIntegerCurve
---@field cost fun(base: number, amount: number, max_level: integer): WeaponIntegerCurve
---@field cycle fun(values: (number|boolean)[]): WeaponNumberCurve|WeaponBooleanCurve Repeats 1..16 values over 16 levels.
---@field unlock fun(level: integer): WeaponBooleanCurve False before the level, true from it onward.
---@field step fun(base: number, increment: number): WeaponNumberCurve Adds increment per level.
---@field switch fun(level: integer, before: number|boolean, after: number|boolean): WeaponNumberCurve|WeaponBooleanCurve
---@field at fun(level: integer, normal: number|boolean, value: number|boolean): WeaponNumberCurve|WeaponBooleanCurve

---@class WeaponApi
---@field kind { static: 'static', modular: 'modular' }
---@field combat { none: integer, melee: integer, projectile: integer, charge: integer, hold: integer, throw: integer, activate: integer }
---@field visual { none: integer, weapon: integer, compact_weapon: integer, item: integer, melee: integer, melee_small: integer, spin: integer }
---@field path { standard: integer, log: integer, rocket: integer }
---@field impact { none: integer, ballistic: integer, launcher: integer, green: integer, electric: integer, sprite: integer, electric_area: integer, sparks: integer, sprite_electric: integer }
---@field static table<string, integer>
---@field part1 { base1: integer, base2: integer, base3: integer, base4: integer, base5: integer, base6: integer, melee: integer, spin: integer }
---@field part2 { barrel1: integer, barrel2: integer, barrel3: integer, barrel4: integer, charge: integer, capacitor: integer, rail: integer, melee1: integer, melee2: integer, melee3: integer, melee4: integer, melee5: integer, melee6: integer }
---@field component_slot { frame: 'frame', part: 'part' }
---@field module_slot { frame: 'frame', part: 'part' }
---@field curve WeaponCurveApi
weapon = {}

---@param definition WeaponDefinition
function weapon.define(definition) end

---@param definition WeaponComponentDefinition
function weapon.component(definition) end

---@param definition WeaponModuleDefinition
function weapon.module(definition) end

---@param definition WeaponComposeDefinition
function weapon.compose(definition) end

---@param stable_id string
---@param visuals WeaponVisuals
function weapon.override_visuals(stable_id, visuals) end

---@param stable_id string
---@param combat WeaponCombat
function weapon.override_combat(stable_id, combat) end

---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_fire(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_tick(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_charge(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_release(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_trigger(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_throw(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_activate(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_collision(stable_id, handler) end
---@param stable_id string
---@param handler WeaponEventHandler
function weapon.on_destroy(stable_id, handler) end

---@class AttackProfileApi
attack_profile = {}

---@param definition WeaponAttackDefinition
function attack_profile.define(definition) end

---@class ForgeApi
forge = {}

---@param definition ForgeRecipeDefinition
function forge.recipe(definition) end

---@class WeaponPresentationContext
local WeaponPresentationContext = {}

---@param index integer State slot in `0..7`.
---@return integer
function WeaponPresentationContext:state(index) end

---@param localization_key string
---@param x integer
---@param y integer
---@param size? integer Default 12.
function WeaponPresentationContext:text(localization_key, x, y, size) end

---@param value integer
---@param maximum integer
---@param x integer
---@param y integer
---@param width integer
---@param height? integer Default 6.
function WeaponPresentationContext:bar(value, maximum, x, y, width, height) end

---@class PresentationApi
presentation = {}

---@param stable_id string
---@param handler fun(ctx: WeaponPresentationContext)
function presentation.on_hud(stable_id, handler) end
