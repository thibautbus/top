-- The claw machine, the scene the keeper starts with talk:play.
--
-- Hold A: the claw moves right; release it, the claw stops.
-- Hold B: the claw goes down; it closes as soon as it touches a prize, when
-- it reaches the belt, or when B is released. The belt keeps moving: aim ahead.
-- It rises with what it holds, carries it to the chute and drops it.
-- The prize in the middle does not move, like Link's Awakening's Yoshi doll,
-- but it slips from the claw one time in SLIP_CHANCE.

local sizes = require("sprites")

local LEFT, RIGHT = 6, 138           -- how far the claw travels (its left edge)
local TOP = 24                       -- the claw's resting height (its top edge)
local BELT = 104                     -- the top of the belt
local BELT_SPEED = 0.5               -- pixels per frame
local LOOP_WIDTH = 176               -- prizes go round, passing under the frame
local SLIP_CHANCE = 3                -- the still prize slips one time in SLIP_CHANCE

local claw = {}

local HELP = {
  move = "A: move",
  lower = "B: lower",
  close = "...",
  rise = "...",
  carry = "...",
  drop = "...",
  done = "",
}

-- Where a prize is this frame: on the moving belt, except the still one on its stand.
local function position(self, prize)
  local size = sizes[prize.sprite]
  if prize.still then return prize.x, BELT - 12 - size.height end
  return (prize.x + self.belt) % LOOP_WIDTH - 8, BELT - size.height
end

function claw:start(ctx)
  self.random = ctx.random
  self.x, self.y = LEFT, TOP
  self.phase = "move"
  self.ready = false                 -- A must be released first: it may have answered "Yes"
  self.belt = 0
  self.wait = 0
  self.held = nil
  self.prizes = {}
  for i, prize in ipairs(ctx.params.prizes) do
    self.prizes[i] = { prize = prize, sprite = prize.sprite, still = prize.still, x = prize.still and 76 or (i - 1) * 36 }
  end
end

-- The prize the claw touches on its way down: between its prongs, and high
-- enough for the claw's bottom to bump into it.
function claw:touched()
  local centre, bottom = self.x + 8, self.y + 12
  for _, prize in ipairs(self.prizes) do
    if not prize.taken then
      local x, y = position(self, prize)
      local width = sizes[prize.sprite].width
      if math.abs(x + width / 2 - centre) <= width / 2 + 2 and bottom >= y then return prize end
    end
  end
end

function claw:update(input)
  self.belt = self.belt + BELT_SPEED
  if not input.held.a then self.ready = true end

  if self.phase == "move" then
    if self.ready and input.held.a then
      self.x = math.min(self.x + 1, RIGHT)
      self.moved = true
    end
    if self.moved and (input.released.a or self.x == RIGHT) then self.phase = "lower" end

  elseif self.phase == "lower" then
    if input.held.b then
      self.y = self.y + 1
      self.lowered = true
    end
    -- It closes on what it touches, which leaves the belt.
    local prize = self:touched()
    if prize or self.y >= BELT - 12 or (self.lowered and input.released.b) then
      self.held = prize
      if prize then prize.taken = true end
      self.phase, self.wait = "close", 20
    end

  elseif self.phase == "close" then
    self.wait = self.wait - 1
    if self.wait == 0 then self.phase = "rise" end

  elseif self.phase == "rise" then
    self.y = self.y - 1
    -- The still prize slips one time in SLIP_CHANCE, halfway up, back onto its stand.
    if self.held and self.held.still and self.y == 60 and self.random(SLIP_CHANCE) == 1 then
      self.held.taken = false
      self.held = nil
    end
    if self.y <= TOP then self.phase = "carry" end

  elseif self.phase == "carry" then
    self.x = self.x - 1
    if self.x <= LEFT then self.phase, self.fall = "drop", 0 end

  elseif self.phase == "drop" then
    self.fall = self.fall + 2
    if self.fall > 40 then self.phase, self.wait = "done", 30 end

  elseif self.phase == "done" then
    self.wait = self.wait - 1
    if self.wait == 0 then return self.held and self.held.prize or false end
  end
end

function claw:draw(g)
  g:clear("#201830")
  -- The machine: its frame, the glass, the rail at the top, the chute.
  g:rect(2, 16, 156, 112, "#584078")
  g:rect(4, 18, 152, 104, "#182838")
  g:rect(4, 18, 152, 4, "#686888")
  g:rect(4, BELT - 2, 22, 22, "#080810")
  -- The belt, running.
  g:rect(26, BELT, 130, 10, "#404048")
  local step = math.floor(self.belt) % 8
  for x = 26 - 8 + step, 150, 8 do
    if x >= 26 then g:rect(x, BELT + 2, 4, 6, "#707078") end
  end
  -- The still prize's stand.
  g:rect(72, BELT - 12, 18, 12, "#806040")
  -- The prizes.
  for _, prize in ipairs(self.prizes) do
    if not prize.taken then
      local x, y = position(self, prize)
      if x >= 26 or prize.still then g:sprite(prize.sprite, x, y) end
    end
  end
  -- The cable, the claw, and what it holds.
  g:rect(self.x + 7, 22, 2, self.y - 22, "#a0a0b0")
  local closed = self.phase == "close" or self.phase == "rise" or self.phase == "carry"
  g:sprite(closed and "claw_closed" or "claw_open", self.x, self.y)
  if self.held then
    local width = sizes[self.held.sprite].width
    local y = self.y + 8 + (self.phase == "drop" and self.fall or 0)
    if y < BELT + 16 then g:sprite(self.held.sprite, self.x + 8 - math.floor(width / 2), y) end
  end
  -- The help, at the top.
  g:text(HELP[self.phase], 8, 0)
end

return claw
