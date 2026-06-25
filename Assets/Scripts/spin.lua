-- spin.lua — demo: spin around Y and bob up/down.
-- Attach a Script component to an object and point it at this file.
-- Per-instance locals persist between OnStart/OnUpdate (closure upvalues).

local t = 0.0
local base = { 0.0, 0.0, 0.0 }

function OnStart()
    base = { self:getPos() }                       -- remember start position
    log("spin.lua started on '" .. self:getName() .. "'")
end

function OnUpdate(dt)
    t = t + dt
    self:setRotation(0.0, t * 90.0, 0.0)            -- 90 deg/sec around Y
    self:setPos(base[1], base[2] + math.sin(t * 2.0) * 0.5, base[3])  -- bob
end

function OnDestroy()
    log("spin.lua stopped")
end
