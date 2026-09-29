-- The Claw Game's sprites, original: one character a pixel, "." transparent.
-- Returns each sprite's size, which the claw uses to place the prizes and to
-- tell whether it touches one.

local black = "#101010"
local sizes = {}

sizes.claw_open = mod.sprite("claw_open", {
  palette = { k = black, y = "#c8c8d0" },
  pixels = [[
    ......kkkk......
    ......kyyk......
    ....kkkyykkk....
    ...kyyyyyyyyk...
    ..kyykkkkkkyyk..
    ..kyk......kyk..
    .kyk........kyk.
    .kyk........kyk.
    kyk..........kyk
    kyk..........kyk
    kk............kk
    k..............k
  ]],
})

sizes.claw_closed = mod.sprite("claw_closed", {
  palette = { k = black, y = "#c8c8d0" },
  pixels = [[
    ......kkkk......
    ......kyyk......
    ....kkkyykkk....
    ...kyyyyyyyyk...
    ...kyykkkkyyk...
    ....kyk..kyk....
    .....kykkyk.....
    .....kykkyk.....
    ......kkkk......
    ................
    ................
    ................
  ]],
})

sizes.rupee = mod.sprite("rupee", {
  palette = { k = black, w = "#f8c8c8", r = "#e83030", d = "#901818" },
  pixels = [[
    ...kk...
    ..kwrk..
    .kwwrdk.
    kwwrrddk
    kwrrrddk
    kwrrrddk
    kwrrrddk
    kwrrrddk
    kwrrrddk
    kwrrrddk
    kwrrrddk
    .kwrrdk.
    ..kwdk..
    ...kk...
  ]],
})

sizes.heart = mod.sprite("heart", {
  palette = { k = black, r = "#f83858", w = "#f8d0d8" },
  pixels = [[
    .kk...kk.
    krrk.krrk
    krwrkrrrk
    krwrrrrrk
    .krrrrrk.
    ..krrrk..
    ...krk...
    ....k....
  ]],
})

sizes.seeds = mod.sprite("seeds", {
  palette = { k = black, b = "#a86030", o = "#f87818", y = "#f8d848" },
  pixels = [[
    ...kkkk...
    ....kk....
    ...kbbk...
    ..kbbbbk..
    .kbboobbk.
    kbboyyobbk
    kbboyyobbk
    kbbboobbbk
    .kbbbbbbk.
    ..kkkkkk..
  ]],
})

sizes.gasha_seed = mod.sprite("gasha_seed", {
  palette = { k = black, g = "#c89048", d = "#8a5a28", w = "#f0d0a0" },
  pixels = [[
    ....kk....
    ...kggk...
    ..kggggk..
    .kggwgggk.
    .kgwggggk.
    kggggggggk
    kggggggggk
    kgggggggdk
    .kggggddk.
    ..kdddk...
    ...kkk....
  ]],
})

sizes.ring = mod.sprite("ring", {
  palette = { k = black, b = "#3878f8", w = "#c0d8f8", y = "#f8c030" },
  pixels = [[
    ...kbbk...
    ..kbwbbk..
    ...kbbk...
    ..kyyyyk..
    .kyk..kyk.
    kyk....kyk
    kyk....kyk
    .kyk..kyk.
    ..kyyyyk..
    ...kkkk...
  ]],
})

return sizes
