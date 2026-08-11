-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

local native_define = attack_profile.define
local area_effect = {
  star = true,
  bossstar = true,
  tempeststar = true,
  railstar = true,
  teslastar = true,
}
local function define(definition)
  definition.visuals.impact_effect = area_effect[definition.name] and
    weapon.impact.electric_area or weapon.impact.electric
  definition.combat.direct_melee = definition.name == "crawler" or
    definition.name == "bosscrawler" or definition.name == "bosssplitter"
    or definition.name == "siegebreakercrawler" or definition.name == "splitcrawler"
    or definition.name == "mendercrawler" or definition.name == "stalkercrawler"
    or definition.name == "cyclonecrawler"
  return native_define(definition)
end

define {
  schema = 4,
  kind = "droid",
  type = 0,
  name = "walker",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_damage = 6,
    projectile_knockback = 1,
    burst_reload = 1,
    electro_amount = 0.5,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_sprite = 7,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 8,
  name = "siegebreakercrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 420,
    full_auto = true,
    projectile_damage = 14,
    projectile_knockback = 34,
    melee_hit_radius = 56,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 9,
  name = "tempeststar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 180,
    full_auto = true,
    projectile_speed = 20,
    projectile_life = 1.2,
    projectile_damage = 4,
    projectile_knockback = 2,
    burst_reload = 1,
    electro_amount = 1,
    cursor_weapon = 2,
    cost = 10,
    projectile_pos_type = weapon.path.log,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 10,
  name = "splitcrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 5,
    projectile_knockback = 24,
    melee_hit_radius = 40,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 11,
  name = "kamikazestar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 1000,
    full_auto = true,
    projectile_damage = 0,
    burst_reload = 1,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 12,
  name = "railstar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 2500,
    full_auto = true,
    projectile_damage = 26,
    projectile_penetration = -1,
    laser_weapon = true,
    aimline = true,
    laser_range = 1200,
    burst_reload = 1,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 13,
  name = "mendercrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 400,
    full_auto = true,
    projectile_damage = 3,
    projectile_knockback = 16,
    melee_hit_radius = 40,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 14,
  name = "stalkercrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 280,
    full_auto = true,
    projectile_damage = 12,
    projectile_knockback = 30,
    melee_hit_radius = 40,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 15,
  name = "teslastar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 3000,
    full_auto = true,
    projectile_damage = 9,
    electro_amount = 1,
    burst_reload = 1,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 16,
  name = "cyclonecrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 600,
    full_auto = true,
    projectile_speed = 24,
    projectile_life = 1.2,
    projectile_damage = 8,
    projectile_knockback = 24,
    explosion_size = 100,
    explosion_damage = 8,
    melee_hit_radius = 44,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 1,
  name = "star",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 24,
    projectile_life = 1.2,
    projectile_damage = 10,
    projectile_knockback = 2,
    burst_reload = 1,
    electro_amount = 1,
    cursor_weapon = 2,
    cost = 10,
    projectile_pos_type = weapon.path.log,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}


define {
  schema = 4,
  kind = "droid",
  type = 2,
  name = "crawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 6,
    projectile_knockback = 24,
    melee_hit_radius = 40,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 3,
  name = "bosscrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 10,
    projectile_knockback = 34,
    melee_hit_radius = 60,
    burst_reload = 1,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 4,
  name = "fly",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    burst_reload = 1,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 5,
  name = "bossstar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 24,
    projectile_life = 1.2,
    projectile_damage = 10,
    projectile_knockback = 2,
    burst_reload = 1,
    electro_amount = 1,
    cursor_weapon = 8,
    cost = 10,
    projectile_pos_type = weapon.path.log,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 2,
    projectile_sprite = 4,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 6,
  name = "bosswalker",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_damage = 10,
    projectile_knockback = 2,
    burst_reload = 1,
    electro_amount = 0.5,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_sprite = 7,
    projectile_trace_type = -3,
  },
}

define {
  schema = 4,
  kind = "droid",
  type = 7,
  name = "bosssplitter",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 10,
    projectile_knockback = 34,
    melee_hit_radius = 60,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}
