-- A second mod with a house, to exercise several mods loaded together, each
-- with its house, and mod.storage: a fortune teller on the island east of the red
-- house of Lynna City, loaded with the claw game (mods/claw-game), who
-- remembers how many times Link came.

mod.description("A fortune teller who remembers each of Link’s visits")

local FORTUNES = {
  "A claw game waits by the southern bridge.",
  "Your next Gasha Seed will grow well.",
  "Someone in town is thinking of you.",
  "Rupees come to those who wait.",
}

mod.house("fortune", { ages = { room = "0/46", col = 6, row = 3 } })

mod.npc("teller", { house = "fortune" }, function(talk)
  -- mod.storage is saved with the game's file: the count survives a save and a reload.
  local visits = (mod.storage.visits or 0) + 1
  mod.storage.visits = visits
  if visits == 1 then
    talk:say("I read the future.")
  else
    talk:say("Welcome back! Visit " .. visits .. ".")
  end
  if talk:ask("Your fortune for 5 Rupees?", { "Yes", "No" }) == "No" then
    return talk:say("The future can wait.")
  end
  if not game.pay(5) then
    return talk:say("Your purse is empty.")
  end
  talk:say(FORTUNES[talk:random(#FORTUNES)])
end)
