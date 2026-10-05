-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

local native_define = attack_profile.define

local function fill(target, key, value)
  if target[key] == nil then
    target[key] = value
  end
end

local function define(definition)
  definition.schema = definition.schema or 4
  definition.kind = definition.kind or "droid"
  definition.combat_template = definition.combat_template or weapon.combat.melee
  definition.visual_template = definition.visual_template or weapon.visual.melee_small
  local combat = definition.combat or {}
  local visuals = definition.visuals or {}
  definition.combat = combat
  definition.visuals = visuals
  fill(combat, "full_auto", true)
  fill(combat, "burst_reload", 1)
  fill(combat, "cost", 10)
  fill(combat, "auto_pick", true)
  fill(combat, "direct_melee", false)
  fill(visuals, "visual_size", {4, 2})
  fill(visuals, "render_recoil", 12)
  fill(visuals, "impact_effect",
    definition.area and weapon.impact.electric_area or weapon.impact.electric)
  definition.area = nil
  return native_define(definition)
end

define {
  type = 0,
  name = "walker",
  combat = {
    fire_rate = 300,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_damage = 6,
    projectile_knockback = 1,
    electro_amount = 0.5,
    cursor_weapon = 8,
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
  },
}

define {
  type = 8,
  name = "siegebreakercrawler",
  combat = {
    fire_rate = 420,
    projectile_damage = 14,
    projectile_knockback = 34,
    melee_hit_radius = 56,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 9,
  name = "tempeststar",
  area = true,
  combat = {
    fire_rate = 180,
    projectile_speed = 20,
    projectile_life = 1.2,
    projectile_damage = 4,
    projectile_knockback = 2,
    electro_amount = 1,
    cursor_weapon = 2,
    projectile_pos_type = weapon.path.log,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 10,
  name = "splitcrawler",
  combat = {
    fire_rate = 300,
    projectile_damage = 5,
    projectile_knockback = 24,
    melee_hit_radius = 40,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 11,
  name = "kamikazestar",
  combat = {
    fire_rate = 1000,
    projectile_damage = 0,
    cursor_weapon = 2,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 12,
  name = "railstar",
  area = true,
  combat = {
    fire_rate = 2500,
    projectile_damage = 26,
    projectile_penetration = -1,
    laser_weapon = true,
    aimline = true,
    laser_range = 1200,
    cursor_weapon = 8,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 13,
  name = "mendercrawler",
  combat = {
    fire_rate = 400,
    projectile_damage = 3,
    projectile_knockback = 16,
    melee_hit_radius = 40,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 14,
  name = "stalkercrawler",
  combat = {
    fire_rate = 280,
    projectile_damage = 12,
    projectile_knockback = 30,
    melee_hit_radius = 40,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 15,
  name = "teslastar",
  area = true,
  combat = {
    fire_rate = 3000,
    projectile_damage = 9,
    electro_amount = 1,
    cursor_weapon = 2,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 16,
  name = "cyclonecrawler",
  combat = {
    fire_rate = 600,
    projectile_speed = 24,
    projectile_life = 1.2,
    projectile_damage = 8,
    projectile_knockback = 24,
    explosion_size = 100,
    explosion_damage = 8,
    melee_hit_radius = 44,
    explosive_projectile = true,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 1,
  name = "star",
  area = true,
  combat = {
    fire_rate = 300,
    projectile_speed = 24,
    projectile_life = 1.2,
    projectile_damage = 10,
    projectile_knockback = 2,
    electro_amount = 1,
    cursor_weapon = 2,
    projectile_pos_type = weapon.path.log,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 2,
  name = "crawler",
  combat = {
    fire_rate = 300,
    projectile_damage = 6,
    projectile_knockback = 24,
    melee_hit_radius = 40,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 3,
  name = "bosscrawler",
  combat = {
    fire_rate = 300,
    projectile_damage = 10,
    projectile_knockback = 34,
    melee_hit_radius = 60,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 4,
  name = "fly",
  combat = {
    fire_rate = 300,
    cursor_weapon = 4,
  },
  visuals = {
    projectile_size = 0,
  },
}

define {
  type = 5,
  name = "bossstar",
  area = true,
  combat = {
    fire_rate = 300,
    projectile_speed = 24,
    projectile_life = 1.2,
    projectile_damage = 10,
    projectile_knockback = 2,
    electro_amount = 1,
    cursor_weapon = 8,
    projectile_pos_type = weapon.path.log,
  },
  visuals = {
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  type = 6,
  name = "bosswalker",
  combat = {
    fire_rate = 300,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_damage = 10,
    projectile_knockback = 2,
    electro_amount = 0.5,
    cursor_weapon = 2,
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
  },
}

define {
  type = 17,
  name = "foundrywarden",
  combat = {
    fire_rate = 200,
    projectile_speed = 900,
    projectile_life = 1.4,
    projectile_damage = 12,
    projectile_knockback = 6,
    explosion_size = 120,
    electro_amount = 0.25,
    cursor_weapon = 6,
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

define {
  type = 18,
  name = "abyssangler",
  combat = {
    fire_rate = 200,
    projectile_speed = 600,
    projectile_life = 1.4,
    projectile_damage = 12,
    projectile_knockback = 5,
    explosion_size = 120,
    electro_amount = 0.25,
    cursor_weapon = 6,
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

define {
  type = 7,
  name = "bosssplitter",
  combat = {
    fire_rate = 300,
    projectile_damage = 10,
    projectile_knockback = 34,
    melee_hit_radius = 60,
    cursor_weapon = 6,
    direct_melee = true,
  },
  visuals = {
    projectile_size = 0,
  },
}

-- Milestone Boss: movement and hit windows are server-authoritative.
define {
  type = 19,
  name = "railreaper",
  combat = {
    fire_rate = 250,
    projectile_speed = 850,
    projectile_life = 2.2,
    projectile_damage = 12,
    projectile_knockback = 4,
    explosion_size = 180,
    explosion_damage = 0,
    cursor_weapon = 6
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11
  },
}

-- Milestone Boss: movement and hit windows are server-authoritative.
define {
  type = 20,
  name = "bulkheadcolossus",
  combat = {
    fire_rate = 250,
    projectile_speed = 850,
    projectile_life = 2.2,
    projectile_damage = 12,
    projectile_knockback = 4,
    explosion_size = 180,
    explosion_damage = 0,
    cursor_weapon = 6
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11
  },
}

-- Milestone Boss: movement and hit windows are server-authoritative.
define {
  type = 21,
  name = "arcconductor",
  combat = {
    fire_rate = 250,
    projectile_speed = 850,
    projectile_life = 2.2,
    projectile_damage = 12,
    projectile_knockback = 4,
    explosion_size = 180,
    explosion_damage = 0,
    cursor_weapon = 6
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11
  },
}

-- Milestone Boss: movement and hit windows are server-authoritative.
define {
  type = 22,
  name = "vaultoverseer",
  combat = {
    fire_rate = 250,
    projectile_speed = 850,
    projectile_life = 2.2,
    projectile_damage = 12,
    projectile_knockback = 4,
    explosion_size = 180,
    explosion_damage = 0,
    cursor_weapon = 6
  },
  visuals = {
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11
  },
}
