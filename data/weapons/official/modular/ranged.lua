-- Modular ranged weapons are derived from independently tunable base and barrel
-- components. Adding or balancing a component changes its six or seven intended
-- combinations without duplicating complete weapon profiles.

local bases = {
  {
    id = "base1", part = weapon.part1.base1,
    fire_rate = 280, ammo = 25, precision_ammo = 15, precision_ammo_growth = 15,
    damage = 20, speed = 1400,
    curvature = 2, knockback = 3.3, explosion = 100.8,
    ai_range = 900, projectile_size = 0.84,
    shotgun_fire = 500, shotgun_burst = 2, wide_spread = true,
    impact = weapon.impact.ballistic,
  },
  {
    id = "base2", part = weapon.part1.base2,
    fire_rate = 490, ammo = 15, precision_ammo = 10, precision_ammo_growth = 10,
    damage = 10, speed = 900,
    curvature = 3, knockback = 5.5, explosion = 172.8,
    explosion_damage = 32.864, explosive = true,
    ai_range = 700, projectile_size = 1.44,
    impact = weapon.impact.launcher,
  },
  {
    id = "base3", part = weapon.part1.base3,
    fire_rate = 370, ammo = 20, precision_ammo = 15, precision_ammo_growth = 10,
    damage = 25, speed = 1200,
    curvature = 1.5, knockback = 0, explosion = 172.8,
    electric = true, ai_range = 900, projectile_size = 1.2,
    speed_growth = 400, rapid_fire = 500, rapid_burst = 3, wide_spread = true,
    impact = weapon.impact.electric,
  },
  {
    id = "base4", part = weapon.part1.base4,
    fire_rate = 500, ammo = 15, precision_ammo = 15, precision_ammo_growth = 10,
    damage = 32, speed = 1700,
    curvature = 2.5, knockback = 6.6, explosion = 187.2,
    ai_range = 900, projectile_size = 1.56,
    impact = weapon.impact.green,
  },
  {
    id = "base5", part = weapon.part1.base5,
    fire_rate = 407, ammo = 16, precision_ammo = 12, precision_ammo_growth = 8,
    damage = 20, speed = 1200,
    curvature = 1.5, knockback = 0, explosion = 172.8,
    electric = true, ai_range = 900, projectile_size = 1.2,
    speed_growth = 400, rapid_fire = 550, rapid_burst = 3,
    wide_spread = true, charge_projectile = true,
    impact = weapon.impact.electric,
  },
  {
    id = "base6", part = weapon.part1.base6,
    fire_rate = 313.6, ammo = 20, precision_ammo = 12, precision_ammo_growth = 12,
    damage = 15.6, speed = 1260,
    curvature = 2, knockback = 3.3, explosion = 100.8,
    ai_range = 900, projectile_size = 0.84,
    range = 990, shotgun_fire = 560, shotgun_burst = 2,
    wide_spread = true, charge_projectile = true,
    bounces = 3, bounce_growth = 4, sprite_offset = 12,
    impact = weapon.impact.none,
  },
}

local barrels = {
  {
    id = "barrel1", part = weapon.part2.barrel1,
    firing = weapon.combat.projectile,
    fire = 1, fire_growth = -0.28,
    ammo = 1, ammo_growth = 0.6,
    damage = 1, damage_growth = 0.6,
    speed = 1, range = 1100,
    full_auto = true, spread = 1,
    explosion = 1, explosion_growth = 36,
    projectile_size = 1, projectile_size_growth = 0.36,
    projectile_offset = 60, sprite = 1, trace = 1, fire_sound = 106,
  },
  {
    id = "barrel2", part = weapon.part2.barrel2,
    firing = weapon.combat.projectile,
    fire = 1.1, fire_growth = -0.22,
    ammo = 0.8, ammo_growth = 0.6,
    damage = 0.35, damage_growth = 0.2,
    speed = 1.3, range = 470,
    full_auto = weapon.curve.unlock(4), spread = 4, spread_growth = 1,
    burst = 1, burst_growth = 2,
    explosion = 0.54, explosion_growth = 19.5,
    projectile_size = 1.7, projectile_size_growth = 0.36,
    projectile_offset = 58, sprite = 2, trace = -1, fire_sound = 107,
    explosion_sound = 11, screenshake = 0.13,
  },
  {
    id = "barrel3", part = weapon.part2.barrel3,
    firing = weapon.combat.projectile,
    fire = 1.2, fire_growth = -0.36,
    ammo = 0.65, ammo_growth = 0.65,
    damage = 1.5, damage_growth = 0.75,
    speed = 2.5, range = 1100,
    full_auto = false, spread = 1,
    explosion = 1.08, explosion_growth = 39,
    projectile_size = 0.8, projectile_size_growth = 0.36,
    projectile_offset = 70, sprite = 3, trace = 2, fire_sound = 108,
  },
  {
    id = "barrel4", part = weapon.part2.barrel4,
    firing = weapon.combat.projectile,
    fire = 0.6, fire_growth = -0.17,
    ammo = 1.4, ammo_growth = 0.8,
    damage = 0.8, damage_growth = 0.25,
    speed = 1.4, range = 1100,
    full_auto = true, spread = 1,
    explosion = 0.67, explosion_growth = 24,
    projectile_size = 1.2, projectile_size_growth = 0.3,
    projectile_offset = 56, sprite = 4, trace = 3, fire_sound = 109,
  },
  {
    id = "charge", part = weapon.part2.charge,
    firing = weapon.combat.charge,
    fire = 0.65, fire_growth = -0.13,
    ammo = 0, ammo_growth = 0,
    damage = 0, damage_growth = 0,
    speed = 1, range = 1100,
    full_auto = false, spread = 1,
    explosion = 0.67, explosion_growth = 0.84,
    projectile_size = 1.4, projectile_size_growth = 0.6,
    projectile_offset = 62, sprite = 5, trace = 4, fire_sound = 110,
  },
  {
    id = "capacitor", part = weapon.part2.capacitor,
    firing = weapon.combat.charge,
    fire = 1.5, fire_growth = -0.42,
    ammo = 0.6, ammo_growth = 0.4,
    damage = 2, damage_growth = 1.2,
    speed = 1, range = 1100,
    full_auto = false, spread = 1,
    explosion = 1, explosion_growth = 36,
    projectile_size = 1.5, projectile_size_growth = 0.45,
    projectile_offset = 60, sprite = 6, trace = 5, fire_sound = 111,
  },
  {
    id = "rail", part = weapon.part2.rail,
    firing = weapon.combat.projectile,
    fire = 1.35, fire_growth = -0.38,
    ammo = 0.52, ammo_growth = 0.36,
    damage = 1.15, damage_growth = 0.69,
    speed = 1.2, range = 1320,
    full_auto = false, spread = 1,
    explosion = 1, explosion_growth = 36,
    projectile_size = 0.7, projectile_size_growth = 0.3,
    projectile_offset = 70, sprite = 7, trace = 6, fire_sound = 112,
    bounces = 1,
    penetration = -1,
  },
}

local function behavior(base_index, barrel_index)
  if barrel_index == 5 and base_index == 1 then return {"charged_burst"} end
  if barrel_index == 6 and base_index == 1 then return {"charged_burst", "capacitor"} end
  if barrel_index == 6 then return {"capacitor"} end
  if barrel_index == 7 then return {"rail"} end
  return {}
end

for base_index = 1, #bases do
  local base = bases[base_index]
  for barrel_index = 1, #barrels do
    local barrel = barrels[barrel_index]
    local speed = base.speed * barrel.speed
    local speed_growth = (barrel_index == 1 or barrel_index == 6 or barrel_index == 7) and (base.speed_growth or 0) or 0
    local damage_factor = barrel.damage
    local ammo_factor = barrel.ammo
    if barrel_index == 5 and base.charge_projectile then
      damage_factor, ammo_factor = 1, 1
    end
    local damage = base.damage * damage_factor
    local ammo = base.ammo * ammo_factor
    local explosion_damage = base.explosion_damage or 0
    local fire_rate = base.fire_rate * barrel.fire
    local fire_growth = base.fire_rate * barrel.fire_growth
    local burst = barrel.burst or 0
    local burst_growth = barrel.burst_growth or 0
    if barrel_index == 2 and base.shotgun_fire then
      fire_rate, fire_growth = base.shotgun_fire, 0
      burst, burst_growth = base.shotgun_burst, 2
    elseif barrel_index == 4 and base.rapid_fire then
      fire_rate, fire_growth = base.rapid_fire, -30
      burst, burst_growth = base.rapid_burst, 4
    elseif base.explosive and (barrel_index == 1 or barrel_index == 6 or barrel_index == 7) then
      burst, burst_growth = 1, 2
    end
    local spread = base.wide_spread and barrel_index == 2 and 5 or barrel.spread
    local spread_growth = base.wide_spread and barrel_index == 2 and 2 or (barrel.spread_growth or 0)
    local ammo_growth = base.ammo * (barrel_index == 5 and base.charge_projectile and 0.6 or barrel.ammo_growth)
    if barrel_index == 3 then
      ammo, ammo_growth = base.precision_ammo, base.precision_ammo_growth
    end
    local damage_growth = base.damage * (barrel_index == 5 and base.charge_projectile and 0.6 or barrel.damage_growth)
    local range = base.range or barrel.range
    local life = range / speed
    local life_growth = range / (speed + speed_growth) - life
    local knockback = base.knockback * math.min(damage_factor, 1.2)
    local knockback_growth = base.knockback * math.min(barrel.damage_growth, 0.8)
    local explosion = base.explosion * barrel.explosion
    local explosion_growth = barrel_index == 5 and base.explosion * 0.84 or barrel.explosion_growth
    local blast_damage = explosion_damage * barrel.explosion
    local blast_growth = explosion_damage * barrel.explosion_growth / base.explosion
    if barrel_index == 5 then blast_growth = explosion_damage * 0.84 end

    weapon.define {
      id = "official:modular:" .. base.id .. "-" .. barrel.id,
      schema = 4,
      behavior = behavior(base_index, barrel_index),
      kind = weapon.kind.modular,
      part1 = base.part,
      part2 = barrel.part,
      max_level = 4,

      combat_template = barrel.firing,
      visual_template = weapon.visual.weapon,

      combat = {
        fire_rate = weapon.curve.linear(fire_rate, fire_growth, 4),
        full_auto = barrel.full_auto,
        max_ammo = weapon.curve.integer_linear(ammo, ammo_growth, 4),
        uses_ammo = ammo > 0,
        shot_spread = weapon.curve.integer_linear(spread, spread_growth, 4),
        projectile_spread = 0.05,
        projectile_speed = weapon.curve.linear(speed, speed_growth, 4),
        projectile_curvature = base.curvature,
        projectile_life = weapon.curve.linear(life, life_growth, 4),
        projectile_damage = weapon.curve.linear(damage, damage_growth, 4),
        projectile_knockback = weapon.curve.linear(knockback, knockback_growth, 4),
        explosion_size = weapon.curve.linear(explosion, explosion_growth, 4),
        explosion_damage = weapon.curve.linear(blast_damage, blast_growth, 4),
        weapon_knockback = weapon.curve.linear(1.5, 0.9, 4),
        burst_count = weapon.curve.integer_linear(burst, burst_growth, 4),
        burst_reload = burst > 0 and 0.9 or 0,
        ai_attack_range = barrel_index == 2 and 600 or base.ai_range,
        valid_for_turret = barrel_index ~= 6,
        electro_amount = base.electric and weapon.curve.linear(0.5, 0.5, 4) or 0,
        explosive_projectile = base.explosive or false,
        laser_weapon = base.electric or false,
        aimline = base.electric or barrel_index == 7,
        laser_range = base.electric and 900 or 0,
        cursor_weapon = weapon.curve.cycle {3, 9, 5, 1, 7},
        cost = weapon.curve.cost(10, 20, 4),
        projectile_bounces = weapon.curve.integer_linear(
          base.bounces or barrel.bounces or 0, base.bounce_growth or 0, 4),
        auto_pick = true,
        charge_damage_min = barrel_index == 6 and 0.25 or 1,
        charge_range_min = barrel_index == 6 and 0.6 or 1,
        projectile_penetration = barrel.penetration or 0,
        charge_penetration_max = barrel_index == 6 and 2 or 0,
        charge_controls_laser = barrel_index == 6,
      },
      visuals = {
        visual_size = {4, 3},
        visual_size2 = {4, 3},
        render_offset = {24, 0},
        muzzle_offset = {66, 0},
        projectile_offset = {barrel.projectile_offset, -11},
        color_swap_x = weapon.curve.linear(0, 0.25, 4),
        color_swap_y = weapon.curve.linear(0, base_index * 0.15, 4),
        render_recoil = 13 + barrel_index * 0.65,
        projectile_size = weapon.curve.linear(
          base.projectile_size * barrel.projectile_size, barrel.projectile_size_growth, 4),
        projectile_sprite = (base.sprite_offset or 0) + barrel.sprite,
        projectile_trace_type = barrel.trace,
        explosion_sprite = 279,
        explosion_sound = barrel.explosion_sound or 0,
        fire_sound = barrel.fire_sound,
        fire_sound2 = 110,
        screenshake_amount = barrel.screenshake or 0,
        impact_effect = base.impact,
      },
    }
  end
end
