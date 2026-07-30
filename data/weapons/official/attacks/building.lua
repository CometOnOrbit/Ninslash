-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

local native_define = attack_profile.define
local impact_effects = {
  stand = weapon.impact.sparks,
  barrel = weapon.impact.sprite,
  barrel2 = weapon.impact.sprite,
  barrel3 = weapon.impact.sprite,
  turret = weapon.impact.sprite,
  flametrap = weapon.impact.sprite,
  generator = weapon.impact.sprite_electric,
  powerbarrel = weapon.impact.sprite,
  powerbarrel2 = weapon.impact.sprite,
  reactor = weapon.impact.sprite,
  teslacoil = weapon.impact.sprite,
}
local function define(definition)
  definition.visuals.impact_effect = impact_effects[definition.name] or weapon.impact.none
  return native_define(definition)
end

define {
  schema = 4,
  kind = "building",
  type = 0,
  name = "none",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 1,
  name = "sawblade",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}


define {
  schema = 4,
  kind = "building",
  type = 2,
  name = "mine1",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 3,
  name = "mine2",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 4,
  name = "barrel",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 200,
    explosion_damage = 30,
    burst_reload = 1,
    explosive_projectile = true,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.12,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 5,
  name = "barrel2",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 200,
    explosion_damage = 30,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.12,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 6,
  name = "barrel3",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 200,
    explosion_damage = 30,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.12,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 7,
  name = "turret",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 80,
    explosion_damage = 20,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.08,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 8,
  name = "lazer",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 9,
  name = "powerupper",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 10,
  name = "base",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 11,
  name = "stand",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 12,
  name = "flametrap",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 150,
    explosion_damage = 40,
    burst_reload = 1,
    flame_amount = 1,
    explosive_projectile = true,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 13,
  name = "jumppad",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 14,
  name = "switch",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 15,
  name = "door1",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 16,
  name = "generator",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 150,
    explosion_damage = 40,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.16,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 17,
  name = "powerbarrel",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 300,
    explosion_damage = 60,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.24,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 18,
  name = "powerbarrel2",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 300,
    explosion_damage = 60,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.24,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 19,
  name = "lightningwall",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    electro_amount = 1,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 20,
  name = "lightningwall2",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 21,
  name = "reactor",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 240,
    explosion_damage = 120,
    burst_reload = 1,
    explosive_projectile = true,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.48,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 22,
  name = "reactor_destroyed",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 23,
  name = "teslacoil",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 240,
    explosion_damage = 60,
    burst_reload = 1,
    ai_attack_range = 800,
    electro_amount = 1,
    explosive_projectile = true,
    cursor_weapon = 6,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 24,
  name = "screen",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 25,
  name = "shop",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}

define {
  schema = 4,
  kind = "building",
  type = 26,
  name = "pve_shield_node",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_life = math.huge,
    explosion_size = 120,
    burst_reload = 1,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    projectile_trace_type = 1,
    explosion_sprite = 279,
  },
}
