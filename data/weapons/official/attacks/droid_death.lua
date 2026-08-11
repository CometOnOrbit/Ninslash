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
  type = 8,
  name = "siegebreakercrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 420,
    full_auto = true,
    projectile_damage = 14,
    projectile_knockback = 34,
    explosion_size = 280,
    explosion_damage = 80,
    melee_hit_radius = 56,
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
    screenshake_amount = 0.3,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
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
    explosion_size = 220,
    explosion_damage = 40,
    burst_reload = 1,
    electro_amount = 1,
    explosive_projectile = true,
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
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 10,
  name = "splitcrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 300,
    full_auto = true,
    projectile_damage = 5,
    projectile_knockback = 24,
    explosion_size = 120,
    explosion_damage = 18,
    melee_hit_radius = 40,
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
    screenshake_amount = 0.1,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 11,
  name = "kamikazestar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 1000,
    full_auto = true,
    explosion_size = 400,
    explosion_damage = 120,
    burst_reload = 1,
    explosive_projectile = true,
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
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.4,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 12,
  name = "railstar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 2500,
    full_auto = true,
    projectile_damage = 26,
    projectile_penetration = -1,
    explosion_size = 220,
    explosion_damage = 40,
    burst_reload = 1,
    explosive_projectile = true,
    laser_weapon = true,
    aimline = true,
    laser_range = 1200,
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
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 13,
  name = "mendercrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 400,
    full_auto = true,
    projectile_damage = 3,
    projectile_knockback = 16,
    explosion_size = 160,
    explosion_damage = 30,
    melee_hit_radius = 40,
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
    screenshake_amount = 0.12,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 14,
  name = "stalkercrawler",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 280,
    full_auto = true,
    projectile_damage = 12,
    projectile_knockback = 30,
    explosion_size = 160,
    explosion_damage = 30,
    melee_hit_radius = 40,
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
    screenshake_amount = 0.12,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
  type = 15,
  name = "teslastar",

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 3000,
    full_auto = true,
    projectile_damage = 9,
    explosion_size = 220,
    explosion_damage = 40,
    burst_reload = 1,
    electro_amount = 1,
    explosive_projectile = true,
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
    explosion_sprite = 279,
    explosion_sound = 11,
  },
}

attack_profile.define {
  schema = 4,
  kind = "droid_death",
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
    explosion_size = 200,
    explosion_damage = 45,
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
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.18,
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
