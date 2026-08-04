-- Public, deterministic declaration helpers shared by official and Workshop content.
-- The native registration functions are captured here and hidden before content runs.
local native_weapon_define = weapon._define
local native_component_define = weapon._define_component
local native_module_define = weapon._define_module
local native_compose_define = weapon._define_compose
local native_forge_recipe = forge._define_recipe
local native_visual_override = weapon._override_visuals
local native_combat_override = weapon._override_combat
local native_attack_define = attack_profile._define
local raw_next = next

local function copy(source)
  local result = {}
  local key, value = raw_next(source)
  while key ~= nil do
    result[key] = value
    key, value = raw_next(source, key)
  end
  return result
end

local combat_defaults = {
  firing_type = 0,
  fire_rate = 0,
  full_auto = false,
  max_ammo = 0,
  uses_ammo = false,
  shot_spread = 1,
  projectile_spread = 0.05,
  projectile_speed = 0,
  projectile_curvature = 0,
  projectile_life = 0,
  projectile_damage = 0,
  projectile_knockback = 0,
  explosion_size = 0,
  explosion_damage = 0,
  melee_hit_radius = 0,
  weapon_knockback = 0,
  burst_count = 0,
  burst_reload = 0,
  ai_attack_range = 0,
  valid_for_turret = false,
  throw_force = 0,
  flame_amount = 0,
  electro_amount = 0,
  explosive_projectile = false,
  laser_weapon = false,
  cursor_weapon = 0,
  cost = 0,
  aimline = false,
  projectile_pos_type = 0,
  laser_range = 0,
  laser_charge = 0,
  projectile_bounces = 0,
  auto_pick = false,
  charge_damage_min = 1,
  charge_damage_max = 1,
  charge_range_min = 1,
  charge_range_max = 1,
  charge_power_min = 1,
  charge_power_max = 1,
  projectile_penetration = 0,
  charge_penetration_max = 0,
  charge_controls_laser = false,
  direct_melee = false,
}

local visual_defaults = {
  render_type = 0,
  visual_size_x = 0,
  visual_size_y = 0,
  visual_size2_x = 0,
  visual_size2_y = 0,
  render_offset_x = 0,
  render_offset_y = 0,
  muzzle_offset_x = 0,
  muzzle_offset_y = 0,
  projectile_offset_x = 0,
  projectile_offset_y = 0,
  hand_offset_x = -26,
  hand_offset_y = 8,
  color_swap_x = 0,
  color_swap_y = 0,
  render_recoil = 0,
  projectile_size = 1,
  projectile_sprite = 0,
  projectile_trace_type = 0,
  trace_threshold = 0,
  explosion_sprite = 0,
  explosion_sound = 0,
  fire_sound = -1,
  fire_sound2 = -1,
  muzzle_type = 0,
  muzzle_amount = 10,
  screenshake_amount = 0,
  impact_effect = 0,
  static_sprite = -1,
}

local function expand(defaults, template_value, overrides, template_name)
  if template_value == nil then
    return overrides
  end
  if type(template_value) ~= "number" then
    error(template_name .. " must use a weapon template constant")
  end
  local result = copy(defaults)
  result[template_name == "combat_template" and "firing_type" or "render_type"] = template_value
  if overrides ~= nil then
    local key, value = raw_next(overrides)
    while key ~= nil do
      result[key] = value
      key, value = raw_next(overrides, key)
    end
  end
  return result
end

local vectors = {
  {"visual_size", "visual_size_x", "visual_size_y"},
  {"visual_size2", "visual_size2_x", "visual_size2_y"},
  {"render_offset", "render_offset_x", "render_offset_y"},
  {"muzzle_offset", "muzzle_offset_x", "muzzle_offset_y"},
  {"projectile_offset", "projectile_offset_x", "projectile_offset_y"},
  {"hand_offset", "hand_offset_x", "hand_offset_y"},
  {"color_swap", "color_swap_x", "color_swap_y"},
}

local function flatten_vectors(visuals)
  if visuals == nil then return end
  for index = 1, #vectors do
    local names = vectors[index]
    local value = visuals[names[1]]
    if value ~= nil then
      if type(value) ~= "table" or #value ~= 2 then
        error(names[1] .. " must contain exactly two values")
      end
      if visuals[names[2]] ~= nil or visuals[names[3]] ~= nil then
        error(names[1] .. " duplicates its expanded fields")
      end
      visuals[names[2]], visuals[names[3]], visuals[names[1]] = value[1], value[2], nil
    end
  end
end

weapon.kind = {static = "static", modular = "modular"}
weapon.combat = {none = 0, melee = 1, projectile = 2, charge = 3, hold = 4, throw = 5, activate = 6}
weapon.visual = {none = 0, weapon = 1, compact_weapon = 2, item = 3, melee = 4, melee_small = 5, spin = 6}
weapon.path = {standard = 0, log = 1, rocket = 2}
weapon.impact = {
  none = 0, ballistic = 1, launcher = 2, green = 3, electric = 4,
  sprite = 5, electric_area = 6, sparks = 7, sprite_electric = 8,
}
weapon.static = {
  tool = 0, gun1 = 1, gun2 = 2, grenade1 = 3, grenade2 = 4, grenade3 = 5,
  bazooka = 6, bouncer = 7, chainsaw = 8, flamer = 9, upgrade = 10,
  shield = 11, respawner = 12, mask1 = 13, mask2 = 14, mask3 = 15,
  mask4 = 16, mask5 = 17, invis = 18, electrowall = 19, areashield = 20,
  syringe = 21, cluster = 22, shuriken = 23, claw = 24, bomb = 25, ball = 26,
  flash_grenade = 27, blind_grenade = 28,
}
weapon.part1 = {base1 = 1, base2 = 2, base3 = 3, base4 = 4, base5 = 5, base6 = 6, melee = 7, spin = 8}
weapon.part2 = {
  barrel1 = 1, barrel2 = 2, barrel3 = 3, barrel4 = 4, charge = 5,
  capacitor = 6, rail = 7, melee1 = 8, melee2 = 9, melee3 = 10,
  melee4 = 11, melee5 = 12, melee6 = 13,
}
weapon.component_slot = {frame = "frame", part = "part"}
weapon.module_slot = weapon.component_slot
weapon.curve = {
  levels = weapon.levels,
  linear = weapon.linear,
  integer_linear = weapon.integer_linear,
  cost = weapon.cost_curve,
}

function weapon.curve.cycle(values)
  if type(values) ~= "table" or #values < 1 or #values > 16 then
    error("weapon.curve.cycle requires 1..16 values")
  end
  local result = {}
  for level = 1, 16 do
    result[level] = values[(level - 1) % #values + 1]
  end
  return weapon.levels(result)
end

function weapon.curve.unlock(level)
  if type(level) ~= "number" or level < 0 or level > 15 or level % 1 ~= 0 then
    error("weapon.curve.unlock level must be an integer in 0..15")
  end
  local result = {}
  for index = 1, 16 do
    result[index] = index - 1 >= level
  end
  return weapon.levels(result)
end

function weapon.curve.step(base, increment)
  return weapon.linear(base, increment * 15, 15)
end

function weapon.curve.switch(level, before, after)
  if type(level) ~= "number" or level < 0 or level > 15 or level % 1 ~= 0 then
    error("weapon.curve.switch level must be an integer in 0..15")
  end
  local result = {}
  for index = 1, 16 do
    result[index] = index - 1 < level and before or after
  end
  return weapon.levels(result)
end

function weapon.curve.at(level, normal, value)
  if type(level) ~= "number" or level < 0 or level > 15 or level % 1 ~= 0 then
    error("weapon.curve.at level must be an integer in 0..15")
  end
  local result = {}
  for index = 1, 16 do
    result[index] = index - 1 == level and value or normal
  end
  return weapon.levels(result)
end

function weapon.define(definition)
  if definition.inherits ~= nil and (definition.combat_template ~= nil or definition.visual_template ~= nil) then
    error("inherits cannot be combined with profile templates")
  end
  flatten_vectors(definition.visuals)
  definition.combat = expand(combat_defaults, definition.combat_template, definition.combat, "combat_template")
  definition.visuals = expand(visual_defaults, definition.visual_template, definition.visuals, "visual_template")
  definition.combat_template, definition.visual_template = nil, nil
  return native_weapon_define(definition)
end

function weapon.component(definition)
  return native_component_define(definition)
end

function weapon.module(definition)
  return native_module_define(definition)
end

function weapon.compose(definition)
  return native_compose_define(definition)
end

function forge.recipe(definition)
  return native_forge_recipe(definition)
end

function weapon.override_visuals(stable_id, visuals)
  flatten_vectors(visuals)
  return native_visual_override(stable_id, visuals)
end

function weapon.override_combat(stable_id, combat)
  return native_combat_override(stable_id, combat)
end

function attack_profile.define(definition)
  flatten_vectors(definition.visuals)
  definition.combat = expand(combat_defaults, definition.combat_template, definition.combat, "combat_template")
  definition.visuals = expand(visual_defaults, definition.visual_template, definition.visuals, "visual_template")
  definition.combat_template, definition.visual_template = nil, nil
  return native_attack_define(definition)
end

weapon._define = nil
weapon._define_component = nil
weapon._define_module = nil
weapon._define_compose = nil
weapon._override_visuals = nil
weapon._override_combat = nil
attack_profile._define = nil
forge._define_recipe = nil
