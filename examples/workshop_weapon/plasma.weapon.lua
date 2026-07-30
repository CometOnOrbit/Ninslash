weapon.define {
  id = "plasma-carbine",
  schema = 4,
  inherits = "official:static:gun1",
  max_level = 4,

  combat = {
    fire_rate = 180,
    max_ammo = 24,
    uses_ammo = true,
    projectile_damage = weapon.curve.linear(14, 8, 4),
    projectile_speed = 1200,
    projectile_life = 0.9,
    cost = weapon.curve.cost(30, 24, 4),
  },

  visuals = {
    visual_size = {4, 2},
    projectile_size = 1.2,
  },

  assets = {
    held_image = "resources/plasma_carbine.png",
    projectile_image = "resources/plasma_bolt.png",
    muzzle_image = "resources/plasma_muzzle.png",
    fire_sound = "resources/plasma_fire.wv",
  },

  -- These are source keys, not language slots. Any language can provide the
  -- same keys in localization/<core-language-file-name>.
  localization = {
    name = "Plasma Carbine",
    description = "A compact plasma weapon.",
  },
}

weapon.module {
  schema = 1,
  id = "plasma-frame",
  slot = weapon.module_slot.frame,
  tags = {"ranged", "energy"},
  localization = {name = "Plasma Frame"},
}

weapon.compose {
  schema = 1,
  id = "plasma-combination",
  frames = {"plasma-frame"},
  parts = {"official:module:part:barrel1", "tag:energy-part"},
  build = function(ctx)
    return {
      max_level = 6,
      behavior = {},
      combat_template = weapon.combat.projectile,
      visual_template = weapon.visual.compact_weapon,
      combat = {
        fire_rate = 180,
        max_ammo = 24,
        uses_ammo = true,
        projectile_damage = weapon.curve.linear(14, 8, 6),
        projectile_speed = 1200,
        projectile_life = 0.9,
        cost = weapon.curve.cost(30, 24, 6),
      },
      visuals = {visual_size = {4, 2}, projectile_size = 1.2},
      assets = {
        held_image = "resources/plasma_carbine.png",
        projectile_image = "resources/plasma_bolt.png",
        muzzle_image = "resources/plasma_muzzle.png",
        fire_sound = "resources/plasma_fire.wv",
      },
      localization = {name = "Plasma Combination", description = "A modular plasma weapon."},
    }
  end,
}

forge.recipe {
  schema = 1,
  id = "plasma-refit",
  priority = 100,
  targets = {"tag:ranged"},
  materials = {"plasma-combination"},
  localization = {name = "Plasma Refit"},
  resolve = function(ctx)
    if ctx.target.level < 2 then return nil end
    return {
      product = ctx.material.stable_id,
      level = ctx.target.level,
      cost = ctx.base_cost + ctx.level_cost * 2,
    }
  end,
}
