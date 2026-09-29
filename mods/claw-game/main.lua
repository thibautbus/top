-- The Claw Game: Link's Awakening's crane game (the Trendy Game), in a house
-- added to Lynna City (Ages) and Horon Village (Seasons). The port builds the house into the game before it
-- starts; Link talks to the keeper by facing the counter and pressing A.

mod.description("A house in Lynna City and in Horon, and a claw game")

local claw = require("claw")

-- The prizes on the belt, and what the game gives for each.
local function prizes()
  local list = {
    { sprite = "rupee", name = "20 Rupees", give = { "rupees", 20 } },
    { sprite = "heart", name = "a Heart", give = { "heart" } },
    { sprite = "seeds", name = "10 Ember Seeds", give = { "seeds", 10 } },
    { sprite = "rupee", name = "20 Rupees", give = { "rupees", 20 } },
    { sprite = "gasha_seed", name = "a Gasha Seed", give = { "gasha_seed" } },
    { sprite = "ring", name = "a Power Ring", give = { "ring", "power_ring_l1" }, still = true },
  }
  -- No satchel, no seeds; no ring box, no ring: Rupees instead.
  if not game.has("seed_satchel") then list[3] = { sprite = "rupee", name = "50 Rupees", give = { "rupees", 50 } } end
  if not game.has("ring_box") then list[6] = { sprite = "rupee", name = "100 Rupees", give = { "rupees", 100 }, still = true } end
  return list
end

-- Ages: on the big island in the south of Lynna City, bottom right, by the bridge.
-- Seasons: in Horon Village, east of the fountain, on the right of the loop of the path.
mod.house("claw_game", {
  ages = { room = "0/55", col = 7, row = 4 },
  seasons = { room = "0/e9", col = 4, row = 2 },
})

mod.npc("keeper", { house = "claw_game" }, function(talk)
  talk:say("Welcome to the Claw Game!")
  talk:say("One try is 10 Rupees.")
  if talk:ask("Want to play?", { "Yes", "No" }) == "No" then
    return talk:say("Come back anytime!")
  end
  if not game.pay(10) then
    return talk:say("You don't have enough Rupees.")
  end

  local prize = talk:play(claw, { prizes = prizes() })
  if not prize then
    return talk:say("Too bad! Try again sometime.")
  end
  game.give(table.unpack(prize.give))
  talk:say("Congratulations! You won " .. prize.name .. "!")
end)
