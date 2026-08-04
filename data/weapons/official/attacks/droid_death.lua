-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

attack_profile.define {
  schema = 4,
  kind = "droid_death",
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
    explosion_size = 160,
    explosion_damage = 30,
    burst_reload = 1,
    electro_amount = 0.5,
    explosive_projectile = true,
    cursor_weapon = 4,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
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
    explosion_size = 220,
    explosion_damage = 40,
    burst_reload = 1,
    electro_amount = 1,
    explosive_projectile = true,
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
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 2,
  name = "crawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 6,
    projectile_knockback = 24,
    explosion_size = 160,
    explosion_damage = 30,
    melee_hit_radius = 40,
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
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.12,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 3,
  name = "bosscrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 10,
    projectile_knockback = 34,
    explosion_size = 320,
    explosion_damage = 60,
    melee_hit_radius = 60,
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
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.24,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 4,
  name = "fly",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
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

attack_profile.define {
  schema = 4,
  kind = "droid_death",
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
    explosion_size = 220,
    explosion_damage = 40,
    burst_reload = 1,
    electro_amount = 1,
    explosive_projectile = true,
    cursor_weapon = 4,
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
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
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
    explosion_size = 220,
    explosion_damage = 40,
    burst_reload = 1,
    electro_amount = 0.5,
    explosive_projectile = true,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 7,
  name = "bosssplitter",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 10,
    projectile_knockback = 34,
    explosion_size = 320,
    explosion_damage = 60,
    melee_hit_radius = 60,
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
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.24,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 8,
  name = "bulwark",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_knockback = 1,
    burst_reload = 1,
    cursor_weapon = 6,
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

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 9,
  name = "assembler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_knockback = 1,
    burst_reload = 1,
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

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 10,
  name = "saboteur",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_knockback = 1,
    burst_reload = 1,
    cursor_weapon = 4,
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

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 11,
  name = "railgunner",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    burst_reload = 1,
    cursor_weapon = 8,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 12,
  name = "siege_engine",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    burst_reload = 1,
    cursor_weapon = 2,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 13,
  name = "overseer_core",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_speed = 1400,
    projectile_life = 0.6,
    projectile_knockback = 1,
    burst_reload = 1,
    cursor_weapon = 6,
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

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 14,
  name = "luminous_predator",
  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,
  combat = {
    fire_rate = 260,
    full_auto = true,
    projectile_damage = 12,
    projectile_knockback = 30,
    melee_hit_radius = 48,
    burst_reload = 1,
    electro_amount = 0.8,
    explosion_size = 120,
    explosion_damage = 20,
    explosive_projectile = true,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 15,
  name = "reef_sentinel",
  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,
  combat = {
    fire_rate = 340,
    full_auto = true,
    projectile_speed = 900,
    projectile_life = 0.8,
    projectile_damage = 18,
    projectile_knockback = 3,
    burst_reload = 1,
    electro_amount = 0.6,
    explosion_size = 140,
    explosion_damage = 24,
    explosive_projectile = true,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_sprite = 7,
    projectile_trace_type = -3,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 16,
  name = "abyssal_heart",
  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,
  combat = {
    fire_rate = 240,
    full_auto = true,
    projectile_damage = 28,
    projectile_knockback = 40,
    melee_hit_radius = 86,
    burst_reload = 1,
    electro_amount = 1,
    explosion_size = 240,
    explosion_damage = 60,
    explosive_projectile = true,
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 2},
    render_recoil = 12,
    projectile_size = 0,
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}
