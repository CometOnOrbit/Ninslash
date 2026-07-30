-- Official weapon data. Loaded in manifest order and compiled into immutable profiles.

weapon.define {
  id = "official:static:upgrade",
  schema = 4,
  behavior = {"upgrade"},
  kind = weapon.kind.static,
  static_type = weapon.static.upgrade,
  max_level = 2,

  combat_template = weapon.combat.activate,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    cursor_weapon = weapon.curve.cycle {9, 5, 1, 7, 3},
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 2},
    render_offset_x = 9,
  },
}

weapon.define {
  id = "official:static:shield",
  schema = 4,
  behavior = {"controller_activate", "activate_shield"},
  kind = weapon.kind.static,
  static_type = weapon.static.shield,
  max_level = 0,

  combat_template = weapon.combat.activate,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    ai_attack_range = 800,
    cursor_weapon = weapon.curve.cycle {5, 1, 7, 3, 9},
    cost = weapon.curve.integer_linear(5, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 3},
    render_offset = {4, -3},
  },
}

weapon.define {
  id = "official:static:respawner",
  schema = 4,
  behavior = {"controller_activate", "activate_respawner"},
  kind = weapon.kind.static,
  static_type = weapon.static.respawner,
  max_level = 0,

  combat_template = weapon.combat.activate,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    ai_attack_range = 800,
    cursor_weapon = weapon.curve.cycle {1, 7, 3, 9, 5},
    cost = 10,
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 4},
    render_offset = {2, -5},
  },
}

weapon.define {
  id = "official:static:mask1",
  schema = 4,
  behavior = {},
  kind = weapon.kind.static,
  static_type = weapon.static.mask1,
  max_level = 0,

  combat_template = weapon.combat.none,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    cursor_weapon = weapon.curve.cycle {7, 3, 9, 5, 1},
    cost = weapon.curve.integer_linear(10, 1350, 15),
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 16,
  },
}

weapon.define {
  id = "official:static:mask2",
  schema = 4,
  behavior = {},
  kind = weapon.kind.static,
  static_type = weapon.static.mask2,
  max_level = 0,

  combat_template = weapon.combat.none,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    cursor_weapon = weapon.curve.cycle {3, 9, 5, 1, 7},
    cost = weapon.curve.integer_linear(10, 1350, 15),
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 16,
  },
}

weapon.define {
  id = "official:static:mask3",
  schema = 4,
  behavior = {},
  kind = weapon.kind.static,
  static_type = weapon.static.mask3,
  max_level = 0,

  combat_template = weapon.combat.none,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    cursor_weapon = weapon.curve.cycle {9, 5, 1, 7, 3},
    cost = weapon.curve.integer_linear(10, 1350, 15),
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 16,
  },
}

weapon.define {
  id = "official:static:mask4",
  schema = 4,
  behavior = {},
  kind = weapon.kind.static,
  static_type = weapon.static.mask4,
  max_level = 0,

  combat_template = weapon.combat.none,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    cursor_weapon = weapon.curve.cycle {5, 1, 7, 3, 9},
    cost = weapon.curve.integer_linear(10, 1350, 15),
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 16,
  },
}

weapon.define {
  id = "official:static:mask5",
  schema = 4,
  behavior = {},
  kind = weapon.kind.static,
  static_type = weapon.static.mask5,
  max_level = 0,

  combat_template = weapon.combat.none,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    cursor_weapon = weapon.curve.cycle {1, 7, 3, 9, 5},
    cost = weapon.curve.integer_linear(10, 1350, 15),
  },
  visuals = {
    visual_size = {4, 4},
    render_offset_x = 16,
  },
}

weapon.define {
  id = "official:static:invis",
  schema = 4,
  behavior = {"controller_activate", "activate_invis"},
  kind = weapon.kind.static,
  static_type = weapon.static.invis,
  max_level = 0,

  combat_template = weapon.combat.activate,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    ai_attack_range = 800,
    cursor_weapon = weapon.curve.cycle {7, 3, 9, 5, 1},
    cost = weapon.curve.integer_linear(5, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 3},
    render_offset = {4, -3},
  },
}

weapon.define {
  id = "official:static:electrowall",
  schema = 4,
  behavior = {"electrowall"},
  kind = weapon.kind.static,
  static_type = weapon.static.electrowall,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    ai_attack_range = 800,
    throw_force = 0.85,
    cursor_weapon = weapon.curve.cycle {3, 9, 5, 1, 7},
    cost = weapon.curve.integer_linear(7, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 3},
    render_offset = {4, -3},
    projectile_size = 2.5,
    projectile_trace_type = 4,
  },
}

weapon.define {
  id = "official:static:areashield",
  schema = 4,
  behavior = {"area_shield"},
  kind = weapon.kind.static,
  static_type = weapon.static.areashield,
  max_level = 0,

  combat_template = weapon.combat.throw,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    ai_attack_range = 800,
    throw_force = 0.75,
    cursor_weapon = weapon.curve.cycle {9, 5, 1, 7, 3},
    cost = weapon.curve.integer_linear(7, 1350, 15),
    auto_pick = true,
  },
  visuals = {
    visual_size = {3, 2},
    render_offset = {16, -3},
    projectile_size = 2.5,
    projectile_trace_type = 5,
  },
}

weapon.define {
  id = "official:static:syringe",
  schema = 4,
  behavior = {},
  kind = weapon.kind.static,
  static_type = weapon.static.syringe,
  max_level = 0,

  combat_template = weapon.combat.activate,
  visual_template = weapon.visual.item,

  combat = {
    full_auto = true,
    ai_attack_range = 800,
    cursor_weapon = weapon.curve.cycle {5, 1, 7, 3, 9},
    cost = 5,
    auto_pick = true,
  },
  visuals = {
    visual_size = {2, 4},
    render_offset = {2, -5},
  },
}
