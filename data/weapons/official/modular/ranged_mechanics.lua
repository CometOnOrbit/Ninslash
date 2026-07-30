-- Preserve each ranged combination's firing mechanism independently of balance
-- formulas. These rules describe projectile/laser identity and trigger behavior.

local base_ids = {"base1", "base2", "base3", "base4", "base5", "base6"}
local barrel_ids = {"barrel1", "barrel2", "barrel3", "barrel4", "charge", "capacitor", "rail"}

local bases = {
  {curvature = 2},
  {curvature = 6, explosive = true},
  {curvature = 1.5, electro = 1},
  {curvature = 1.8, aimline = true},
  {curvature = 1.5, electro = 1, laser = true, aimline = true, laser_range = 900},
  {curvature = 2, bounces = weapon.curve.integer_linear(3, 4, 4)},
}

local barrels = {
  {full_auto = true, curvature = 1, reload = 0.9},
  {
    full_auto = weapon.curve.unlock(4), curvature = 1,
    burst = weapon.curve.integer_linear(1, 2, 4), reload = 0.9,
  },
  {
    full_auto = weapon.curve.unlock(4), curvature = 0.4,
    reload = 0.9, aimline = true, spread = 0,
  },
  {full_auto = true, curvature = 0.6, reload = 0.9},
  {
    full_auto = true, curvature = weapon.curve.linear(1, -0.4, 4),
    reload = 0.9, turret = false,
  },
  {
    full_auto = true, curvature = 1, reload = 0.9,
    turret = false, aimline = true, spread = 0,
  },
  {
    full_auto = true, curvature = 0.5, reload = 0.9,
    aimline = true, spread = 0,
  },
}

for base_index = 1, #bases do
  local base = bases[base_index]
  for barrel_index = 1, #barrels do
    local barrel = barrels[barrel_index]
    local override = {
      full_auto = barrel.full_auto,
      uses_ammo = true,
      projectile_spread = barrel.spread or 0.05,
      projectile_curvature = type(barrel.curvature) == "table" and
        weapon.curve.linear(base.curvature, -base.curvature * 0.4, 4) or
        base.curvature * barrel.curvature,
      burst_count = barrel.burst or 0,
      burst_reload = barrel.reload,
      valid_for_turret = barrel.turret ~= false,
      explosive_projectile = base.explosive or false,
      electro_amount = base.electro and weapon.curve.linear(base.electro, 0.7, 4) or 0,
      laser_weapon = base.laser or false,
      aimline = base.aimline or barrel.aimline or false,
      laser_range = base.laser_range or 0,
      laser_charge = 0,
      projectile_bounces = base.bounces or 0,
    }

    -- Heavy-frame scatter and long barrels are the only native BASE3 lasers.
    if base_index == 3 and barrel_index == 2 then
      override.electro_amount = weapon.curve.linear(0.5, 0.3, 4)
      override.laser_weapon = true
      override.laser_range = 450
      override.laser_charge = -1
    elseif base_index == 3 and barrel_index == 3 then
      override.laser_weapon = true
      override.laser_range = 700
      override.laser_charge = weapon.curve.integer_linear(60, 60, 4)
    elseif base_index == 3 and barrel_index == 4 then
      override.electro_amount = weapon.curve.linear(0.3, 0.3, 4)
      override.burst_count = weapon.curve.integer_linear(3, 4, 4)
      override.burst_reload = 0.25
    elseif base_index == 3 and barrel_index == 5 then
      override.electro_amount = 0
    end

    -- Precision-frame variants inherit BASE3 barrel mechanics, then become
    -- reliable hitscan weapons with clamped laser reach.
    if base_index == 5 then
      override.laser_weapon = true
      override.aimline = true
      override.laser_range = barrel_index == 2 and 700 or 900
      if barrel_index == 2 then override.laser_charge = -1 end
      if barrel_index == 2 then
        override.electro_amount = weapon.curve.linear(0.5, 0.3, 4)
      end
      if barrel_index == 3 then
        override.laser_charge = weapon.curve.integer_linear(60, 60, 4)
      end
      if barrel_index == 4 then
        override.electro_amount = weapon.curve.linear(0.3, 0.3, 4)
        override.burst_count = weapon.curve.integer_linear(3, 4, 4)
        override.burst_reload = 0.25
      end
      if barrel_index == 5 then override.electro_amount = 0 end
    end

    -- The original scatter barrels for balanced and ricochet frames use a
    -- two-shot burst and faster reload.
    if (base_index == 1 or base_index == 6) and barrel_index == 2 then
      override.burst_count = weapon.curve.integer_linear(2, 2, 4)
      override.burst_reload = 0.4
    end

    local explosive_burst = base_index == 2 and
      (barrel_index == 1 or barrel_index == 6 or barrel_index == 7)
    if explosive_burst then
      override.burst_count = weapon.curve.integer_linear(1, 2, 4)
      override.burst_reload = weapon.curve.linear(0.9, -0.25, 4)
    end

    weapon.override_combat(
      "official:modular:" .. base_ids[base_index] .. "-" .. barrel_ids[barrel_index],
      override)
  end
end
