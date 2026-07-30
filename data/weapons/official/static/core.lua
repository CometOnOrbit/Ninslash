-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

weapon.define {
  id = "official:static:tool",
  schema = 4,
  behavior = {"tool"},
  kind = weapon.kind.static,
  static_type = weapon.static.tool,
  max_level = 0,

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 320,
    full_auto = true,
    projectile_damage = 10,
    melee_hit_radius = 30,
    ai_attack_range = 50,
    cursor_weapon = weapon.curve.cycle {9, 5, 1, 7, 3},
    cost = 5,
  },
  visuals = {
    visual_size = {4, 2},
    render_offset = {-20, -4},
    projectile_offset = {26, -4},
  },
}

weapon.define {
  id = "official:static:gun1",
  schema = 4,
  behavior = {"compact_gun_hands", "impact_spark"},
  kind = weapon.kind.static,
  static_type = weapon.static.gun1,
  max_level = 0,

  combat_template = weapon.combat.projectile,
  visual_template = weapon.visual.compact_weapon,

  combat = {
    fire_rate = 240,
    projectile_spread = 0.06,
    projectile_speed = 1200,
    projectile_curvature = 2.2,
    projectile_life = 0.6,
    projectile_damage = 15,
    explosion_size = 80,
    ai_attack_range = 700,
    cursor_weapon = weapon.curve.cycle {5, 1, 7, 3, 9},
    cost = 2,
  },
  visuals = {
    visual_size = {4, 2},
    render_offset = {16, -8},
    muzzle_offset = {20, -5},
    projectile_offset = {36, -16},
    render_recoil = 14.95,
    projectile_size = 0.7,
    projectile_trace_type = 4,
    fire_sound = 0,
  },
}

weapon.define {
  id = "official:static:gun2",
  schema = 4,
  behavior = {"compact_gun_hands", "electric_gun"},
  kind = weapon.kind.static,
  static_type = weapon.static.gun2,
  max_level = 0,

  combat_template = weapon.combat.charge,
  visual_template = weapon.visual.compact_weapon,

  combat = {
    fire_rate = 200,
    full_auto = true,
    projectile_damage = 35,
    ai_attack_range = 500,
    electro_amount = 0.5,
    cursor_weapon = weapon.curve.cycle {1, 7, 3, 9, 5},
    cost = 2,
  },
  visuals = {
    visual_size = {4, 2},
    render_offset = {16, -8},
    projectile_offset = {36, -16},
    render_recoil = 13.65,
    fire_sound = 13,
    muzzle_type = 1,
  },
}

weapon.define {
  id = "official:static:grenade1",
  schema = 4,
  behavior = {"grenade_timed", "explosion_smoke"},
  kind = weapon.kind.static,
  static_type = weapon.static.grenade1,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    explosion_size = 300,
    explosion_damage = 120,
    ai_attack_range = 700,
    throw_force = 1,
    explosive_projectile = true,
    cursor_weapon = weapon.curve.cycle {7, 3, 9, 5, 1},
    cost = 5,
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 3},
    render_offset_x = 4,
    projectile_size = 2.5,
    projectile_trace_type = 4,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.48,
  },
}

weapon.define {
  id = "official:static:grenade2",
  schema = 4,
  behavior = {"grenade_laser"},
  kind = weapon.kind.static,
  static_type = weapon.static.grenade2,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    explosion_size = 320,
    explosion_damage = 30,
    ai_attack_range = 700,
    throw_force = 1,
    electro_amount = 0.5,
    explosive_projectile = true,
    cursor_weapon = weapon.curve.cycle {3, 9, 5, 1, 7},
    cost = 5,
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 3},
    render_offset_x = 4,
    projectile_size = 2.5,
    projectile_trace_type = 5,
    explosion_sprite = 279,
    explosion_sound = 86,
  },
}

weapon.define {
  id = "official:static:grenade3",
  schema = 4,
  behavior = {"grenade_drop"},
  kind = weapon.kind.static,
  static_type = weapon.static.grenade3,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    explosion_size = 140,
    ai_attack_range = 700,
    throw_force = 1,
    cursor_weapon = weapon.curve.cycle {9, 5, 1, 7, 3},
    cost = 5,
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 3},
    render_offset_x = 4,
    projectile_size = 2.5,
    projectile_trace_type = 7,
    explosion_sound = 117,
  },
}

weapon.define {
  id = "official:static:bazooka",
  schema = 4,
  behavior = {"bazooka"},
  kind = weapon.kind.static,
  static_type = weapon.static.bazooka,
  max_level = 2,

  combat_template = weapon.combat.projectile,
  visual_template = weapon.visual.weapon,

  combat = {
    fire_rate = 640,
    full_auto = true,
    max_ammo = weapon.curve.integer_linear(6, 9, 2),
    uses_ammo = true,
    projectile_speed = 400,
    projectile_life = 0.8,
    projectile_damage = 10,
    explosion_size = 240,
    explosion_damage = 80,
    burst_count = weapon.curve.integer_linear(1, 2, 2),
    burst_reload = weapon.curve.linear(0.95, -0.75, 2),
    ai_attack_range = 700,
    valid_for_turret = true,
    explosive_projectile = true,
    cursor_weapon = weapon.curve.cycle {5, 1, 7, 3, 9},
    cost = weapon.curve.cost(20, 20, 2),
    projectile_pos_type = weapon.path.rocket,
    auto_pick = true,
  },
  visuals = {
    visual_size = {6, 3},
    render_offset_x = 30,
    muzzle_offset_x = 60,
    projectile_offset = {65, -8},
    color_swap_x = weapon.curve.linear(0, 1, 2),
    color_swap_y = weapon.curve.linear(0, 0.4, 2),
    render_recoil = 20,
    projectile_size = 1.3,
    projectile_sprite = 12,
    projectile_trace_type = -1,
    explosion_sprite = 279,
    explosion_sound = 11,
    fire_sound = 114,
    screenshake_amount = 0.32,
  },
}

weapon.define {
  id = "official:static:bouncer",
  schema = 4,
  behavior = {"green_explosion"},
  kind = weapon.kind.static,
  static_type = weapon.static.bouncer,
  max_level = 2,

  combat_template = weapon.combat.projectile,
  visual_template = weapon.visual.weapon,

  combat = {
    fire_rate = 240,
    full_auto = true,
    max_ammo = weapon.curve.integer_linear(20, 20, 2),
    uses_ammo = true,
    shot_spread = weapon.curve.integer_linear(1, 1, 2),
    projectile_speed = 1500,
    projectile_curvature = 0.5,
    projectile_life = 0.8,
    projectile_damage = weapon.curve.linear(0, 3, 2),
    explosion_size = weapon.curve.linear(140, 15, 2),
    explosion_damage = weapon.curve.integer_linear(24, 4, 2),
    ai_attack_range = 700,
    valid_for_turret = true,
    explosive_projectile = true,
    cursor_weapon = weapon.curve.cycle {1, 7, 3, 9, 5},
    cost = weapon.curve.cost(10, 20, 2),
    projectile_bounces = 9,
    auto_pick = true,
  },
  visuals = {
    visual_size = {6, 3},
    render_offset_x = 30,
    muzzle_offset_x = 62,
    projectile_offset = {65, -11},
    color_swap_x = weapon.curve.linear(0, 0.9, 2),
    color_swap_y = weapon.curve.linear(0, 0.4, 2),
    render_recoil = 14.95,
    projectile_size = weapon.curve.linear(1, 0.1, 2),
    projectile_sprite = 13,
    projectile_trace_type = 6,
    explosion_sound = 117,
    fire_sound = 115,
    muzzle_type = 2,
    screenshake_amount = weapon.curve.linear(0.096, 0.016, 2),
  },
}

weapon.define {
  id = "official:static:chainsaw",
  schema = 4,
  behavior = {"chainsaw"},
  kind = weapon.kind.static,
  static_type = weapon.static.chainsaw,
  max_level = 2,

  combat_template = weapon.combat.hold,
  visual_template = weapon.visual.weapon,

  combat = {
    fire_rate = 500,
    full_auto = true,
    max_ammo = 15,
    uses_ammo = weapon.curve.cycle {true, true, false, false},
    projectile_damage = weapon.curve.integer_linear(6, 4, 2),
    melee_hit_radius = weapon.curve.integer_linear(14, 10, 2),
    ai_attack_range = 150,
    cursor_weapon = weapon.curve.cycle {7, 3, 9, 5, 1},
    cost = weapon.curve.cost(10, 20, 2),
    auto_pick = true,
  },
  visuals = {
    visual_size = {7, 3},
    render_offset_x = 30,
    projectile_offset = {53, -11},
    color_swap_x = weapon.curve.linear(0, 0.25, 2),
    color_swap_y = weapon.curve.linear(0, 0.9, 2),
    render_recoil = 2.2,
    fire_sound = 10,
  },
}

weapon.define {
  id = "official:static:flamer",
  schema = 4,
  behavior = {"flamer"},
  kind = weapon.kind.static,
  static_type = weapon.static.flamer,
  max_level = 2,

  combat_template = weapon.combat.hold,
  visual_template = weapon.visual.weapon,

  combat = {
    fire_rate = 200,
    full_auto = true,
    max_ammo = weapon.curve.integer_linear(25, 15, 2),
    uses_ammo = true,
    projectile_damage = weapon.curve.linear(2, 3, 2),
    melee_hit_radius = 24,
    ai_attack_range = 600,
    flame_amount = 1,
    cursor_weapon = weapon.curve.cycle {3, 9, 5, 1, 7},
    cost = weapon.curve.cost(10, 20, 2),
    auto_pick = true,
  },
  visuals = {
    visual_size = {6, 3},
    render_offset_x = 30,
    projectile_offset = {86, -11},
    color_swap_x = weapon.curve.linear(0, 0.6, 2),
    color_swap_y = weapon.curve.linear(0, 0.8, 2),
    fire_sound = 67,
  },
}
