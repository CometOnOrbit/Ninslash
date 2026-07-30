-- This script runs in the combat sandbox. It has no access to
-- files, sockets, engine objects, or floating-point world coordinates.
weapon.on_fire("workshop:9000000001:plasma-carbine", function(ctx)
  local charge = ctx:state_get(0)
  ctx:state_set(0, (charge + 1) % 4)
  ctx:spawn_projectile {
    speed = 1400,
    life = 90,
    damage = 18 + charge * 2,
    radius = 7,
    bounces = 1,
  }
  ctx:visual(0)
end)
