-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

weapon.define {
  id = "official:static:cluster",
  schema = 4,
  behavior = {"cluster", "explosion_smoke"},
  kind = weapon.kind.static,
  static_type = weapon.static.cluster,
  max_level = 2,

  combat_template = weapon.combat.projectile,
  visual_template = weapon.visual.weapon,

  combat = {
    fire_rate = 600,
    full_auto = true,
    max_ammo = weapon.curve.integer_linear(15, 15, 2),
    uses_ammo = true,
    projectile_spread = 0.04,
    projectile_speed = weapon.curve.integer_linear(1000, 0, 2),
    projectile_curvature = weapon.curve.integer_linear(7, 0, 2),
    projectile_life = weapon.curve.linear(2, 0, 2),
    explosion_size = 140,
    explosion_damage = 34,
    burst_count = weapon.curve.integer_linear(1, 3, 2),
    burst_reload = weapon.curve.linear(0.9, -0.6, 2),
    ai_attack_range = 700,
    valid_for_turret = true,
    explosive_projectile = true,
    cursor_weapon = weapon.curve.cycle {1, 7, 3, 9, 5},
    cost = weapon.curve.cost(10, 20, 2),
    projectile_bounces = weapon.curve.integer_linear(0, 0, 2),
    auto_pick = true,
  },
  visuals = {
    visual_size = {6, 3},
    render_offset_x = 24,
    muzzle_offset = {64, -4},
    projectile_offset = {66, -12},
    hand_offset = {-16, 4},
    color_swap_x = weapon.curve.linear(0, 0.7, 2),
    color_swap_y = weapon.curve.linear(0, 0.5, 2),
    render_recoil = 17.25,
    projectile_size = weapon.curve.linear(1, 0, 2),
    projectile_sprite = 14,
    explosion_sprite = 279,
    explosion_sound = 11,
    fire_sound = 4,
    screenshake_amount = 0.136,
  },
}

weapon.define {
  id = "official:static:shuriken",
  schema = 4,
  behavior = {"shuriken"},
  kind = weapon.kind.static,
  static_type = weapon.static.shuriken,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    projectile_damage = 100,
    melee_hit_radius = 20,
    ai_attack_range = 700,
    throw_force = 1.4,
    cursor_weapon = weapon.curve.cycle {7, 3, 9, 5, 1},
    cost = weapon.curve.integer_linear(10, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 10,
    projectile_size = 2.5,
    projectile_trace_type = -4,
    trace_threshold = 20,
  },
}

weapon.define {
  id = "official:static:claw",
  schema = 4,
  behavior = {"claw"},
  kind = weapon.kind.static,
  static_type = weapon.static.claw,
  max_level = 0,

  combat_template = weapon.combat.melee,
  visual_template = weapon.visual.melee_small,

  combat = {
    fire_rate = 350,
    full_auto = true,
    projectile_damage = 20,
    melee_hit_radius = 40,
    ai_attack_range = 80,
    cursor_weapon = weapon.curve.cycle {3, 9, 5, 1, 7},
    cost = weapon.curve.integer_linear(10, 1350, 15),
  },
  visuals = {
    visual_size = {4, 2},
    render_offset = {-20, -4},
    projectile_offset = {36, -4},
  },
}

weapon.define {
  id = "official:static:bomb",
  schema = 4,
  behavior = {"bomb"},
  kind = weapon.kind.static,
  static_type = weapon.static.bomb,
  max_level = 0,

  combat_template = weapon.combat.activate,
  visual_template = weapon.visual.item,

  combat = {
    fire_rate = 350,
    full_auto = true,
    explosion_size = 400,
    explosion_damage = 240,
    throw_force = 0.4,
    explosive_projectile = true,
    cursor_weapon = weapon.curve.cycle {9, 5, 1, 7, 3},
    cost = weapon.curve.integer_linear(10, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {3, 4},
    render_offset_x = 8,
    explosion_sprite = 279,
    explosion_sound = 11,
    screenshake_amount = 0.96,
  },
}

weapon.define {
  id = "official:static:ball",
  schema = 4,
  behavior = {"ball"},
  kind = weapon.kind.static,
  static_type = weapon.static.ball,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    throw_force = 1.4,
    cursor_weapon = weapon.curve.cycle {5, 1, 7, 3, 9},
    cost = weapon.curve.integer_linear(10, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 16,
    projectile_size = 2,
    projectile_trace_type = 8,
  },
}
