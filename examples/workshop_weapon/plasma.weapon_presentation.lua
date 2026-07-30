-- Presentation scripts are client-only and read-only. They cannot spawn
-- entities, damage targets, send messages, or access files/Steam APIs.
presentation.on_hud("workshop:9000000001:plasma-carbine", function(ctx)
  local charge = ctx:state(0)
  ctx:text("Plasma charge", 22, 150, 12)
  ctx:bar(charge, 3, 22, 166, 120, 6)
end)
