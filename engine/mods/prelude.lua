-- The engine's half of a mod's runtime (engine/mods): what a mod's
-- Lua calls, written in Lua over the host's primitives.  It runs first, in the
-- mod's own Lua state, and receives the primitives as its argument; the mod's
-- files see only the environment built here, never the primitives.
--
-- A conversation is a coroutine the host resumes once a frame, with the
-- player's keys of that frame; each resume draws that frame's overlay and
-- ends by yielding.  talk:say, talk:ask and talk:play are loops of such
-- frames.  The game does not run the conversation: its keys are held while
-- the coroutine lives (mod.c).

local host = ...
local yield = coroutine.yield

-- A primitive of the host, called so that its error names the mod's file and line, not this prelude's: `call` is
-- always called by the API function the mod called (level 1 `call`, 2 that function, 3 the mod).
local pcall, unpack, pack = pcall, table.unpack, table.pack
local function call(primitive, ...)
  local results = pack(pcall(primitive, ...))
  if not results[1] then error(results[2], 3) end
  return unpack(results, 2, results.n)
end

-- ---- determinism: the functions whose result would depend on the run -------

-- A table's keys in a fixed order: numbers, then strings, then false and true.
-- Lua walks a table in the order of its hash part, which depends on addresses
-- and on the string seed; a mod never sees that order.
local function key_rank(key)
  local kind = type(key)
  if kind == "number" then return 1 elseif kind == "string" then return 2 elseif kind == "boolean" then return 3 end
  error("pairs: a table keyed by a " .. kind .. " has no fixed order; key it by numbers or strings", 4)
end

local function key_less(a, b)
  local ra, rb = key_rank(a), key_rank(b)
  if ra ~= rb then return ra < rb end
  if ra == 3 then return (not a) and b end
  return a < b
end

local raw_next = next
local function sorted_keys(t)
  local keys, key = {}, raw_next(t)
  while key ~= nil do
    key_rank(key)   -- every key, not only those the sort compares: a table of one table key is refused too
    keys[#keys + 1] = key
    key = raw_next(t, key)
  end
  table.sort(keys, key_less)
  return keys
end

local function sorted_pairs(t)
  local keys, i = sorted_keys(t), 0
  return function()
    i = i + 1
    local key = keys[i]
    if key ~= nil then return key, t[key] end
  end, t, nil
end

local function sorted_next(t, key)
  local keys = sorted_keys(t)
  if key == nil then
    local first = keys[1]
    if first ~= nil then return first, t[first] end
    return nil
  end
  for i = 1, #keys do
    if rawequal(keys[i], key) then
      local following = keys[i + 1]
      if following ~= nil then return following, t[following] end
      return nil
    end
  end
  error("next: the key is not in the table", 2)
end

-- tostring of a table or a function shows its address: here, its type alone.
local raw_tostring = tostring
local function safe_tostring(value)
  local kind = type(value)
  if kind == "table" then
    local meta = getmetatable(value)
    if meta and meta.__tostring then return raw_tostring(value) end
    return "table"
  elseif kind == "function" or kind == "thread" or kind == "userdata" then
    return kind
  end
  return raw_tostring(value)
end

-- Finalizers and weak tables run when the collector decides, which a mod cannot rely on: refused when the metatable
-- is set, and when they are added to it later (a guard on the metatable itself, which Lua then keeps from being
-- replaced; rawset is not given to mods).  A metatable that has a metatable of its own (Derived of Base, in the
-- idiom of inheritance) gets the guard's writing through that one, when the mod set it; one whose metatable is
-- hidden (__metatable) cannot be a metatable.
local raw_setmetatable, rawset = setmetatable, rawset
local GC_MESSAGE = "__gc and __mode are not allowed in a mod (the collector's timing is not a mod's to rely on)"
local GUARD = {
  __newindex = function(t, key, value)
    if key == "__gc" or key == "__mode" then error(GC_MESSAGE, 2) end
    rawset(t, key, value)
  end,
  __metatable = false,
}
local guarded = {}   -- the metatables the guard is on (a strong table, never walked)
local function safe_setmetatable(t, meta)
  if meta ~= nil then
    if type(meta) ~= "table" then error("setmetatable: a metatable is a table", 2) end
    if rawget(meta, "__gc") ~= nil or rawget(meta, "__mode") ~= nil then error("setmetatable: " .. GC_MESSAGE, 2) end
    if not guarded[meta] then
      local own = getmetatable(meta)
      if own == nil then
        raw_setmetatable(meta, GUARD)
      elseif type(own) == "table" and guarded[own] then
        -- Writing into meta goes through own's __newindex: the guard's, unless the mod has one (which cannot write
        -- into meta without rawset).
        if rawget(own, "__newindex") == nil then rawset(own, "__newindex", GUARD.__newindex) end
      else
        error("setmetatable: a table whose metatable is hidden cannot be a metatable", 2)
      end
      guarded[meta] = true
    end
  end
  return raw_setmetatable(t, meta)
end

local function copy(source, without)
  local out = {}
  for key, value in sorted_pairs(source) do
    if not (without and without[key]) then out[key] = value end
  end
  return out
end

local safe_string = copy(string, { dump = true })
local raw_format, raw_find, raw_sub = string.format, string.find, string.sub
safe_string.format = function(pattern, ...)
  if type(pattern) == "string" and raw_find(pattern, "%%p") then
    error("string.format: %p shows an address, which is not the same from run to run", 2)
  end
  local args = pack(...)
  for i = 1, args.n do   -- %s of a table or a function would show its address
    local kind = type(args[i])
    if kind == "table" or kind == "function" or kind == "thread" or kind == "userdata" then args[i] = safe_tostring(args[i]) end
  end
  return (call(raw_format, pattern, unpack(args, 1, args.n)))
end

-- The functions whose work in C the instruction budget does not see are charged for it.  A pattern search can take
-- a time that grows as the subject's length to the power of its quantifiers: a pattern whose cost would exceed ten
-- million steps is refused.
local charge = host.charge
local function quantifiers(pattern)
  local n, i = 0, 1
  while i <= #pattern do
    local c = raw_sub(pattern, i, i)
    if c == "%" then
      i = i + 2
    elseif c == "[" then
      i = i + 1
      if raw_sub(pattern, i, i) == "^" then i = i + 1 end
      if raw_sub(pattern, i, i) == "]" then i = i + 1 end
      while i <= #pattern and raw_sub(pattern, i, i) ~= "]" do i = i + (raw_sub(pattern, i, i) == "%" and 2 or 1) end
      i = i + 1
    else
      if c == "*" or c == "+" or c == "-" or c == "?" then n = n + 1 end
      i = i + 1
    end
  end
  return n
end
local function searched(name, subject, pattern, plain)
  if type(subject) ~= "string" and type(subject) ~= "number" then return end
  if type(pattern) ~= "string" then return end
  local n = #tostring(subject)
  local cost = plain and n * #pattern or n ^ (quantifiers(pattern) + 1) * #pattern
  if cost > 1e7 then
    error("string." .. name .. ": this pattern on a string of " .. n .. " characters could take too long for a frame; use fewer * + - ?", 3)
  end
  charge(n)
end
for _, name in ipairs({ "find", "match", "gmatch", "gsub" }) do
  local raw = string[name]
  safe_string[name] = function(subject, pattern, a, b)
    searched(name, subject, pattern, name == "find" and b)
    local results = pack(call(raw, subject, pattern, a, b)) return unpack(results, 1, results.n)
  end
end
local raw_rep = string.rep
safe_string.rep = function(text, n, sep)
  if type(n) == "number" and n > 0 then charge(n + (#tostring(text) + #tostring(sep or "")) * n / 16) end
  return (call(raw_rep, text, n, sep))
end
for _, name in ipairs({ "upper", "lower", "reverse" }) do
  local raw = string[name]
  safe_string[name] = function(text) if type(text) == "string" then charge(#text / 16) end return (call(raw, text)) end
end
local safe_table = copy(table)
local raw_move, raw_insert, raw_remove, raw_concat, raw_sort = table.move, table.insert, table.remove, table.concat, table.sort
safe_table.move = function(a1, f, e, t, a2)
  if type(f) == "number" and type(e) == "number" and e >= f then charge(e - f + 1) end
  return (call(raw_move, a1, f, e, t, a2))
end
safe_table.insert = function(t, ...) if type(t) == "table" then charge(#t) end return (call(raw_insert, t, ...)) end
safe_table.remove = function(t, ...) if type(t) == "table" then charge(#t) end return (call(raw_remove, t, ...)) end
safe_table.concat = function(t, ...) if type(t) == "table" then charge(#t) end return (call(raw_concat, t, ...)) end
safe_table.sort = function(t, ...) if type(t) == "table" then charge(#t * 16) end return (call(raw_sort, t, ...)) end
local safe_utf8 = copy(utf8)
for _, name in ipairs({ "len", "codepoint", "offset" }) do
  local raw = utf8[name]
  safe_utf8[name] = function(text, ...) if type(text) == "string" then charge(#text / 16) end return (call(raw, text, ...)) end
end
local safe_math = copy(math, { random = true, randomseed = true })
-- ("..."):method() looks in the strings' metatable: the same functions as string above.
getmetatable("").__index = safe_string

local function print_line(...)
  local parts = {}
  for i = 1, select("#", ...) do parts[i] = safe_tostring((select(i, ...))) end
  host.print(table.concat(parts, "\t"))
end

-- ---- the environment of the mod's files -------------------------------------

local env = {
  assert = assert, error = error, ipairs = ipairs, select = select, type = type, tonumber = tonumber,
  rawequal = rawequal, rawget = rawget, rawlen = rawlen, getmetatable = getmetatable,
  pairs = sorted_pairs, next = sorted_next, tostring = safe_tostring, setmetatable = safe_setmetatable,
  string = safe_string, table = safe_table, math = safe_math, utf8 = safe_utf8,
  print = print_line,
}

-- require loads another file of the package, once: NAME.lua beside main.lua.
local modules, loading = {}, {}
env.require = function(name)
  if type(name) ~= "string" or not name:match("^[%w_]+$") then
    error("require: a module is a file of the mod, named with letters, digits and _ (got " .. safe_tostring(name) .. ")", 2)
  end
  if modules[name] ~= nil then return modules[name] end
  if loading[name] then error("require: " .. name .. " requires itself", 2) end
  loading[name] = true
  local value = call(host.run_file, name .. ".lua", env)
  loading[name] = nil
  if value == nil then value = true end
  modules[name] = value
  return value
end

-- ---- mod: what main.lua declares --------------------------------------------

local npcs = {}
local mod = {}
env.mod = mod

-- mod.storage: a table saved with the game's file, one for each of its three files.  It is what the file the player
-- loaded holds (empty for a new file, and before a file is loaded); the game's save writes it, as it is then, into
-- that file.  Booleans, numbers, strings and tables of them, keyed by whole numbers or strings; 16 KiB once saved.
mod.storage = host.storage

-- mod.description(text): one line, 100 bytes at most, that the launcher's Mods page shows under the mod's name.  A
-- mod that gives none shows an empty line.
function mod.description(text)
  if type(text) ~= "string" then error("mod.description: give the mod's description, a string", 2) end
  call(host.description, text)
end

-- mod.house(name, { ages = { room = "0/RR", col = C, row = R }, seasons = { room = "0/RR", col = C, row = R } })
-- A house the port composes into the game before it starts: a facade of three by three metatiles whose top-left is
-- at col, row of overworld room 0/RR (the present in Ages, Holodrum in Seasons, in every season), its door below the
-- middle, and an interior with a counter, where the keeper stands.  A game the house does not name has no house.
local houses = {}
function mod.house(name, where)
  if type(name) ~= "string" or name == "" then error("mod.house: the first argument is the house's name", 2) end
  if houses[name] then error("mod.house: " .. name .. " is declared twice", 2) end
  if type(where) ~= "table" then error("mod.house: " .. name .. ": give { ages = { room = \"0/RR\", col = C, row = R } } (or seasons = ...)", 2) end
  local place = where[host.game]
  houses[name] = { placed = place ~= nil }
  if place == nil then return end
  local group, index = tostring(place.room):match("^(%x)/(%x%x)$")
  if group ~= "0" then error("mod.house: " .. name .. ": a house stands in an overworld room, written 0/RR in hexadecimal", 2) end
  call(host.house, name, tonumber(index, 16), place.col, place.row)
end

-- mod.npc(name, { house = "name" }, function(talk) ... end): the keeper of one of the mod's houses; the
-- conversation starts when Link, in front of the counter, faces it and presses A.
-- mod.npc(name, { existing = { ages = "G/RR", seasons = "G/RR" } }, function(talk) ... end): one of the game's NPCs,
-- in the room named for the game played; the conversation starts when one of its texts closes.
function mod.npc(name, where, conversation)
  if type(name) ~= "string" or name == "" then error("mod.npc: the first argument is the NPC's name", 2) end
  if npcs[name] then error("mod.npc: " .. name .. " is declared twice", 2) end
  if type(where) ~= "table" or (type(where.existing) ~= "table" and type(where.house) ~= "string") then
    error("mod.npc: " .. name .. ": give { house = \"name\" } or { existing = { ages = \"G/RR\", seasons = \"G/RR\" } }", 2)
  end
  if type(conversation) ~= "function" then error("mod.npc: " .. name .. ": the third argument is its conversation, a function", 2) end
  npcs[name] = conversation
  if where.house then
    local house = houses[where.house]
    if not house then error("mod.npc: " .. name .. ": no house " .. where.house .. " (mod.house declares it first)", 2) end
    if house.keeper then error("mod.npc: " .. name .. ": the house " .. where.house .. " has a keeper already, " .. house.keeper, 2) end
    house.keeper = name
    if house.placed then call(host.npc, name, where.house) end   -- a house of the other game only
    return
  end
  local room = where.existing[host.game]
  if room ~= nil then
    local group, index = tostring(room):match("^(%x)/(%x%x)$")
    if not group then error("mod.npc: " .. name .. ": a room is written G/RR in hexadecimal, as 3/f8", 2) end
    call(host.npc, name, tonumber(group, 16), tonumber(index, 16))
  end
end

-- mod.sprite(name, { palette = { k = "#101010", w = "#f8f8f8" }, pixels = [[ ... ]] })
-- One character a pixel; "." and " " are transparent, any other character is a key of the palette.
-- Returns the sprite's size, { width = w, height = h }.
function mod.sprite(name, spec)
  if type(name) ~= "string" or type(spec) ~= "table" or type(spec.pixels) ~= "string" or type(spec.palette) ~= "table" then
    error("mod.sprite: give a name and { palette = { ... }, pixels = [[ ... ]] }", 2)
  end
  local rows = {}
  for line in spec.pixels:gmatch("[^\n]+") do
    local row = line:gsub("^%s+", ""):gsub("%s+$", "")
    if row ~= "" then rows[#rows + 1] = row end
  end
  if #rows == 0 then error("mod.sprite: " .. name .. " has no pixels", 2) end
  local width = #rows[1]
  for i, row in ipairs(rows) do
    if #row ~= width then error("mod.sprite: " .. name .. ": row " .. i .. " is " .. #row .. " pixels wide, the first " .. width, 2) end
  end
  call(host.sprite, name, width, #rows, table.concat(rows), spec.palette)
  return { width = width, height = #rows }
end

-- ---- game: what a mod may read and ask of the game ------------------------------

local game = {}
env.game = game
game.name = host.game

-- The rupees Link has, less what game.pay has taken and the game has not yet removed.
function game.rupees() return (call(host.rupees)) end
-- Takes n rupees (an amount the game knows: 1, 5, 10, 20, 30, 50, 100...) when Link has them: true; false otherwise.
function game.pay(n) return (call(host.pay, n)) end
-- Gives a prize, by the game's own routine: game.give("rupees", 20), game.give("heart") (a full refill),
-- game.give("seeds", 10) (ember seeds), game.give("gasha_seed"), game.give("ring", "power_ring_l1").
-- false when Link cannot receive it now (no seed satchel, no ring box).
function game.give(kind, what) return (call(host.give, kind, what)) end
-- Whether Link has one of the game's treasures, by its name in the disassembly: "seed_satchel", "ring_box".
function game.has(name) return (call(host.has, name)) end

-- The game's own routines, by the names of the disassembly (constants/common/*.s, lower case). Each runs at the
-- end of the frame, as the game runs it; true when queued.
function game.give_treasure(name, parameter) return (call(host.give_treasure, name, parameter)) end   -- giveTreasure
function game.lose_treasure(name) return (call(host.lose_treasure, name)) end                        -- loseTreasure
function game.play_sound(name) return (call(host.play_sound, name)) end                              -- playSound: "snd_getseed", "mus_minigame"
function game.flag(name) return (call(host.flag, name)) end                                          -- a global flag, read now
function game.set_flag(name) return (call(host.set_flag, name)) end                                  -- setGlobalFlag
function game.unset_flag(name) return (call(host.unset_flag, name)) end                              -- unsetGlobalFlag

-- The names, checked while main.lua declares: each returns the name, or stops the load with the file and the line.
--   local JINGLE = game.sound("snd_solvepuzzle")
function game.sound(name) return (call(host.sound, name)) end
function game.treasure(name) return (call(host.treasure, name)) end
function game.global_flag(name) return (call(host.global_flag, name)) end

-- ---- a conversation ------------------------------------------------------------

local surface = host.surface
local g = {}
function g:clear(colour) return (call(surface.clear, surface, colour)) end
function g:rect(x, y, width, height, colour) return (call(surface.rect, surface, x, y, width, height, colour)) end
function g:sprite(name, x, y, flip) return (call(surface.sprite, surface, name, x, y, flip)) end
function g:text(text, x, y, colour) return (call(surface.text, surface, text, x, y, colour)) end
local BOX_X, BOX_Y, BOX_W, BOX_H = 4, 100, 152, 40

local function draw_box(lines, reveal)
  g:rect(BOX_X, BOX_Y, BOX_W, BOX_H, "#f8f8f8")
  g:rect(BOX_X + 1, BOX_Y + 1, BOX_W - 2, BOX_H - 2, "#101010")
  local left = reveal
  for i, line in ipairs(lines) do
    if left <= 0 then break end
    local shown = host.prefix(line, left)
    g:text(shown, BOX_X + 8, BOX_Y + 4 + (i - 1) * 16, "#f8f8f8")
    left = left - host.length(line)
  end
end

local function page_length(lines)
  local n = 0
  for _, line in ipairs(lines) do n = n + host.length(line) end
  return n
end

-- One page of text: two lines, revealed a character a frame, closed by A or B.
local function show_page(lines)
  local total, reveal = page_length(lines), 0
  while true do
    draw_box(lines, reveal)
    local input = yield()
    if reveal < total then
      reveal = (input.pressed.a or input.pressed.b) and total or reveal + 1
    elseif input.pressed.a or input.pressed.b then
      return
    end
  end
end

local Talk = {}
Talk.__index = Talk

function Talk:say(text)
  local lines = call(host.wrap, text)
  for i = 1, #lines, 2 do show_page({ lines[i], lines[i + 1] }) end
end

-- The question on the box's first line, its options on the second; returns the option chosen.
-- The options stand on one line of sixteen characters, two spaces between them: a longer line is refused.
function Talk:ask(text, options)
  if type(options) ~= "table" or #options < 2 then error("talk:ask: give the options, as { \"Yes\", \"No\" }", 2) end
  local width = 2 * (#options - 1)
  for _, option in ipairs(options) do width = width + call(host.length, tostring(option)) end
  if width > 16 then
    error("talk:ask: the options take " .. width .. " characters with their spaces, a line of the box holds 16", 2)
  end
  local lines = call(host.wrap, text)
  for i = 1, #lines - 1, 2 do show_page({ lines[i], i + 1 < #lines and lines[i + 1] or nil }) end
  local question = lines[#lines]
  local cursor = 1
  while true do
    draw_box({ question }, host.length(question))
    local x = BOX_X + 16
    for i, option in ipairs(options) do
      if i == cursor then g:text(">", x - 8, BOX_Y + 20, "#f8f8f8") end
      g:text(option, x, BOX_Y + 20, "#f8f8f8")
      x = x + (host.length(option) + 2) * 8
    end
    local input = yield()
    if input.pressed.left and cursor > 1 then cursor = cursor - 1 end
    if input.pressed.right and cursor < #options then cursor = cursor + 1 end
    if input.pressed.a then return options[cursor] end
    if input.pressed.b then return options[#options] end
  end
end

-- A number from 1 to n, from the conversation's own generator (the same as a scene's ctx.random).
function Talk:random(n) return (call(host.random, n)) end

-- A host scene: scene:start(ctx), then scene:update(input) and scene:draw(g)
-- once a frame until update returns a value, which play returns (false: no prize).
function Talk:play(scene, params)
  if type(scene) ~= "table" or type(scene.update) ~= "function" or type(scene.draw) ~= "function" then
    error("talk:play: a scene is a table with update and draw functions", 2)
  end
  local instance = raw_setmetatable({}, { __index = scene })
  local ctx = { params = params or {}, random = function(n) return (call(host.random, n)) end, width = 160, height = 144 }
  if instance.start then instance:start(ctx) end
  instance:draw(g)
  while true do
    local input = yield()
    local result = instance:update(input)
    instance:draw(g)
    if result ~= nil then
      yield()   -- the last image of the scene stays for its frame
      return result
    end
  end
end

-- ---- the host's entries ----------------------------------------------------------

local entry = {}

function entry.load_main()
  host.run_file("main.lua", env)   -- its errors carry their own file and line
end

-- The conversation of an NPC, as a coroutine the host resumes once a frame.
function entry.conversation(name)
  local conversation = npcs[name]
  return coroutine.create(function()
    conversation(raw_setmetatable({}, Talk))
  end)
end

return entry
