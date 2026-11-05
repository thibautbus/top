# Reference

The command line, every key of `settings.txt`, and the options in detail, with what they change for routes and savestates: for those who write routes, edit the settings by hand or make mods. It names the game's places and events, and exact timings. The manual for playing is [`PLAYING.md`](PLAYING.md).

- [Command line](#command-line)
- [settings.txt](#settingstxt)
- [Options in detail](#options-in-detail): [the Enhanced view](#the-enhanced-view), [the view's sizes](#the-views-sizes), [the camera](#the-camera), [colour correction and vsync](#colour-correction-and-vsync), [continuous transitions](#continuous-transitions), [the swim's hold](#the-swims-hold), [cheats](#cheats), [the compact text](#the-compact-text), [the wide text box](#the-wide-text-box), [text size](#text-size), [text speed](#text-speed), [unvisited rooms](#unvisited-rooms), [item hotkeys](#item-hotkeys)
- [Files, saves and savestates](#files-saves-and-savestates)
- [The launcher in detail](#the-launcher-in-detail): [the home screen](#the-home-screen), [Display](#display), [Controls](#controls), [the pause menu](#the-pause-menu), [Android](#android), [About](#about)
- [Log lines](#log-lines)
- [Languages in detail](#languages-in-detail)

## Command line

```bash
build/the-oracles-project                                     # the launcher's home screen
build/the-oracles-project --rom "/path/to/Oracle of Ages.gbc" # the game straight away, without the home screen
```

`--rom` starts a game without the home screen; Escape then ends the session (on Android, Back too). There is no pause menu there: the controller's menu button, its trigger and Guide do nothing. Without options it plays in Faithful, in a window at scale 4, whatever Display chose; colour correction and vsync are the settings'. Options:

| Option | Effect |
| --- | --- |
| `--save PATH` | the save file; by default the ROM's path with `.sav` (a raw SRAM image, compatible with other emulators) |
| `--patch PATCH.bps` | a fan game's patch, applied in memory to `--rom`; it, `--rom` and `--eu-rom` may be a `.zip` holding the file ([ROMs, patches and zips](#roms-patches-and-zips)) |
| `--enhanced` | the Enhanced profile, 256x144 |
| `--zoom-out` | the drawn-back view, 480x270, or 480x360 on a 4:3 screen (implies `--enhanced`); `--fullscreen --scale 4` fills 1920x1080 exactly; the same as `--view far` |
| `--view near\|medium\|far` | the Enhanced view's level (implies `--enhanced`): how much of the world it shows, in the screen's shape (4:3 or 16:9), in fullscreen as in a window |
| `--aspect auto\|16:9\|4:3` | the view's shape: the screen's (`auto`), or the one named whatever the screen; `aspect=` in the settings, which Display does not show |
| `--ghosts auto\|1\|2` | Enhanced: the ghosts preparing the rooms around at once, each on its own processor core, for this run; `ghosts=` in the settings, `auto` (the default) two on a device of four processor threads or more, one otherwise. Two fill the view faster after a warp or a load (the time travels of Ages); the log says the count at the game's start. Display's Neighbor workers, in its Advanced rows, set `ghosts=` |
| `--screen-margin off\|auto\|small\|large` | the band of the screen the game, the touch controls and the launcher's pages keep to, for this run, never written: Display's Screen margin shows it, greyed, and does not change it; `screen_margin=` in the settings (below) |
| `--core sameboy\|mgba` | the core the game runs on: SameBoy, Accurate, the reference, or mGBA, Fast, lighter, for small devices; `core=` in the settings; a savestate loads only on the core it was taken on |
| `--continuous-transitions` | Link keeps walking through the scrolling transitions (implies `--enhanced`) |
| `--continuous-swim` | the same, and Link keeps swimming through them too (implies `--continuous-transitions`); the home screen's choice turns it on |
| `--continuous-diagonal` | the same, and a diagonal held carries Link across the transitions' axis too (implies `--continuous-transitions`); the home screen's choice turns it on |
| `--continuous-steps` | the same, and Link takes a step a frame through them as the keys give it (implies `--continuous-transitions`); the home screen's choice turns it on |
| `--swim-hold` | swimming, a direction held swims at full speed where the game wants a button tapped: A with the flippers, the direction with Ages' mermaid suit (below); `swim_hold=on` in the settings |
| `--cheats LIST` | Ages, Seasons and the three fan games: the cheats named, comma-separated, among `no-damage`, `ammo` and `rupees` (below); an empty list for none; `cheats_ages=`, `cheats_seasons=`, `cheats_kinomi=`, `cheats_moonrise=` and `cheats_temple=` in the settings |
| `--text classic\|compact` | Enhanced, Ages, Seasons and the fan games: the text box's lines in the font's own widths and the texts laid out again for them (below), or as the game draws and lays them out; `text=` in the settings |
| `--text-size normal\|large\|xlarge` | Enhanced: the text box three halves as large, in whole screen pixels, or twice as large, where it fits (below); `text_size=` in the settings |
| `--text-speed classic\|instant` | the text box's lines letter by letter at the speed chosen on the file screen, or each line whole, A still turning the page (below); `text_speed=` in the settings |
| `--text-box standard\|wide` | Enhanced, Ages, Seasons and the fan games: the game's text box, or a wider one the port draws, 192 pixels of text, the texts laid out again for it, classic or compact (below); `text_box=` in the settings |
| `--unvisited-rooms shown\|hidden` | Enhanced: the rooms around Link that he has never entered, drawn (`shown`) or kept dark until he enters them, as on the game's map (`hidden`, below), for this run; `unvisited_rooms=` in the settings |
| `--item-hotkeys=off\|use\|equip` | the item hotkeys for this run |
| `--camera 1\|2` | the Enhanced camera's profile |
| `--mods DIR` | a mod, repeated for several ([`MODDING.md`](MODDING.md)) |
| `--language en-eu\|fr\|de\|it\|es` | Ages or Seasons in the European edition's English (`en-eu`) or an official translation, from the European ROM of the same game (below), or a fan game (`--patch`) in its Oracle's for the texts it shares with it; `en`, the default, plays the US text |
| `--eu-rom FILE\|DIR` | the European ROM a language comes from, or a directory where it is found by its SHA-1; without it, the refusal gives the loader's reason for the file, or names each file of the directory the loader refused (a damaged zip) with its reason |
| `--language-pack FILE` | the language's pack of the game `--rom` starts: read when it exists, else extracted from `--eu-rom` and written there; the games the home screen starts each take their own beside `settings.txt` and ignore it |
| `--record ROUTE`, `--play ROUTE` | record the session's inputs, replay them ([`ROUTES.md`](ROUTES.md)) |
| `--scale N`, `--fullscreen` | the window |
| `--colour-correction on\|off`, `--vsync auto\|on\|off`, `--mute` | image, pacing, sound |
| `--frames N`, `--no-window`, `--screenshot PATH.ppm` | quit after N frames; no window and no pacing; the last frame as an image at exit |
| `--pretend-slow` | each game in Enhanced counts as one that ran slowly: back on the home screen, the suggestion of a lighter quality, to try it |

- A gameplay option given without `--rom` (`--continuous-transitions`, `--continuous-swim`, `--continuous-diagonal`, `--continuous-steps`, `--swim-hold`, `--cheats`, `--text`, `--text-speed`, `--unvisited-rooms`, `--item-hotkeys`, `--enhanced`, `--zoom-out`, `--view`, `--aspect`, `--core`, `--camera`) overrides Display and the settings for the games the home screen starts.
- `--save`, `--frames`, `--no-window`, `--screenshot` and `--play` belong to the game `--rom` starts and are refused without it.
- `--record ROUTE` without `--rom` records the first session the home screen starts.
- `--vsync`, `--colour-correction`, `--camera`, `--aspect` and `--core` hold for the run they are given in and never change the settings file.

From the command line, `--rom BASE --patch PATCH.bps` applies the patch in memory at each launch, never writes the image, and refuses with the reason a patch made for another base, cut short or damaged (the format's three CRC32 are checked).

On Windows the game's window is sized in pixels, so the scale is the one asked at 125 % or 150 % too; the launcher's window keeps its size in points. On macOS the windows ask for a Retina display's pixels: two pixels per point.

With `--cheats`, which holds for the run, the page shows the cheats it gives, greyed, and changes nothing. From the pause, the page shows the cheats the game plays with, `--cheats`' included, and a change applies to it (below), the settings keeping the cheats the page shows.

## settings.txt

The player's settings are `settings.txt` in the user's settings directory (on Linux `~/.local/share/the-oracles-project/`, on macOS `~/Library/Application Support/the-oracles-project/`, on Windows `%APPDATA%\the-oracles-project\`; `ORACLES_SETTINGS_DIR` names another), written with all its values at the first launch so that it can be edited, but for `core=`, `subpixel_camera=`, `quality_hint=` and `link_phase=`, which appear once set ([Every key](#every-key)):

```
colour_correction=1
vsync=auto
camera=2
rom_ages=/path/to/Oracle of Ages.gbc
rom_seasons=
patch_kinomi=
patch_moonrise=/path/to/Moonrise Regalia 1.0.6.bps
patch_temple=
eu_rom_ages=/path/to/Oracle of Ages (Europe).gbc
eu_rom_seasons=
language_ages=fr
language_seasons=en
language_kinomi=en
language_moonrise=en
language_temple=en
item_hotkeys_ages=use
item_hotkeys_seasons=off
mods_ages=claw-game,fortune-teller
mods_seasons=
cheats_ages=
cheats_seasons=no-damage,rupees
cheats_kinomi=
cheats_moonrise=
cheats_temple=ammo
profile=enhanced
view=far
core=sameboy
menus=view
hud=full
aspect=auto
far=screen
unvisited_rooms=shown
ghosts=auto
transitions=on
swim_hold=off
text=classic
text_box=standard
text_size=normal
text_speed=classic
window_scale=full
scaling=sharp
screen_margin=off
update_check=on
launcher_window=1280x720
key_right=Right
key_left=Left
key_up=Up
key_down=Down
key_a=X
key_b=Z
key_select=Backspace
key_start=Return
pad_a=a
pad_b=b
pad_select=back
pad_start=start
menu_pad=leftstick
menu_tip=0
hotkeys_tip=0
key_hotkey1=A
key_hotkey2=S
key_hotkey3=Q
key_hotkey4=W
key_hotkey_bind_b=Left Shift
key_hotkey_bind_a=Left Ctrl
pad_hotkey1=x
pad_hotkey2=y
pad_hotkey3=leftshoulder
pad_hotkey4=rightshoulder
hotkey_ages_1=b:0a:--
hotkey_ages_2=-
hotkey_ages_3=-
hotkey_ages_4=-
hotkey_seasons_1=-
hotkey_seasons_2=-
hotkey_seasons_3=-
hotkey_seasons_4=-
```

The file the launcher writes has comment lines too (`#`), which it writes again each time: one above each group of keys, and above a hotkey slot the name of its item.

Edit `settings.txt` only while the launcher is closed. The launcher reads it when it starts and writes the whole file again at each change, from the home screen or from a game, so that an edit made while it runs is lost.

### Every key

| Key | Values | Default | Launcher row | Applies |
| --- | --- | --- | --- | --- |
| `profile` | `faithful`, `enhanced` | `enhanced` | Display › Profile | at the next Play |
| `view` | `near`, `medium`, `far` | the device's quality | Display › View | at the next Play |
| `core` | `sameboy`, `mgba` | absent: `mgba` on Android, `sameboy` elsewhere | Display › Advanced › Core | at the next Play |
| `ghosts` | `auto`, `1`, `2` | `auto` | Display › Advanced › Neighbor workers | at the next Play |
| `quality_hint` | `low`, `medium`, `high`, `max`, `custom` | absent | none: the quality a game that ran slowly last suggested to leave | written by the launcher |
| `window_scale` | `2`, `3`, `4`, `full` | `full` | Display › Window | at the next Play; from the pause at once |
| `hud` | `original`, `full`, `wide` | `full` | Display › HUD | at once, from the pause too |
| `text` | `classic`, `compact` | `classic` | Display › Text › Text | at the next Play |
| `text_box` | `standard`, `wide` | `standard` | Display › Text › Text box | at the next Play |
| `text_size` | `normal`, `large`, `xlarge` | `normal` | Display › Text › Text size | at once, from the pause too |
| `text_speed` | `classic`, `instant` | `classic` | Display › Text › Text speed | at the next Play; from the pause when the game resumes |
| `scaling` | `sharp`, `fill` | `sharp` | Display › Picture › Scaling | at once, from the pause too |
| `screen_margin` | `off`, `auto`, `small`, `large` | `off` | Display › Picture › Screen margin | at once, from the pause too |
| `colour_correction` | `0`, `1` | `1` | Display › Picture › Color correction (F2 in game) | at once, from the pause too |
| `far` | `screen`, `fixed` | `screen` | Display › Picture › Far size (Fitted, Fixed) | at the next Play |
| `menus` | `view`, `large` | `large` on Android, `view` elsewhere | Display › Picture › Menus & scenes | at once, from the pause too |
| `unvisited_rooms` | `shown`, `hidden` | `shown` | Display › Picture › Unvisited rooms | at the next Play; from the pause when the game resumes |
| `transitions` | `on`, `off` | `on` | Display › Advanced › Continuous transitions | at the next Play |
| `vsync` | `auto`, `on`, `off` | `auto` | Display › Advanced › Vsync | at the next Play; from the pause when the game resumes |
| `subpixel_camera` | `0`, `1` | absent: the platform's default | Display › Advanced › Camera (Pixel, Smooth); with Smooth the camera also tells a stop on Link's first still frame ([the camera](#the-camera)) | at once, from the pause too |
| `camera` | `1`, `2` | `2` | none: the camera's profile, 2 following Link at his own speed and settling when he stops ([the camera](#the-camera)) | at the next Play |
| `link_phase` | `0`, `1` | absent (written only as `1`) | none: a trial, Link drawn in phase on a diagonal walk, the band following him; nothing with the Smooth camera | at the next Play |
| `aspect` | `auto`, `16:9`, `4:3` | `auto` | none: the view's shape, the screen's or the one named; the launcher's own layout follows it too | at the next launch |
| `rom_ages`, `rom_seasons` | a path | empty | Cartridge › ROM | at the next Play |
| `patch_kinomi`, `patch_moonrise`, `patch_temple` | a path | empty | a fan game's Cartridge › Patch | at the next Play |
| `eu_rom_ages`, `eu_rom_seasons` | a path | empty | Cartridge › European ROM | at the next Play |
| `language_ages`, `language_seasons` | `en`, `en-eu`, `fr`, `de`, `it`, `es` | `en` | Cartridge › Language | at the next Play |
| `language_kinomi`, `language_moonrise`, `language_temple` | the same | `en` | none: read and written, not applied (the home screen plays the fan games in English) | never |
| `mods_ages`, `mods_seasons` | the mods' folder names, comma-separated, eight at most | empty | the Mods page's switches | at the Mods page's Play |
| `cheats_ages`, `cheats_seasons`, `cheats_kinomi`, `cheats_moonrise`, `cheats_temple` | `no-damage`, `ammo`, `rupees`, comma-separated | empty | the Cheats page | at the next Play; from the pause when the game resumes |
| `item_hotkeys_ages`, `item_hotkeys_seasons` | `off`, `use`, `equip` | `off` | Controls › Item hotkeys (Off, On: `use`; `equip` only by hand) | at the next Play; from the pause when the game resumes |
| `swim_hold` | `off`, `on` | `off` | Controls › Swimming (Tap, Hold) | at the next Play; from the pause when the game resumes |
| `key_right`, `key_left`, `key_up`, `key_down`, `key_a`, `key_b`, `key_select`, `key_start` | an SDL key name, or empty for none | `Right`, `Left`, `Up`, `Down`, `X`, `Z`, `Backspace`, `Return` | Controls › Buttons, Keyboard | at once, from the pause too |
| `pad_a`, `pad_b`, `pad_select`, `pad_start` | an SDL controller button name, or empty | `a`, `b`, `back`, `start` | Controls › Buttons, Gamepad | at once, from the pause too |
| `menu_pad` | a button name, `lefttrigger`, `righttrigger`, or empty for Guide alone | `leftstick` | Controls › In game › Menu | at once, from the pause too |
| `menu_tip` | `0` to `9`, `off` (`on` reads as `0`) | `0` | Controls › Reset to defaults sets it back to `0`: the games the reminder naming the menu's key was shown over; `off` once the menu was opened by its key, or after ten | written by the launcher, at the end of a game that showed it and as the menu opens by its key |
| `hotkeys_tip` | `0` to `2`, `off` (`on` reads as `0`) | `0` | Controls › Reset to defaults sets it back to `0`: the pause menus of Ages or Seasons the line on the item hotkeys was shown in; `off` once they were turned on in Controls, or after three | written by the launcher, as a pause menu that shows it opens |
| `key_hotkey1` to `key_hotkey4` | an SDL key name | `A`, `S`, `Q`, `W` | Controls › Hotkeys, Slot 1 to Slot 4 | at once, from the pause too |
| `key_hotkey_bind_b`, `key_hotkey_bind_a` | an SDL key name | `Left Shift`, `Left Ctrl` | Controls › Hotkeys, Bind to B, Bind to A | at once, from the pause too |
| `pad_hotkey1` to `pad_hotkey4` | an SDL controller button name | `x`, `y`, `leftshoulder`, `rightshoulder` | Controls › Hotkeys, Slot 1 to Slot 4, Gamepad | at once, from the pause too |
| `hotkey_ages_1` to `hotkey_ages_4`, `hotkey_seasons_1` to `hotkey_seasons_4` | `-` for an empty slot, else `<b or a>:<item>:<variant or -->` in hexadecimal, the ROM's own identifiers (`b:0a:--`) | `-` | the slots, filled in game | written by the launcher |
| `update_check` | `on`, `off` | `on` | About › Update check | `off` at once; `on` at the next launch |
| `launcher_window` | `WIDTHxHEIGHT` | `1280x720` | none: the home screen's window as it was left | at the next launch |

### The keys in detail

A key's name and its value may have spaces around them and capitals, as in `update_check = OFF`: they are read as `update_check=off` and written back so. A key the launcher does not know, a later version's or a test build's, is kept as it was, after the key it followed in the file (at the end when that key is not written), and never read; a comment is not kept, the file's own being written again.

- `eu_rom_ages=` and `eu_rom_seasons=` are Cartridge's European ROM, `language_ages=` and `language_seasons=` its Language (`en`, `en-eu`, `fr`, `de`, `it` or `es`); the fan games' `language_` keys are not applied (below).
- `ghosts=` is Display's Neighbor workers (`auto`, `1` or `2`), `scaling=` its Scaling (`sharp` or `fill`).
- `screen_margin=` is the band of the screen the game, the touch controls and the launcher's pages keep to, the rest black:
  - `off` (the default) the whole screen, as on every device until now;
  - `auto` clear of a notch or a curved edge that Android reports at the top or the bottom of the landscape screen (a display cutout, a waterfall edge);
  - `small` and `large` 24 and 48 dp clear at the top and at the bottom, in the display's pixels (on Android its density, on a desktop its scale, a pixel a dp at 100 %), or as far as `auto` would where that is more.

  In the band the game keeps its largest whole scale, one less than the whole screen's where the margins take what it needed (Medium's 384x216 at four times on a 1080-line screen rather than five), and Fill fills the band's height; a margin that would take half the screen is not applied. A change applies at once, from the pause too.
- `hud=` is the status bar in Enhanced:
  - `full`, the default, draws it across the whole view, the bands beside it on its sixteen lines taking its own background, the colour the game gives its edge at each frame (its fades, its own colour changes, the colour correction), a line it shows blank extending as it is;
  - `original` keeps it as the game draws it, 160 pixels wide between two dark bands;
  - `wide` lays it out across the band on that background, the game's own blocks moved as they are: the hearts and the money (the rupees, a dungeon's keys, Subrosia's ore) at the left corner, B and A at the right, the item hotkeys' slots just before B, eight pixels from the view's edges where the view leaves them (four in Medium 4:3, none in Near 16:9, whose 256 pixels the blocks fill, nor in Medium 4:3 with fifteen or sixteen hearts).

  The hearts take one row where the view's width allows it (Medium, Far and Far's fitted sizes), the game's two rows in Near; in Near 4:3, 213 pixels wide, the slots do not fit and are not drawn, the hotkeys working all the same (Controls › Hotkeys shows their items). The game's other layouts follow: fifteen or sixteen hearts, Biggoron's sword. A game whose status bar is none of the game's layouts keeps it centred.

  The game's world is never covered, the framed core (menus, cutscenes) keeps its dark border and the bar centred, a room's load keeps the band as in play, and a change applies to a running game at once.
- `swim_hold=` (`off` or `on`) is the swim's hold for every game the home screen starts, Controls' Swimming row (`Tap` or `Hold`).
- `text=` (`classic` or `compact`) is the compact text for every game the home screen starts in Enhanced (below), `text_box=` (`standard` or `wide`) its text box (below); both apply at the next Play, the texts being laid out when the game starts, and change from the home screen only, the pause showing them greyed.
- `text_size=` (`normal`, `large` or `xlarge`) is the text box's size (below), which applies at once, from the pause too.
- `unvisited_rooms=` (`shown` or `hidden`) is Display › Picture's Unvisited rooms, the rooms around never entered kept dark in Enhanced (below), for every game, `--unvisited-rooms` overriding it for a run without changing it.
- `cheats_ages=`, `cheats_seasons=`, `cheats_kinomi=`, `cheats_moonrise=` and `cheats_temple=` are each game's Cheats page, the cheats it starts with from the home screen (below), a fan game's its own and not its Oracle's, a list of their names; a cheat the launcher does not know (a later version's) is dropped, with a line on the standard error, the others kept.
- `core=sameboy` or `core=mgba` appears once the player chooses a core in Display's Advanced rows: without it, the application's default plays (Accurate on the desktops, Fast on Android), so that an update that changes the default reaches a player who never chose. A game's save (`.sav`, the cartridge's RAM) is the same file on both cores: a game saved on one opens on the other; a savestate does not.

`update_check=` is the update check, `on` by default: when the home screen opens, the launcher asks GitHub once, in the background and never waiting for it, for the latest release of the port, and compares its version with its own.

A newer one appears on the home screen when the answer comes, or when a game the answer came during returns to it, fading in by the version, nothing else moving: "· 1.3.0 available" after it in the 16:9 layout, "1.3.0 available" on its own line above it in the 4:3 layout.

Down from Exit highlights it, down again the version number, then Start game, and up from Start game reaches the number, then the notice; Enter, A, a click or a tap opens its release page in the system's browser, the launcher staying open, and the key hint Select says Open while it is highlighted; where no browser takes it (some Android consoles have none), a message gives the page's address instead.

It shows on the home screen only: not in the pause menu, not on the other pages. The standard error names it as well (`update check: version 1.3.0 available`), without the address asked or the page's.

Offline, a refusal, GitHub's rate limit, an answer it cannot read, a draft or a pre-release: nothing is shown, a line on the standard error says why. `off` asks nothing, and hides a notice already shown. A game started with `--rom` never asks.

`update_check=`'s request: on Linux the request goes through the system's libcurl (`libcurl.so.4`), loaded when the check runs: without it nothing is asked.

## Options in detail

### The Enhanced view

**Enhanced** shows a wide surface, 256x144: the status bar in a band of sixteen lines at the top, centred, and below it the world, the play area placed by a smooth camera in world coordinates with the neighbouring rooms drawn around it, computed and peopled as [Faithful and Enhanced](PLAYING.md#faithful-and-enhanced) says. Large rooms (dungeons) are shown whole, centred, their scrolls sliding the band; interiors of one room are centred and still.

- Menus, cutscenes and the file select show the core's image framed: at the view's scale, or alone at its own largest whole scale, as Faithful shows it, with `menus=large` in the settings (Display › Picture's Menus & scenes), the default on Android, where the view's scale on a small screen makes them hard to read (`menus=view` keeps the view's), which changes from the pause too, at once.
- A room's load (a door, stairs: the room left's last frames, the reveal, Link walking in until the camera takes him) stays at the view's scale, the status bar's band at the top as in play (its background, the hotbar) and below it the play area alone, where the world shows it before and after (but a room Link walks into from its edge, which the camera frames once he is in), the screen's blank between them (the LCD off, a full fade) framed as the game shows it, at the large menus' scale where it follows a screen they enlarge (a cutscene's picture fading out, the map) until the room shows.

### The view's sizes

**The drawn-back view** (`--zoom-out`) gives the Enhanced surface 480x270: at scale 4 it fills a 1080p screen. On a 4:3 screen it is 480x360, the same three rooms across with 90 lines more of the world.

Outdoors the camera frames three rooms across and two down (nearly three in 4:3), Link on the middle line; a large room is shown whole, centred; elsewhere the normal band stays, centred, but in Gifts of Kinomi, whose caves are interiors of rooms joined by scrolls: there the whole band shows the interior with the rooms it opens onto, above and below as beside, black around. A savestate taken at one size is refused at the other.

**The view's levels** (`--view`, `view=` in the settings): the Enhanced surface is near (256x144), medium (384x216) or far (480x270, the drawn-back view), each in the screen's shape: on a 4:3 screen near is 213x160, medium 320x240 and far 480x360 (on a 640x480 screen, three, two and one times; far fills 960x720 twice and 1440x1080 three times).

The shape is the screen's, nearer 4:3 or 16:9, unless `aspect=16:9` or `aspect=4:3` in the settings, or `--aspect` for one run, names one whatever the screen (a 4:3 view tried on a wide screen, or a screen whose shape is misread). A window takes the surface's shape.

**Far fitted to the screen**: where far's size covers less than four fifths of the screen at its largest whole scale, far takes a size that fills it three times, with as much of the world as the screen allows: 426x320 on a 4:3 screen of 1280x960 (a Retroid Pocket Nova, an Anbernic RG477M, a Retroid Pocket Mini), 412x360 on 1240x1080 (a Retroid Pocket Mini V2 or Classic, an AYN Thor's bottom screen), where 480x360 filled half the screen twice; 2.7 rooms across rather than three.

It is computed when the game starts, on the band the screen margin leaves the game (`screen_margin=`, Display › Picture's Screen margin), the screen's lines above and below it taken off, the shape still the screen's: on 1280x960, Small's margin leaves 1280x840 at 2.5 pixels a dp, where 426x320 does not fit three times, and far keeps 480x360 at twice, as it did before it was fitted. And only where the view takes the screen's shape (`aspect=auto`). Every other screen keeps far's size (640x480, 960x720, 1024x768, 1920x1080, phones, monitors).

`far=fixed` in the settings (Display › Picture's Far size, Fixed) keeps far's size whatever the screen (`far=screen`, Fitted, by default), and Display then shows that size; it changes from the home screen only, a running game keeping its view's size; the log's start says which size far took and why (`the far view fitted to the 1280x960 screen: 426x320 at 3x, where 480x360 covers 56% of it at 2x`, or with a margin `the far view at 480x360: the 1280x840 band the margin leaves on the 1280x960 screen at 3x gives 426x280, not a size the view takes`).

A savestate taken at one of far's sizes loads only at that size, and the refusal names the setting that shows the surface it was taken on (`far=fixed`, `far=screen`, `aspect=16:9`).

Medium draws back as far does: two rooms or more across outdoors, a large room whole. Near in 4:3 is narrower than a large room's 240 pixels and shorter than its 176 lines: the band follows the game's window across and up and down, inside the room, all its 144 lines shown; a room of one screen (most interiors and dungeon rooms) shows the game's 128 lines, the band black above and below them.

### The camera

The camera has two profiles (`--camera`, `camera=` in the settings):

- 1 waits for Link to cross a dead zone of sixteen pixels and catches up;
- 2, the default, follows Link continuously at his own speed, steady on stairs as on flat ground, and comes to rest when he stops: it lets its look-ahead go and brakes to a stop, a little past him only when it cannot stop sooner, never coming back.

With the Smooth camera (below, `subpixel_camera=`) it tells the stop on the first frame he stands still with the pad let go; with a direction held on the pad along that axis, either way, or during a transition, a frame he stands still is the game holding him (the end of a scroll, a swim stroke) or a wall, and the camera keeps its speed until four such frames say he has stopped, as the Pixel camera always waits.

When the game puts him elsewhere (back at a room's entrance after a fall or a whirlpool) it pans to him in a fraction of a second rather than gliding there. Toward a neighbouring room still being prepared or waiting for the ghost, or past an edge it shows black, it goes no further than Link himself, without looking ahead, until the room is there, so that it seldom shows more black than before (a little more when the ghosts run far behind, in a thread on a busy device; and where Link stands beside the black past the map's edge, framed without the look-ahead the previous camera kept).

With the Pixel camera its whole pixels step with Link's:

- a move that would set his sprite back against the way he last stepped waits until the smooth camera is two pixels away, three on a frame he does not step;
- three or four pixels it held come as two a frame, a greater gap at once (the woods' loop putting the world elsewhere, a pose putting his sprite three pixels away);
- it never waits past an edge it is held at, so that Link's sprite seldom goes a pixel one way and back on the screen;
- the band then steps as Link does, 1, 1, 0 on a diagonal, as in the game itself.

In both, a frame the game holds Link while the camera moves can still show him back a little, half a pixel with the Smooth camera, a pixel with the Pixel one, as at the end of some scrolls.

`subpixel_camera=` is Display › Advanced's Camera, the Enhanced camera to the 256th of a pixel: `1` (Smooth) moves the band by the camera's fraction of a pixel at the screen's own resolution and draws Link at his exact place, so that a diagonal walk, a swim across the sea floor or the stairs scroll smoothly.

In a dungeon's large room, where the camera is the game's window, Link is drawn at his exact place there too: the room shown whole and still (medium, far), he alone moves by his fraction; where the band follows the game's window (near), it takes his fraction with the window's whole pixels while the window follows him, short of the room's far edges, so that he does not step across and down in turn on a diagonal.

The side views and the large rooms under the sea's waves keep the whole pixels. Where the band takes Link's own fraction there, his sprite stays on the band's pixel of the screen, rather than a pixel of the screen back and forth against it as his own rounded down would.

`0` (Pixel) keeps the camera's whole pixels, as before 1.2.0.

Absent, the platform's default plays, whatever the Quality:

- on on Windows, macOS and Linux;
- on Android, on where the game is shown at three times its size or more on the screen (in the screen margin's band, Fill's fraction left out: Near on a 640x480 handheld, every view on a 1080-line screen), off under it, where the camera's half pixels made Link step back on the screen more often than they smoothed the walk.

The key is written once the player chooses in the row, so that a later default leaves the player's choice alone.

A change applies at once, from the pause too, the band and Link taking up or giving back their fractions over a few frames rather than at once, and so does a Scaling, Window or Screen margin that takes the view across the Android threshold. On a screen less than twice the game's size it changes nothing. With a Screen margin and the touch controls the band is moved too. The session's log says which camera a game starts with and why, and at its end on how many frames the band was moved.

### Colour correction and vsync

Colour correction off is the raw RGB555 conversion; on, SameBoy's "modern balanced" rendering, close to an original screen.

`vsync auto` presents each frame at the display's rate when it refreshes between 59 and 61 Hz, close enough to the Game Boy's 59.7275 Hz for the audio rate control to absorb the difference; on another display, or with `off`, the host paces at 59.7275 Hz. Under vsync the host checks that the presentation waits for the display, and if it does not, turns vsync off and paces itself for the rest of the session. Vsync changed from the pause applies when the game resumes: turned on, the host checks the display again from there; turned off, it paces.

### Continuous transitions

**Continuous transitions** (`--continuous-transitions`, both games): in the wide view the game's own scroll no longer shows, and what remains of a transition is Link frozen for about forty frames. The option doubles the scroll's step and moves Link a pixel a frame in the transition's direction, his walk animated, so he keeps walking (with the steps below, while a direction is held toward the edge); beyond the pixels the game moves him itself, he only walks onto tiles without collision.

A transition the game walks Link into itself, a scene driving him with its own keys (`wUseSimulatedInput`, but its value 2, the player's keys reversed), stays the game's whatever the options below: its scroll, its bound at the edge, and the place it gives him in the room entered, which the scene goes on from.

The game's state then differs from a native session's: a route recorded with the option replays with it, and a savestate taken with it is refused without it. A savestate keeps where the option stands in a transition in progress, which goes on after the load as it would have; one written by an earlier version leaves that transition to the game's scroll, its step as the option had it.

**The swim through them** (`--continuous-swim`, which Display's continuous transitions turn on; a separate option so that the routes recorded with `--continuous-transitions` alone replay as they were played): Link swimming at the surface, in both games, flippers or Ages' mermaid suit, diving or not, keeps swimming at his own speed through the transition, never ending it short of the pixels the game moves him itself, his swimming animation going on, and beyond the pixels the game moves him itself only into water.

Ages, Seasons, Gifts of Kinomi and Temple of Seasons qualify it, Gifts of Kinomi with its flippers, its Zora scale speeding a swim where the swimmer's ring does in Ages, Temple of Seasons with its flippers, as Seasons swims (its tiles of sea water, entered with its suit, are left to the game); Moonrise Regalia, whose water stops a swim every frame, plays without it when the home screen starts it, and refuses `--continuous-swim` with `--rom`.

On Ages' sea floor he goes on at his own speed too, onto tiles without collision, walking while a direction is held and standing without one, as the game draws him there.

A stroke of the flippers that carries him to an edge takes the transition the game would take with the direction held, when the direction held neither points to the edge nor away from it; the mermaid suit, driven by the direction pad, keeps the game's behaviour.

With the option, Link is also held back from a wall leftward and upward, and walks onto stairs and along a bridge. A savestate does not count the swim: a state taken with it loads without it.

**The diagonal through them** (`--continuous-diagonal`, which Display's continuous transitions turn on; a separate option so that the routes recorded without it replay as they were played): the game holds Link across the transition's axis for the whole scroll, so that walking diagonally into a room he went straight along the axis for half a second, the view sliding along it alone.

With the option, on foot, a diagonal held toward the edge goes on as the game walks it, at its own speed along each axis, across the axis onto tiles of the room entered without collision; a wall there holds him as the game would, and the carry never leaves him, when the scroll ends, against a wall or over a hole, water or lava where walking straight would not.

Swimming (with `--continuous-swim`, flippers or Ages' mermaid suit) and on Ages' sea floor, his motion goes on at its own angle and speed, which the game freezes through the scroll and takes up again after it, across the axis only into water swimming.

While the room entered loads, its walls are read from the game's own tables, as the load will make them, so that the carry goes on through it in Ages, Seasons and the three fan games; in the few rooms whose own code changes their tiles (all of them in the fan games), nothing carries him across until the walls are known, and what those frames held back is caught up then, at his own ratio across and along, never faster than a pixel a frame across on foot.

A savestate does not count it. A game whose profile does not qualify the diagonal plays without it when the home screen starts it, and refuses `--continuous-diagonal` with `--rom`; Ages, Seasons and the three fan games qualify it.

**A step a frame through them** (`--continuous-steps`, which Display's continuous transitions turn on; a separate option so that the routes recorded without it replay as they were played): without it, Link walks the whole scroll at a pixel a frame whether a direction is held or not, so that a tap at a room's edge takes him through the scroll by himself, and the frame that ends the scroll leaves him still for a frame, the one the game finishes the scroll in, before he goes on, as a slight catch.

With the option, on foot, he goes through the transition as he would walk in the open, as the keys say:

- a direction held toward the edge, straight or diagonally, walks him on at the game's own speed, through a long load of the room too;
- with none held there, from the frame it is let go he goes on to where the game itself places him at the scroll's end, its own move of the transition (15 pixels across, 16 up or down, from where it began, the pixels he walked included, or further if he walked further), evenly up to the frame that ends the scroll, never faster than he walked, and without walking (let go late in the scroll after a diagonal, he may stop a little short of that place; let go at the end of a long load, after walking as far as the scroll's own steps take him, he stands until they start);
- with the diagonal, a direction held across the axis, a diagonal or square across, walks him that way too, turned to face it as the game turns him (and coming back that way after a fall in that room), onto tiles without collision, as soon as the room entered's walls are known, and never past the edge of the room entered on that side, slowing as he nears it so as to reach it as the scroll ends rather than stopping short on that side, so that at a corner the next transition carries on from where he stands.

However far into the room entered he walked or swam, a whirlpool, a hole or a drowning there brings him back where the game itself would have placed him at the scroll's end. Without the option the continuous transitions leave that place where the scroll left him, which may be well into the room: a whirlpool there can take him back to it again and again.

The frame that ends the scroll gives him his step, walking or swimming, as the frames before it, and the frame that starts a diagonal one his single step, where the game's own and the option's came together. Swimming and on Ages' sea floor, his motion is the game's, as without it.

A savestate does not count it. Ages, Seasons and the three fan games qualify it.

**At the line between the rooms** (`--continuous-line`, which Display's continuous transitions turn on, with the steps; a separate option so that the routes recorded without it replay as they were played): the game starts a scroll 6 pixels before the line between two rooms (5 at the right) and never lets Link stand in those pixels, putting him back on their bound at once, so that a tap at a room's edge always takes him into the next room, and the next room always pushes him past those pixels on its side, whether a direction is still held or not.

With the option, on foot and on Ages' sea floor, the scroll starts as his middle reaches the line itself, and he stands anywhere in a room up to it:

- a tap at an edge takes him toward it and no further, the room unchanged;
- on foot, letting go during the scroll leaves him where he is, the scroll's own move undone, however few pixels past the line, standing in the pose the game gives him with nothing held (`animateLinkStanding`: his walk's first frame, set again each frame) rather than the step the scroll froze;
- holding the way back walks him back through the scroll across the line and into the room he left, on that room's walls and holes as normal play last had them, and once the scroll is over the game takes him back into that room from where he stands, the scroll back starting at once (the transition forced, without the game putting him back on the line first);
- a wall of that room just past the line holds him at the line itself, and the scroll back starts as he crosses it; so does the line where either room has a transition of its own (`getNextActiveRoom`'s list: Seasons' Lost Woods and Onox's castle, Ages' scrambled forest, the eye puzzles), whose scroll back may lead elsewhere than the room he left.

Carried across the axis by a diagonal, he goes on at his pace past the room's own edge on that side, into the room beside the room entered, whose walls, holes and water hold him there as the game's would, as far as 32 pixels past the line.

Outdoors, on Ages' two overworlds and Seasons' Holodrum and Subrosia, each within its own map (from its first column and row where a fan game walls it inside the grid), and on Ages' sea floor, its collisions are predicted from the ROM's tables as the game would load them (`engine/gameplay/room_predict.c`: the room's index beside, its tileset as `loadTilesetData` chooses it, Ages' group swapped by the room's flags, Seasons' season, then its layout, its substitutions on its own flags, and its tileset's collisions), and compared with what the game loads once a scroll enters that room (`transitions.beside_mismatched`).

At the frame that ends the scroll, past the line on that side, the scroll into the room beside starts there, from where he stands, with no frame of normal play between the two scrolls. No key toward the edge is needed at that frame: past the line, he may not stay in the room entered.

Where it is not predicted (a map's edge, a room with a transition of its own on either side, a room pack of its own (but in Subrosia, whose scrolls compare no pack in the game either), a tileset the game's code chooses, a room whose own code changes its tiles after its graphics are drawn (a Gasha spot with something planted, a few others), a large room, the dungeons, in the fan games a room whose own code changes its tiles as it loads), and where one of the room entered's objects, created by its load, may leave the player without the controls in its first frame (an entry scene, a character's script that disables the menu or forces Link's state: the objects the games' sources say may do it in their first frame, drawn into `engine/gameplay/room_scenes.c`; they run that frame once the second scroll is decided, which they could no longer refuse), he goes as far as the room's own edge, where the game now lets him stand, slowing as he nears it so as to reach it as the scroll ends.

If the transitions are disabled while he stands past the line, he walks back across to it during the scroll; still past it as the scroll ends, he is put back on it, as the game's own edge test would.

Swimming at the surface (with the swim through them), the edges are the line's as well, at the bottom where the tile the game takes as his own (y+5) is still the room's last row (y 122: past it the game would read the row beyond the room, no water, and take him out of it).

Through the scroll he swims as the game's own swim routines move him in the middle of a room, a frame at a time: `updateLinkDirectionFromAngle`, then the mermaid suit's `linkUpdateVelocity` or the flippers' `linkUpdateFlippersSpeed` and `func_5933` (their speed and its cap, the counter and period of its steps, the stroke's state and frames), the swim's hold's strokes included, his move that frame read from `objectSpeedTable` on both axes, the scroll's own move undone and nothing added.

His walls and his own tile hold him as the game's would (no wall at the points `calculateAdjacentWallsBitset` tests on the side he goes, his tile in water he can swim in), and his swim's state is written back, so that the game takes it up after the scroll as it stood.

That bottom line is his wherever the line's tests take him past it: carried across into the room below, put back on it, or swimming back down into the room left.

Scrolling up into a room, he never ends the scroll in its rows under that line (y 123 to 127), where the frame of normal play after it would take him out of the water: swum back down there, he goes back into the room he left, the scroll back starting at the frame that ends the scroll; going up or gliding to a stop at a pace that would leave him there, he is taken over the line by the scroll's end, no more a frame than his pace lacks spread over the frames left (`transitions.swim_lifted_frames`).

On Ages' sea floor, where the game has him walk in the mermaid suit's gait (`linkUpdateVelocity@mermaidSuit`, then `linkUpdateMovement`), the edges are the line's at every side, the room's last row included, and he goes through the scroll the same way, his walk animated with a direction held and standing without one, a hole holding him as on foot, and a move held whole by a wall stopping him (`linkResetSpeed`), as the game has it.

The step is the game's own at every frame of normal play (`transitions.swim_model_mismatched`, none on any route), so a glide let go at an edge goes exactly as far as in the middle of a room (`swim.edge_glides_unlike`).

A swim that would carry him across the line, a glide or a direction held toward it, starts the scroll at the frame before the one whose step takes him past it, from where that step leaves him (at the frame that ends a scroll too), so that the game never puts him back on the line first; one that dies short of the line leaves him in his room.

Turned back, he swims back past the line onto the room left's water, its tiles and walls as normal play last had them, and the scroll back starts at the frame that ends the scroll, with no frame of normal play outside the room entered; at a corner a diagonal carries him into the room beside onto its predicted water, as on foot; where the room beside is not predicted (a room whose own code changes its tiles after its graphics are drawn, and wherever it is not on foot), his move across slows so as to reach the line as the scroll ends, as on foot, the next scroll starting there.

On a current (`wcc92`) the game's own edge and push stay. The game's own reasons not to start a scroll stay, at the line: a hole, water without the flippers, a knockback, a jump's delay; and so do the scrolls it starts without a direction held: a current, a jump down a ledge, a fall off a cliff into the room below, the cart.

On an animal, in a side view, in a dungeon's large rooms (whose transitions are the game's, its push included), at the edges of Ages' world map, and where a script disables the transitions (Maple scattering her things, the wind seed, Ricky's jump), the game's own edges stay. A fall or a drowning in the room entered brings him back where the game itself would place him after its scroll, as with the steps.

Ages, Seasons and the three fan games qualify it.

### The swim's hold

**The swim's hold** (`--swim-hold`, `swim_hold=on` in the settings, off by default, in Faithful and Enhanced, Ages, Seasons, Gifts of Kinomi and Temple of Seasons): swimming, Link speeds up by strokes, and the game starts a stroke only on a new press, so the player taps.

- With the flippers, in both games, at the surface or in a side view, A starts a stroke of 25 frames, which speeds Link up and slows him back, the presses during it read by nobody; held, A gives one stroke.
- With Ages' mermaid suit, wherever the game swims with it (the sea floor, the surface, the side views), a direction starts a stroke of five frames, each raising Link's speed and turning him towards the direction held; a direction released during one slows him down, and on the sea floor he moves only while one is held; held, a direction gives one stroke and Link stops.

With the option, a direction held swims as the best tapping does, in both games, and so does A held with the flippers: when a stroke would serve (with the flippers, as soon as the last one ends; with the suit, Link below his top speed, turning, or about to slow down), the game takes the direction held, or A, for a new press of the button it waits for, sound included, and plays its own stroke; it reads nothing else differently and nothing is written into the game. With nothing held, no stroke starts: Link slows down and stops as the game has him.

Held, the flippers stroke every 25 frames, Link swimming about four fifths of a pixel a frame, as fast as any tapping of A, against half a pixel with the direction alone; the suit reaches its top speed, a pixel and an eighth a frame, in about ten frames, and keeps it with a stroke every nine. Tapping still swims, as the game wants it, a little less well with the suit where a tap is released during a stroke the option started.

Gifts of Kinomi and Temple of Seasons have it, with their flippers; Moonrise Regalia does not: the home screen starts it without it, and `--rom` refuses `--swim-hold` for it. A route recorded with it replays with it (`swim-hold` in its options), and one recorded without replays without it whatever the setting; a savestate does not count it.

### Cheats

**Cheats** (`--cheats`, `cheats_ages=`, `cheats_seasons=`, `cheats_kinomi=`, `cheats_moonrise=` and `cheats_temple=` in the settings, none by default, in Faithful and Enhanced, Ages, Seasons and the three fan games): each cheat is on when its name is in the list.

- `no-damage`: a hit still knocks Link back, with its sound, its red flash and the frames during which nothing hits him again, but he loses no heart, and so never dies of a hit; a fall into a hole, the water or the lava sends him back where the game sends him, at no cost. The hearts the story takes itself are taken (in Ages the fairy of the bombs' upgrade, who leaves Link one heart, and the Smog's penalty; in Seasons the scripted collapse in the Moblins' keep). With full hearts, a sword of level 2 or more throws its beam at every swing, as the game has it.
- `ammo`: a seed, a bomb or a Bombchu used is not taken from its count; with none left there is still none, the cheat giving nothing, and Ages' bomb-upgrade fairy still takes every bomb from a Link who claims the silver bomb.
- `rupees`: a price the game takes (a shop, a game, the rupees a character asks for) costs nothing, as long as the wallet holds it, which the game checks first; a mod's prices (`game.pay`) still apply, and Seasons' ore chunks are spent as the game spends them.

The game reads nothing else differently and nothing is written into it: what a cheat leaves (rupees kept, items bought, ammunition kept, fewer deaths) is in the game when it saves, in the same save as without cheats.

Gifts of Kinomi keeps Ages' routines for all three, its seeds, bombs, Bombchus and prices included (777 rupees among them), and Ages' bomb-upgrade fairy; so does Moonrise Regalia, with Ages' prices. Temple of Seasons keeps Seasons' for the hits and the bombs; its seeds and its Bombchus share one count, the Bombchus' of Seasons, which `ammo` keeps as it keeps the bombs; its prices are taken from the count Seasons keeps the ore chunks in, its shops' and the ore chunks' routines alike, and `rupees` makes them cost nothing.

A route recorded with cheats replays with them (`cheat-no-damage`, `cheat-ammo` and `cheat-rupees` in its options), and one recorded without replays without them whatever the settings; a savestate does not count them.

A cheat changed from the pause menu's Cheats, which starts from the cheats the game plays with, applies from the frame the game resumes on. In a game that ran without the hooks (Faithful with no other option that needs them), the first cheat turned on attaches them, and they stay until the game ends, every cheat off again or not: a check at each instruction, which a small device may feel. A route being recorded writes the change with its frame (`6120 cheat rupees on`, [`ROUTES.md`](ROUTES.md)) and replays it there.

The end-of-session report says what they did.

### The compact text

**The compact text** (Display's Text, `--text compact`, `text=compact` in the settings, classic by default, in Enhanced alone, Ages, Seasons and the three fan games; `--text` given to the home screen shows in the row, greyed, for the run): the text box draws its lines in the font's own widths, each letter as wide as its ink and a pixel, a space five pixels, instead of a cell of eight pixels a character, and the texts are laid out again for it.

Each box's two lines hold what their 128 pixels hold, about 18 characters instead of 12 to 14, so that a dialogue shows about a box in five fewer (from 16 % in Seasons' English to 24 % in Ages' French over all the texts, a few points more over the dialogues alone).

A page ends where the game ends it (`\stop`): its line breaks become spaces and its words fill its lines again, the fewest the width allows, breaking where a phrase ends or before a name in colour rather than within it; a word the game broke with a hyphen at a line's end is whole again, hyphenated again only where a line needs it. The pages that lay out a choice, a secret, a centred or aligned line, a song, a single box and the inventory's descriptions keep the game's layout.

The colours, the choices, the commands and the sounds are the game's, and the dialogue runs at the same characters per second, a character after another at the game's text speed (on a line of more than sixteen, a space comes with the character before it, and two narrow letters together if need be), each letter appearing whole when the game shows it, never cut, never early. A symbol or an item's icon, and a choice's cursor, take a whole tile.

English (US) and the five languages of the European ROM are laid out alike, the corrections and the port's own texts included, in the font of the image (a language's letters too).

The fan games are laid out in their English, their own texts and those they share with their Oracle, a box in five fewer too (from 16 % in Temple of Seasons to 21 % in Gifts of Kinomi over all the texts), their choices' cursor drawn where their engine draws it; a fan game started with a language (`--language` with `--patch`) refuses the compact text, its translation not being laid out.

It changes what the game draws and how many boxes it shows: never in Faithful, which shows the core's image as it is, where `--rom` refuses it and the home screen plays the classic text. A route recorded with it replays with it (`text-compact` in its options), and one recorded without replays without it whatever the setting: its keys follow the game's pages. A savestate taken with it loads with it only. The end-of-session report says how many lines it drew, and how many texts it laid out.

### The wide text box

**The wide text box** (Display › Text's Text box, `--text-box wide`, `text_box=wide` in the settings, standard by default, in Enhanced alone, Ages, Seasons and the fan games): the dialogues show in a box the port draws, 208 pixels wide instead of the game's 144, its lines 192 pixels of text instead of 128, the widest box every view of Enhanced holds (Near 4:3, 213 pixels across, keeps 2 on each side); still two lines a box, the game paging, pacing, sounding and choosing as it does.

The texts are laid out again for it, in either text:

- with the classic text (`--text classic`), in the game's own letters of eight pixels, 24 of them a line instead of 16, a box in five fewer in Ages' English (from 18 % in Seasons' English to 26 % in Ages' French over all the texts);
- with the compact text, in the font's own widths, about a box in three fewer (from 30 % in Seasons' English to 38 % in Ages' French).

In the world, the box goes where the game's would, centred across the view, the room drawn again under the game's own; over the game's menus, map and cutscenes, framed, it stands where the game's box does, centred on it and past the frame's edges, and the large menus (`menus=large`) take the frame and the box together, whether a box shows or not, at a scale that stays the same.

A line shows character by character at the game's pace, whole letters only, whatever its width. The pages the game lays out itself (a choice, a song, a centred line, a text of a single box) keep their lines, centred in the wide box as in the game's, a choice's cursor and an item's icon with them.

In Faithful, which shows the core's image as it is, there is no wide box: `--rom` refuses it and the home screen plays the game's. The fan games made on the Oracles have it as the Oracles do, their own English laid out again for it, the pages they keep as their engine laid them out centred in it.

A route recorded with it replays with it (`text-box-wide` in its options), one recorded without replays without it; a savestate taken with it loads with it only, the box's lines and its reveal kept in it.

A savestate refused for its text or its box names what loads it, to choose from the home screen:

- the text ("Refused: taken with the Compact text: choose Compact in Display's Text from the home screen"),
- the box ("Refused: taken with the Wide box: choose Wide in Display's Text box from the home screen")
- or both, and in Faithful the Enhanced profile, with the text and the box where those chosen are not the state's.

The classic text in the game's box, the default, is the game's text byte for byte: nothing is hooked for it.

### Text size

**Text size** (Display › Text's Text size, `--text-size normal|large|xlarge`, `text_size=` in the settings, normal by default, in Enhanced): the text box drawn larger, for the small screens (a handheld's, in the far view) and the large ones.

- **Large** is about three halves, in whole screen pixels so that it stays sharp: the game shown at k screen pixels a pixel, the box is drawn at k + ceil(k / 2) (six for four, three halves, on a 1080p screen in the far view; five for three; three for two; twice once on a 640x480 handheld's far view), over the frame by the presentation.
- **Very large** (`xlarge`) is twice as large, in the view itself: the game's box, 288 pixels wide, in the medium and far views; the wide box, 416 pixels wide, in the far view.

Each applies where the box so enlarged fits the view, the next size down elsewhere, without a word, since the view changes in play: on 1920x1080, Large shows the game's box in every view (eleven pixels for seven in Near) and the wide box from Medium on; on 640x480, Large shows both in the medium and far views of 4:3 and neither in Near. Centred across the view in the world, centred on the game's box over the framed menus and cutscenes, against the edge of the screen the game draws its box against.

With `menus=large`, the menus, the map and the cutscenes keep the scale they have with Normal: both sizes are drawn there by the presentation, the enlarged rectangle widened for the box only as far as that scale holds it, and a size whose box does not fit at that scale falls to the next one down (on 640x480, where the menus show three times as large, the box stays at that scale, 432 or 624 pixels wide).

The fan games have Text size too, on the game's box and on the wide box.

It is presentation alone: the game, its routes and its savestates do not change, nothing more is hooked (the classic text in the game's box stays the game's, its box's pixels enlarged), and a change applies at once, from the pause too, the image behind its menu included: to or from Very large where the view draws the box twice as large (the medium and far views), that image takes the box at its new size over the world as the frame showed it, the game not drawn again.

### Text speed

**Text speed** (Display › Text's Text speed, `--text-speed classic|instant`, `text_speed=` in the settings, classic by default, in Faithful and Enhanced, Ages, Seasons and the three fan games): the game shows a text letter by letter, at the speed chosen on its file screen and kept in its save, and a press of A or B shows the rest of the line at once, the next line whole too, a second press turning the page.

**Instant** shows each line whole, as a press of A at its start does, through the game's own text: the first page of a box whole at once, each page after it within a fifth of a second of the A that turns it, the game's scroll of the lines kept; A still turns the page, and a player used to pressing twice will turn two. The sound is the one a press gives, a sound a line instead of a sound a letter; a character's voice (a Goron's, a Tokay's, Ricky's) is kept, the line stopping a frame on it.

What the game holds is kept:

- the essences' texts (`\slow`) show letter by letter for their first two seconds, then whole;
- a choice's cursor comes after the game's delay, which keeps a press too many from choosing;
- a text that closes by itself stays its time.

The file screen's speed and the save are not touched.

It changes the game's pace, not its picture: Faithful has it, the text compact or classic, the box standard or wide. A ROM no profile recognises plays the classic text: Display › Text greys the row for it, on the home screen as in its pause, "Needs an original ROM.", and the summary beside Text leaves " · Instant" out.

In a game that ran without the hooks (Faithful with no other option that needs them), turning it on attaches them until the game ends: a check at each instruction, which a small device may feel.

A route recorded with it replays with it (`text-instant` in its options), one recorded without replays without it whatever the setting, and a change from the pause applies from the frame the game resumes on, written in a route being recorded (`9120 set text-speed instant`, [`ROUTES.md`](ROUTES.md)); a savestate loads with it on or off.

### Unvisited rooms

**Unvisited rooms** (Display › Picture's Unvisited rooms, `--unvisited-rooms shown|hidden`, `unvisited_rooms=` in the settings, shown by default, in Enhanced, Ages, Seasons and the three fan games): with `hidden`, the rooms around Link that he has never entered stay dark but for their edge, as [Unvisited rooms](PLAYING.md#unvisited-rooms) says.

Where a dark room meets a room drawn in clear, the dark one shows a soft edge along their border, inside it, a glimpse of that room as it is, its ground and whoever stands or moves there, people and enemies included:

- up to five pixels in full over a wavy line, then three pixels darkened, three darker, then the black, each limit following the wave, which is fixed to the world and never crawls as the camera moves;
- round a corner that it shares with a room in clear alone, the edge turns;
- between two dark rooms, and at the world's end, the black stays whole;
- a dark room whose picture is still being prepared when it comes into view takes its edge as it arrives, fading in like a room entered;
- the Low quality keeps the straight edge.

The room he is in is always shown, and the room a scroll takes him into from its first frames. A room he enters for the first time fades in from its dark look over sixteen frames, about a quarter of a second: during the scroll into it, the part the game's window shows is the game's own picture, already whole, while the rest of the room fades in beside it, so that an edge between the two slides with the window until the fade ends. After a warp, a time travel, a load or a savestate, the rooms visited are there at once.

A room the game shows Link in during one of its own cutscenes is one it does not mark: the view shows it as long as he is there and after he scrolls out of it, until the next warp or load, the map and the view after that keeping it dark until he enters it in play. A room visited in the other era of Ages, or in Holodrum for Subrosia, stays dark until he enters it in this one; the sea's floor and its surface count as one.

The rooms are prepared as before: the dark is the view's choice, never a room waiting to be ready. It is presentation alone: the game, its routes and its savestates do not change.

### Item hotkeys

**Item hotkeys** (`--item-hotkeys=off|use|equip`, in Faithful and Enhanced, originals only): four keys (`A`, `S`, `Q`, `W`; `x`, `y`, `leftshoulder` and `rightshoulder` on a controller, X, Y, LB and RB) act on four slots. Held with left Shift or left Ctrl, a key takes the item of B or of A into its slot; with the inventory open on the items, it takes the item under the cursor.

In `use` mode the key is one more item button: the item moves onto one of the two buttons the game reads in the current state, that button is held for the player as long as the key is, and the button's own item comes back after thirty quiet frames.

While Link holds a bomb or an object the bracelet lifted, the game reads no item button and throws on a new press of A or B: any slot's key then presses the button the holding item answers to, the bombs or the bracelet, nothing exchanged, and the object goes as with B; that press lasts until the game has read it, the key held or not, so that an item listening to the button held does not start, and a route notes it as a `throw` line, not as a use of the button's item.

In `equip` mode the key equips the item on its target, and pressed again brings the previous item back.

The game's equip sound (`snd_selectitem`, which the inventory plays as it equips) sounds through its own `playSound` for each item the `equip` mode equips or gives back and each item a key takes into its slot, in play or from the inventory open on its items (where it plays at once, the menu still open, as the menu's own equip does), never for a use of the `use` mode, whose item only borrows a button; it goes from the start of the next frame, and a route notes it as a `sound` line ([`ROUTES.md`](ROUTES.md#the-format)).

In Enhanced, a hotbar in the status bar's gutters shows the four slots: the item's icon as the game draws it, the key, and the button the item is on.

Where four slots of 38 pixels fit after four blank columns (Medium and Far, 16:9 and 4:3, Far fitted to the screen included; beside the bar laid out with **Wide layout**, two a side beside it otherwise) each slot takes the status bar's own look of B and A:

- the key's first character at the size of the bar's B and A, in their one-pixel style, one column clear of the opening bracket, as the bar leaves after its B;
- a second character (LB, RB) small under it, where a red badge with the button the item is on takes its place while it is there;
- then the item between two brackets of the bar's shape, spaced exactly as the bar spaces B's and A's around theirs, four blank columns kept from the money and from B, as the bar leaves between its B and its A, and three at least between two slots, as the bar would draw it on a button:
  - its sprites where the bar's OAM routine puts them (an item of one sprite at the left, not centred, Ages' harp eight pixels right);
  - over them the tiles `drawTreasureExtraTiles` (bank2.s) writes beside them, found from the item's `treasureDisplayData` entry (its display mode and the treasure it counts) and drawn from the bar's own tiles in the VRAM and its palettes: two digits for a quantity (bombs, Bombchus, each seed in the satchel, the shooter's or the slingshot's seeds, "00" at none), "L-" and a level, Ages' harp notes, Seasons' rod with the seasons had and the gloves' N;
- the value read as `checkTreasureObtained` reads it, at each frame (a flag and a byte), nothing for an item not had or one the game shows without (`$ff`);
- an item on B or A is checked against the bar's own block (`enhanced.hotbar_pictures_checked`, `enhanced.hotbar_picture_wrong`, none);
- dotted brackets for an empty slot, red after a refusal, a line under the icon blinking while waiting, the item on a button or not.

With **HUD Original**, the slots continue the bar on both sides, the columns between its right edge and the right pair in their background, the dark bands left beyond the outer slots alone. With **Wide layout** in Medium 4:3, that width puts the hearts on two rows while the hotbar shows; without the hotkeys the layout is as before.

Elsewhere (Near 16:9) the compact look: the icon, the key at its top right in small letters, the button on a badge under it, a frame for the states. In Near 4:3 the gutters are too narrow for it: no hotbar, the keys working all the same ([Item hotkeys](PLAYING.md#item-hotkeys)).

The icon is there from the start of play, the item never equipped yet: the view finds the sprites the status bar would draw it with as its own routines do (`loadEquippedItemSpriteData`, `loadItemIconGfx`), in `treasureDisplayData` and the icons' graphics of the player's ROM, at the item's level as the game holds it (a slot's seeds or song for the satchel, the shooter and the harp), found again when the bar reloads its icons (`loadEquippedItemGfx`, hooked as soon as the hotbar is shown) or a treasure is given.

An item on a button is checked against what the screen shows (`enhanced.hotbar_icons_decoded`, the distinct icons found, `enhanced.hotbar_icons_refound`, the times one was found again, `enhanced.hotbar_icon_wrong`, none).

Where the tables cannot say, the icon is the one the bar drew on a button, and a label until then; a dotted frame for an empty slot, a faded icon for an absent item, a frame that blinks while waiting and red after a refusal.

It stays as long as the game's status bar is on screen, a dialogue, a scroll and a room's load included, its icons fading and flashing with the bar's own; it goes with the bar (the title, the file select, the menus, a cutscene that clears the bar, the blank between two rooms), and never shows on the framed core F3 switches to, its loads and scrolls included.

The slots and the keys are remembered; the Controls line of a game turns them on (`use`) or off.

## Files, saves and savestates

### ROMs, patches and zips

**Cartridge** is the game's cartridge:

- the ROM (file, folder and status: original ROM, all profiles; unrecognised ROM, Faithful only; refused, with the loader's reason; none);
- **Choose ROM…**, which opens the system's file dialog (Windows', macOS'; on Linux the desktop's through its portal, or zenity's if installed; without one, a message suggests dropping the file on the window);
- the save, its date, and its two actions, **Open folder** and **Import save…** (Android: **Export save…** and **Import save…**; [Saves](PLAYING.md#saves)), which left and right move between;
- **Play**.

Choosing, dropping and remembering a ROM: [Cartridge](PLAYING.md#cartridge).

A ROM, a European ROM or a patch may also be a `.zip` that holds it, here, on the command line and on Android alike:

- a ROM is the one `.gbc` or `.gb` file in it, a patch the one `.bps` (a zip of a ROM and its patch serves as either), stored or deflated (the usual compression), read in memory, its other files (a readme, macOS's `__MACOSX` folder) left aside;
- a zip of several ROMs or of several patches, an encrypted one, a Zip64 one, another compression (bzip2, LZMA), or a zip or a file in it larger than 8 MiB (16 MiB for a patch) is refused with the reason, and so is a 7z or RAR archive, which the port does not read;
- the save goes beside the zip, under its name (`Oracle of Ages.zip`, `Oracle of Ages.sav`).

A file dropped is a patch rather than a ROM when its name ends in `.bps`, it begins as a BPS patch does, or it is a zip whose ROMs and patches are all `.bps` files, which its directory says without decompressing anything; a file it cannot tell (a zip that holds neither or whose directory cannot be read, a 7z or RAR archive, a file too large or that does not open) is taken as the patch on a fan game's page, where the patch's row says why it is refused, and as a ROM elsewhere.

On Android, a chosen file never overwrites anything: the same content under the same name is taken as it is, another content takes the first free name (`Name (2).gbc`), and the player's original is never touched.

### Saves

The save is written when it changes, about every five seconds and at exit, through a temporary file renamed afterwards.

### Savestates

F5 saves and F7 loads a state (`<save>.state`, beside the `.sav`). A state is composite: the core's state in its own format, the host's state (the camera, empty in Faithful), the mods' storage, the continuous transitions' progress and the host's lines of the text box (each empty when unused), and the versions of the format, the core, the game, the ROM and the active mods, gameplay options included.

A file made with another version, another ROM or other options is refused with a message that says which; one taken on the other core, Accurate (SameBoy) or Fast (mGBA), is refused by the core's name, the core to choose said on the standard error. Loading a state stops a route being replayed or recorded.

A savestate taken in far at another size than far's on this screen (a 1.1.0 savestate on a 1280x960 or 1240x1080 screen) loads with Display › Picture's Far size on Fixed; the game's own save loads either way.

## The launcher in detail

The paragraphs below describe the launcher's pages row by row, with their notes and their layouts; the manual's tables sum them up.

### The home screen

Without `--rom`, `the-oracles-project` opens the home screen in a 16:9 window of 1280x720 at the first opening, resizable down to 960x540, whose size is remembered: Oracle of Ages, Oracle of Seasons and Fan games, the chosen entry on the right with its title, its state (`Ready`, with the date of its save as the last session, `ROM not found`, or for a fan game whose Oracle's ROM is chosen, `Patch not found`, `Patch refused` when its patch is there but refused) and its menu, the two others on the left.

A window nearer 4:3 than 16:9 (a 640x480 handheld, 1024x768), or any window with `aspect=4:3` in the settings, takes the 4:3 layout, larger for a small screen:

- the three entries as tabs along the top, which left and right still change, the title alone and the menu on the right (the tab names the game), the motif's ring left of the menu;
- Cartridge, Display, Mods and Cheats lay their rows in a single column under the game's title, Display its diagram small at the top right, Advanced a button beside it, and the highlighted row's explanation once, under its rows, on one line with a note under it;
- Controls splits into three tabs beside its title, Buttons (the game's buttons and Reset to defaults), Hotkeys (the item hotkeys line and its slots) and In game (the fixed keys, Escape with the controller's menu button first, then F2 to F11, and Swimming), the tabs a row of their own above the grid, where left, right and Enter change the tab and down enters it, a key's capture saying its prompt in their place.

`aspect=16:9` keeps the 16:9 layout in any window.

Moving through the pages: [The home screen](PLAYING.md#the-home-screen).

The key hints at the bottom of every page, the pause menu's included, follow the device used last:

- after a key, the keyboard's keys alone (the arrows, Enter, Esc);
- after a controller's button or stick, the controller's alone (the D-pad, A, B, and the menu's button where Escape stands alone);
- after a click or a touch, and at the opening, both ("Enter / A").

Each key keeps the width of its widest label, so that the hints stay in place when the device changes.

### Display

**Display** sets, for every game the home screen starts:

- the profile, Faithful (160x144) or Enhanced, the wide world drawn back with the rooms around and continuous transitions;
- the quality, Low, Medium, High or Max, which sets at once what weighs on the device, the view, the core and the neighbour workers (Low: Near on the Fast core, Medium: Medium, High: Far, the three with the workers on Auto; Max: Far on the Accurate core with two workers), the quality changing to Custom, shown dashed and never chosen, when one of those is changed by hand, and greyed in Faithful;
- the window, 2x, 3x or 4x the profile's surface, or fullscreen at the screen's largest whole scale (a scale that does not fit the screen with the title bar is reduced at launch, and Display says so; where every choice comes to the same scale, as on a 640x480 screen, or on Android, which plays fullscreen, the row is greyed and does not change, and says the screen shows the game at one size only, which Scaling can make fill the screen);
- the view in Enhanced, Near, Medium or Far, each with its size in the screen's shape, the one the game will take: Far's fitted to the screen where far fits it (below, `far=`), 426x320 on 1280x960 and 412x360 on 1240x1080, which Enhanced's size, the windows, Scaling and the diagram follow too, and in the 4:3 layout View's help adds "Far fits this screen: 426x320 at 3x."; greyed in Faithful;
- the HUD in Enhanced, Original, Full width or Wide layout (`hud=`, below), greyed in Faithful, and from the pause in a game running in Faithful whatever the profile chosen;
- Text, which opens Display › Text as Advanced opens its rows and sums up beside it what applies ("Compact · Wide · Normal", "As the game" in Faithful, and " · Instant" after it when Text speed is Instant);
- Picture, which opens Display › Picture likewise and sums up beside it its scaling, margin and colors ("Sharp · No margin · Color off", then " · Far fixed", " · Large menus" (" · View menus" on Android) and " · Rooms hidden" after it where they apply and differ from the platform's default, named while they hold on the line, else counted: " · 2 more").

### Display's defaults and the slow-game suggestion

At the first opening Display is on Enhanced, fullscreen, with colour correction and continuous transitions on, vsync auto (in step with a display of 59 to 61 Hz on the desktops, off on Android, where it only added latency), the scaling on Sharp, Android included, and the device's quality: Max on the desktops, on Android, whose devices go down to small handhelds, High on a device of 8 processor threads and 6 GB or more, Medium otherwise; the item hotkeys start off.

After a game in Enhanced that ran slowly, more than 2 % of its frames late over a minute of play or more, the home screen suggests the quality just lighter in a toast at the bottom left ("The game ran slowly at High: Medium may play smoother."), or under Custom the view just nearer; nothing changes by itself. A click on the toast opens Display on that row, and the first key or button takes it away. It comes once for a quality (`quality_hint=` in the settings) until the view, the core or the workers change; the end-of-game report says it too, in one line.

### Display › Text

Display › Text has four rows and a preview:

- the text, Classic, the game's fixed-width letters, or Compact, each letter its own width (`text=`, below), its note saying that savestates taken with Compact need Compact;
- the text box, Standard, the game's, or Wide, the port's (`text_box=`, below), its note saying that savestates taken with Wide need Wide;
- the text size, Normal, Large or Very large (`text_size=`, below);
- the text speed, Classic, letter by letter at the speed chosen on the file screen, or Instant, each line whole (`text_speed=`, below).

The first three belong to Enhanced and are greyed in Faithful, "Enhanced only."; the text speed changes the game's pace, not its picture, and applies in either profile and to every game, so that the page opens on it in Faithful; on a fan game's page the four rows apply as on an Oracle's, a row its profile did not qualify greyed, "Ages and Seasons only for now."; from the pause, the text and the text box, which a game lays its texts out for when it starts, are greyed, "Change it from the home screen.", the text size applies at once and the text speed when the game resumes.

In the 4:3 layout the preview stands above the four rows, at most 314x236.

The preview draws the view the game will play in, in a frame at its proportions, the status bar at its top and the box at its bottom at the size it will take there (the text size falling back to the next size down where the view has no room for it, which the preview says), its text a sample of the port's own in the font of the game's ROM (the fan game's base ROM), or bars without a ROM.

The view fills its frame whatever the text size, and the box is drawn over it at its place and its proportion, sampled for pixel art as Fill samples the game, so that its letters stay sharp and grow with the text size. Where a pixel of the box would come under one screen pixel (Normal in Far at 1280x720, every size on a 640x480 screen), the box is drawn at one screen pixel a pixel and the frame shows the bottom of the view around it, the box centred, or from the frame's left where its text is wider than the frame (on a 640x480 screen, the box and little more).

### Display › Picture

Display › Picture, the diagram kept beside it, says how the game's picture shows:

- the scaling, Sharp, the image at its largest whole scale, every pixel exact, or Fill, the image as large as the screen allows in its proportions, sampled for pixel art (each pixel of the screen takes the colour of the game's pixel it covers, those astride two blending them, so that pixels stay sharp and look the same size), each choice with the image's size in fullscreen; Fill applies in fullscreen only, a window keeping its whole scales;
- the screen margin, Off, Auto, Small or Large (`screen_margin=`, below);
- colour correction, the three in either profile;
- Far size, Fitted or Fixed (`far=`, below), Far at a size made for the screen or at its usual size, a row only on a screen where they differ (1280x960, 1240x1080), its note giving both ("Here Fitted is 426x320 at 3x; Fixed is 480x360 at 2x.");
- Menus & scenes, View or Large (`menus=`, below), the game's menus, map, cutscenes and file screen at the view's scale or alone at their own largest scale, easier to read on a small screen, Large chosen at the first opening on Android and View on the desktops;
- Unvisited rooms, Shown or Hidden (`unvisited_rooms=`, below), the rooms around Link that he has never entered drawn or kept dark, Shown by default, from the pause applying when the game resumes ("Applies when the game resumes."), dimmed for the run where `--unvisited-rooms` gave it.

The last three in Enhanced, greyed in Faithful, "Enhanced only.".

### Display › Advanced

Advanced, under the diagram (beside it in the 4:3 layout) and after Picture for the keys (Enter or A, right, or a click), shows Display's Advanced rows in their place, Escape or B returning to Advanced:

- the core, Accurate (SameBoy), the reference, or Fast (mGBA), lighter, for small devices, a savestate loading only on the core it was taken on;
- continuous transitions, greyed in Faithful, which take Link swimming through them too;
- vsync;
- the neighbour workers, Auto, 1 or 2, the ghosts that prepare the rooms around in Enhanced, each on its own processor core, two filling the view faster after a warp or a load (Auto takes two on a device of four processor cores or more, one otherwise, and says which: "Auto · 2");
- the camera, Pixel or Smooth (`subpixel_camera=`, below), in Enhanced, greyed in Faithful, Smooth moving the camera by fractions of a pixel at the screen's resolution so that diagonal walks and stairs glide, Pixel keeping the game's whole pixels, the platform's default shown until one is chosen, with the note "No effect at this size: the view fills the screen at 1x." where the game is shown under twice its size.

### Controls

**Controls** sets the keys and buttons for every game: the eight buttons of the game on the keyboard and on a controller (A, B, Select and Start; the d-pad and the left stick always move), the four item hotkey slots and their two modifiers, and the item hotkeys line of the game shown. How a cell is bound: [Controls](PLAYING.md#controls).

Under them, the fixed keys in game begin with Menu: Escape, and the controller's button that opens the menu too (`menu_pad=`, below, L3 by default), whose cell takes only a button the game's buttons and the item hotkeys leave free, or a trigger (with the defaults: LT, RT, L3 or R3); B gives up waiting there. A cell of the grid never takes the menu's button, which gives up its waiting: change Menu first to give its button to another action.

Escape and F2 to F11 show once a key has been pressed, and on a desktop until a controller is used; after a controller's input with no key pressed yet, the menu's cell stands alone, the row on one line (in the 4:3 layout, Swimming moves up under it). On Android they show only once a key is pressed, never F11, and Back stands beside the menu's cell ("Menu · also Back" in the 4:3 layout). A controller's buttons are shown by their short names (A, B, X, Y, Back, Start, LB, RB, LT, RT, L3, R3).

**Reset to defaults** restores the grid, the hotkeys and the menu's button, and brings back the reminder and the pause menu's line below (`menu_tip=0`, `hotkeys_tip=0`).

With the item hotkeys of Ages or Seasons Off, their grid (Bind to B and Bind to A included) is drawn at half opacity, and a sentence under it says what On does: "Off: turn it on to use an item with one key, without opening the inventory."; a fan game, and an unrecognised ROM's pause, have none.

Each game started from the home screen shows a reminder naming the key that opens the menu, centred under the status bar whatever the HUD (16 lines of the game's image from its top, then a gap):

- "L3 opens the menu" after a controller's input, the menu's button by its short name as Controls writes it ("Guide opens the menu" without one, Guide alone opening it then; "Back opens the menu" on Android);
- "Esc opens the menu" after a key ("Back opens the menu" on Android, where a handheld's buttons may come as keys and there is no Escape);
- "The pause button opens the menu" after a touch on Android;
- "L3 or Back opens the menu" with a controller on Android.

It follows the device of the last input in the game, the home screen's before any.

It waits for the game's status bar in play for 120 frames in a row (two seconds; a flash of the bar before a cutscene does not start it): its map signs a layout the image draws, neither the bar nor the area under it one colour, outside the game's cutscenes (its cutscene index past 1), with no menu open (the inventory keeps the bar at its top) and no dialogue under way, and in Enhanced in the wide world, not a framed screen; a menu, a dialogue or a cutscene starts the wait again.

A game without a profile, whose bar is not known, counts every frame as in play: its reminder shows after the same two seconds, over whatever the game shows then; where no guest is attached (Faithful without the options that need one), the bar is read without hooks.

It shows about four seconds whatever is pressed, then fades out over 0.6 s, takes no input, and goes as the pause opens over it. It is the launcher's drawing over the presented frame: the game, its route and its savestates never see it.

`menu_tip=` counts the games it was drawn over; it comes back at each game until the player opens the menu by its key (Escape, the controller's menu button or Guide, the touch controls' pause, Android's Back; not the pause that Android's background brings), ten games at most. 1.3's `menu_toast=` (`due` or `shown`, the toast of the first game started with a controller) is read and dropped: the reminder shows again to those who saw it.

In the pause menu of Ages or Seasons on an original ROM with their item hotkeys off, a line under the list, "Item hotkeys are off · Controls › Item hotkeys", the list a line higher; it shows in the first three pause menus (`hotkeys_tip=` counts them as they open, but not the pause Android's background brings), goes once the hotkeys are on (turning them on in Controls sets `hotkeys_tip=off`), and is not there for a fan game, an unrecognised ROM, nor in a 16:9 window under 960 pixels wide, whose pause has no Controls.

### The Cheats page

**Cheats**, in the menu of Ages, Seasons and each fan game after Mods, turns each game's cheats on and off, a fan game's its own (`cheats_ages=`, `cheats_seasons=`, `cheats_kinomi=`, `cheats_moonrise=` and `cheats_temple=`, below): No damage, Ammo never runs out and Rupees never run out, each Off or On, with what it does beside it ("Buying costs nothing. Mods still charge.", a fan game, which has no mods, saying the first sentence alone; in the 4:3 layout the highlighted row's, under the rows); under the title, "Cheats play with your usual save: what you keep stays in it.".

With `--cheats`, which holds for the run, the page shows the cheats it gives, greyed, and changes nothing. From the pause, the page shows the cheats the game plays with, `--cheats`' included, and a change applies to it (below), the settings keeping the cheats the page shows.

An Oracle's menu and pause menu on a ROM that is not the original have no Cheats, its profile having none: their other entries are as they were.

### A fan game's Cartridge

Its Cartridge page asks for both:

- **Base ROM** is the ROM of its Oracle (Ages' for Gifts of Kinomi and Moonrise Regalia, Seasons' for Temple of Seasons), the same the Oracle's page shows and changes, chosen once for the Oracle and every game made on it;
- **Patch** is the game's `.bps` file, or a `.zip` of it (**Choose patch…**), read as a BPS patch whose checksums hold, and refused when it is the patch of another of these fan games, in the version recognized above, which makes that game from the Oracles' ROMs chosen ("Refused: the patch of Moonrise Regalia, not of Gifts of Kinomi."), the patch kept before it staying (another version, or a fan game whose Oracle's ROM is not chosen, is not told apart).

Below, its folder and the image the two make, built in memory as the session builds it: recognised, unrecognised (Faithful only), refused with the reason (a patch made for another base, another fan game's patch written in `settings.txt`), or "The game needs both files."

A patch dropped on a fan game's page becomes its patch, unless it is another of these fan games' (as above); dropped elsewhere, it goes to the fan game whose image it makes from its Oracle's ROM already chosen. The fan game's save and Open folder: [Fan games](PLAYING.md#fan-games).

### Cartridge's Save row

- **Import save…** ([Saves](PLAYING.md#saves)) opens the system's file dialog (Android's document picker), then asks over the page, "Replace this game's save? The current one is kept as a backup." with the file chosen and its folder (on Android, the folder of the shared storage the document picker says the file is in, `Download` for the downloads, and no folder line for a file whose picker names none, a cloud drive's): Replace or Cancel (for a game without a save yet, "Use this file as this game's save?", Import or Cancel) (left and right, Enter or A; Escape or B cancels).

  The file must be a save of the game: of Oracle of Ages for Ages, Gifts of Kinomi and Moonrise Regalia, of Oracle of Seasons for Seasons and Temple of Seasons, 8 KiB with one game file at least whose checksum holds. Any other file is refused, "This file is not a save of Oracle of Ages." (or of the fan game named), and nothing is replaced; a file larger than a save (a video or a disc image chosen by mistake) is refused so without being read.

  The save it replaces goes to a `Backups` folder beside it, under its name and the time (`Backups/Oracle of Ages (USA) 2026-10-20 14-22.sav`), never over another backup, and the game's savestate goes there with it (`Backups/Oracle of Ages (USA) 2026-10-20 14-22.state`), even without a save: a savestate holds the save it was taken with, which a Load state would bring back over the one imported, so Load state stays greyed until a savestate of the new save is taken.

  The row then says what happened, in place of its note, until the page is left, on two lines, that the save is imported, then each file that went to Backups ("In Backups, beside the ROM: the old save and its savestate.").

  The save a game with mods plays on (`NAME.mods.sav`, [Mods](PLAYING.md#mods)) goes to Backups too, with its savestate and the mods' storage (`.mods.state`, `.mods.store`), and when any of the three is there the confirmation says so ("Replace this game's save, with mods too? What it replaces is kept as a backup.") and the status names them ("In Backups: the old save and its savestate, with mods too, and the mods' data."): the next game with mods starts again from the save imported, as the first one did, so that the save brought in plays with and without mods.
- A save of the European ROM: the texts that German, Italian or Spanish make the US game read at the wrong place are signs, rings' names and dialogues. A fan game's save is copied as it is.
- A save made here also plays with the European ROM elsewhere: Cartridge's note says so under an Oracle's save; on Android it follows the action highlighted, Export save… with that note, Import save… with its own ("Import a save from another emulator or another device, also one made with the European ROM: its language is then set to English for this game.").
- **Open folder** and, on Android, **Export save…**: as [Saves](PLAYING.md#saves) says; Export save… is greyed while there is no save.

### The pause menu

**The pause menu.** Escape in a game the home screen started pauses it, as the controller's menu button and Guide do (below, Keys): the menu lies over the game's last image, darkened, with Resume, Save state, Load state ("from <time>" when a state exists, greyed with "No savestate yet" otherwise), Controls, Display, Cheats (for a game whose ROM has cheats) and Quit to launcher, which ends the session (save written, report on the standard error) and returns to the home screen. Its narrow windows and how it resumes: [The pause menu](PLAYING.md#the-pause-menu).

From the pause, a row acts at once, when the game resumes, or not at all:

- keys, colour correction, the HUD, the camera, the scaling, the screen margin, Menus & scenes, the window and the text size apply at once;
- vsync, Unvisited rooms, the text speed, Swimming, the item hotkeys and the cheats apply when the game resumes;
- the profile, the quality, the view, the core, continuous transitions, the neighbour workers, the far size, the text and the text box, which a running game keeps (it lays its texts out when it starts), are greyed and do not change, each saying "Change it from the home screen." (in the 4:3 layout on the help's note line), as a note under Display's title says once ("Greyed rows change from the home screen; the rest applies at once.").

The window changes while the menu shows, its scale lowered to what the screen holds as at Play:

- the row shows the window the game has, fullscreen or not as F11 or `--fullscreen` left it, with no size highlighted for a window that is none of them (F11 out of a game started fullscreen keeps the home screen's window), and a size chosen there applies whatever the settings held, which store the window only once the row is chosen;
- it greys the sizes whose window would leave the pause too narrow for Controls and Display (Near's 2x and 3x, Medium's 2x, Faithful's 2x and 3x), which left and right step over, saying "Smaller windows change from the home screen.", or after a reduced window's note "Reduced to 3x to fit this screen; smaller ones from the home screen.";
- a page whose window is made narrow by hand while it is open comes back to Resume.

Swimming, the text speed and the cheats apply from the frame the game resumes on (a page being revealed finishes whole there, as after a press of A); the item hotkeys turned on from that frame too, their keys and the hotbar with them, and turned off from the first frame their policy is at rest, no exchange under way and no button's item still to come back, so that a button never keeps a hotkey's item, their keys doing nothing from the resume.

A fan game shows greyed from its pause what its profile does not have, the item hotkeys and, in Moonrise Regalia, Swimming's hold, "Ages and Seasons only for now." in place of the lines that say fan games ignore or refuse them. An Oracle whose ROM no profile recognises has neither either: in its pause the two rows are greyed and inert, saying "Needs an original ROM.".

`--swim-hold`, `--text-speed`, `--item-hotkeys`, `--scale`, `--fullscreen` and `--vsync` hold until the pause changes their row, as `--colour-correction` does; a savestate loads with the swim's hold, the item hotkeys and the window as they are when it loads, none of them being in its identity, and a load lets an item hotkeys' exchange under way go, as it always did.

The colour correction shows from the game's next frame, the image behind the menu staying the last one the game drew; the HUD, Menus & scenes and the text size change that image at once, a text box Very large where the view draws it twice as large itself (the medium and far views) included, drawn again at the new size over the world as the frame showed it; Display's Advanced rows open on the first row that changes there, vsync, and Display › Text on Text size.

The end-of-game report says the scaling in effect at the last frame. The pause holds the session between two frames: the core does not run, no frame is counted, and a route being recorded contains none.

### Keys and buttons

Keyboard by default: arrows, X = A, Z = B, Enter = Start, Backspace = Select; F2 colour correction, F3 wide world or framed core, F5 save state, F7 load state, F11 fullscreen, Escape quit (the pause menu for a game the home screen started).

Controller by default: d-pad or left stick, A, B, Start, Back = Select (on Android, Android's Back opens the pause menu, a handheld's included: [Android](PLAYING.md#android)), the left stick's click (L3) = Escape, the pause menu, as Guide (the Home button) is too wherever the system gives it to the application (with `--rom`, which has no pause menu, neither does anything); every controller plugged in plays, several at once included (a handheld whose system counts another input as a controller, a phone with a Bluetooth pad).

Key names are SDL's (`Right`, `X`, `Return`, `Space`, `F1`...), button names SDL controllers' (`a`, `b`, `x`, `y`, `back`, `start`, `leftshoulder`...); on a Nintendo controller `a` is the button labelled A. An unknown name is reported and replaced by the default.

The menu's button is `menu_pad=` in the settings, `leftstick` by default: a button, or a trigger, `lefttrigger` or `righttrigger`, pressed past half its travel and let go below a quarter, the same trigger of several controllers read as one, let go once none holds it (on a handheld without sticks, often the only inputs the game leaves free), or empty for Guide alone.

A button the game's buttons, the d-pad or an item hotkey slot have stays theirs: when the settings are read, the menu is left without one, Guide alone opening it, Controls' Menu cell showing none and the first game's toast not shown, and the standard error says so; an unknown name is the default's, `leftstick`.

The menu's button never reaches the game, nor does Guide unless one of the game's buttons is bound to it; on the home screen and in the pause they are Back, as Escape is. Each press that opens the menu is written on the standard error with its button and its controller, for a device whose buttons are not those expected.

### Android

- The mods folder is `Android/data/org.theoraclesproject/files/mods`, in the application's folder on the shared storage beside `roms` and `patches`, reachable from a computer over USB, since most file managers cannot open `Android/data` from Android 11 on (not beside `settings.txt`, which is in the application's private storage): each mod goes there in a folder of its own, as on a desktop. The Mods page shows that path, its line under it says it is reachable over USB, and Open folder makes the folder and says where it is. A game with mods plays on its own save beside the ROM, as on a desktop.
- Android's Back button is the launcher's Escape: back in the home screen, the pause menu in a game, a capture cancelled in Controls; that is so whatever device it comes from, the system's navigation, a keyboard, or a handheld's built-in controller (a Retroid Pocket's Back opens the pause menu). A controller's select button (`back` in SDL's names) stays the game's Select.

  A controller that Android reports without a Select of its own (one it has no key layout for, whose select button it may report as Back) keeps its Back as the game's Select, as before: its left stick's click or its Guide opens the menu. A controller's left stick's click (`menu_pad=`) opens the pause menu, as its Guide or Home button does where Android does not keep it for itself.

  The log says, for each controller at its first key, whether Android reports a Select and so what its Back does (`controller <name>: has Select, its Back opens the menu`, or `no Select, its Back is the game's Select`), and each Back that opens the pause menu with the device it came from.
- Choose ROM and Choose patch open Android's document picker; the chosen file is copied, under its name, into `Android/data/org.theoraclesproject/files/roms` (or `patches`), where its save and savestates go too, reachable over USB. Cartridge shows that folder from `Android/data`, as a computer sees it over USB (`Android/data/org.theoraclesproject/files/roms`).

  A chosen file never overwrites anything: the same content under the same name is taken as it is, another content takes the first free name (`Name (2).gbc`), and the player's original is never touched.
- The system bars stay hidden: a swipe from the screen's edge shows them for a moment, and they hide by themselves; bars that Android leaves shown (after the notification shade, a system dialog, a return to the application) are hidden again when the application has the focus back, or three seconds after they showed. Each time is written in the log, `system bars hidden again`, a line every ten seconds at most.
- What the launcher reports, the end-of-game report on late frames and audio underruns included (written when a game is left through its pause menu), goes to `Android/data/org.theoraclesproject/files/the-oracles-project.log`, reachable from a computer over USB; the previous run's is kept beside it as `the-oracles-project.previous.log`. It is the file to send with a report of a game that stutters.
- Touch controls ([Android](PLAYING.md#android)): Select and Start at the bottom centre, each control lighter while pressed, the d-pad keeping a finger that slides off it.
- A screen whose edges hide part of the picture, the game's status bar or the top of its inventory cut off without any system bar showing: `screen_margin=small` (24 dp clear at the top and at the bottom), or `large` (48 dp) where the picture is still cut. Where Android reports the hidden band itself (a display cutout or a curved edge at the top or the bottom), `auto` is enough and takes no more than it.

  The log's `home screen opened` and `game started` lines say the margin, what it was read from (the cutout's insets, the pixels a dp) and the band it leaves, as `screen margin small (cutout insets 0 above, 0 below; 2.63 pixels a dp): band 0,63 2400x954`.
- A savestate taken in far at another size: as [Savestates](#savestates) says.
- The application declares fragile user data: at uninstall, Android offers to keep its data, ROMs, patches, saves and mods included.

### About

**About.** The version number at the bottom left of the home screen opens About ([About and privacy](PLAYING.md#about-and-privacy)); while it is highlighted an outline frames it, the number staying in place, and the key hint Select says Open. It reacts on the home screen only.

About is laid out as Cheats: "The Oracles Project" for its title, the version and the notice ("v1.3.0 · 1.4.0 available") under it, and one row, Update check, On or Off (`update_check=`, above), "On: the launcher asks GitHub once at launch whether a newer version is out. Off: no network request." (in the 4:3 layout "On: one request to GitHub at launch." and "Off: no network request." under the row). Off hides the notice at once and is written at once; On asks at the next launch. Escape or B returns to the home screen, on the version number.

## Log lines

The launcher writes what it reports on its standard error: the terminal it runs from on a computer (on Windows, the console window that opens with it); on Android, Android's log (`adb logcat -s the-oracles-project`) and `Android/data/org.theoraclesproject/files/the-oracles-project.log`, the run before kept as `the-oracles-project.previous.log`. Besides the lines named with each option above:

- At exit the launcher reports the late frames and, for audio, the underruns (silences heard); the core produces slightly more or fewer samples per second depending on the queue's fill, so that it never drops or starves.
- On Android, the log says, for each controller at its first key, whether Android reports a Select and so what its Back does (`controller <name>: has Select, its Back opens the menu`, or `no Select, its Back is the game's Select`), and each Back that opens the pause menu with the device it came from.
- On Android, the system bars that Android leaves shown (after the notification shade, a system dialog, a return to the application) are hidden again when the application has the focus back, or three seconds after they showed. Each time is written in the log, `system bars hidden again`, a line every ten seconds at most.
- The log's `home screen opened` and `game started` lines say the margin, what it was read from (the cutout's insets, the pixels a dp) and the band it leaves, as `screen margin small (cutout insets 0 above, 0 below; 2.63 pixels a dp): band 0,63 2400x954`.

## Languages in detail

**Languages.** The European ROMs of Ages and Seasons (`(Europe) (En,Fr,De,Es,It)`) hold the game's texts in English, French, German, Italian and Spanish, under the identifiers of the US texts.

With `--language` the US ROM plays in one of them: the texts are extracted once from the European ROM into a pack (a few hundred kilobytes, made on the player's machine, never shipped), composed into the image beside the game, and served to the game's text engine, which lays them out, pages them and prints them as its own; the European font's 35 glyphs that the US font lacks or draws otherwise (the German sharp s, the Spanish inverted marks, more accented letters) are written over it. Every text is translated.

Where the European version moved a text, the pack takes it from its new place; where it needs the European game's code (a count the US code does not set, a shop laid out otherwise), a rule rebuilds it at extraction from Nintendo's words in the US text's commands.

The few French lines of Ages whose European text says something else than the US one (the rounds cleared, the Goron who cannot forge the Biggoron's Sword, a secret's title) are the port's own, unofficial translations, `config/language_own.tsv`. `config/language_exceptions_ages.tsv` and `config/language_exceptions_seasons.tsv` name these texts.

Where the official translation is wrong (a place for another, a wrong word, an item named several ways), the extraction corrects the word or the phrase, `config/language_corrections.tsv`.

The graphics that carry text are the language's too: the file select and its copy, erase, new file, secret, link and name entry screens, the message speed, the save menu, the game over and "to be continued" screens, the dungeons' names and the inventory's save label, as the European game draws them in that language (34 graphics in Ages, 33 in Seasons, taken into the pack). The title screen and the credits stay as the US game has them, the European ROM having no other version of them in the languages.

**English (EU)** (`--language en-eu`) is the European edition's English: a revision of the US text, with its own fixes and wording, served as the translations are, with the same corrections where it kept a mistake of the US text (the Green Holy Ring's electric damage, the Rock Brisket the inventory called Rock Sirloin, a few misspellings), and its own versions of the file select, the save menu, the game over and the secrets' screens (22 graphics in Ages, 21 in Seasons; the dungeons' names are the US ones). English (US) (`en`) stays the cartridge's own text.

From the home screen, the language is chosen in the game's Cartridge, its Language row after European ROM: English (US), English (EU) and the four translations, each named in its own language, those after English (US) once the European ROM is recognized; the explanation under them says which text plays, and the home screen names the language beside Cartridge (`language_ages=` and `language_seasons=` in `settings.txt`). Without `--language` the image and the game are the US ROM's, untouched.

A savestate taken in a language loads only in it, and with the same pack: one extracted again from other tables refuses it. A pack made by another version of the port from other tables, or damaged, is extracted again when the European ROM is there; one that cannot be written (a full disk) serves all the same, extracted again at the next start, and the game says so. The end-of-session report says the language, its pack and the texts shown.

**The fan games in a language.** Moonrise Regalia, Gifts of Kinomi and Temple of Seasons keep most of their Oracle's texts word for word. With `--language` and `--eu-rom` (the European ROM of Ages for the first two, of Seasons for Temple), every text of the fan game that is a US text takes that text's official translation, as the Oracle plays it, and the fan game's own texts stay in its English: in each language Moonrise translates 2,567 of its 2,838 texts, Kinomi 2,457 of 2,953, Temple 1,558 of 1,906. A scene can so mix the two languages where the fan game rewrote some lines and kept others.

The pack is made on the player's machine from his European ROM and the fan game's image, as the session builds it, and nothing of either is shipped. The fan game's graphics are its own and keep their English (the file select, the save menu).

These counts are of the texts the fan game's image holds, not of those it shows: an image keeps texts of its Oracle that the fan game may never show, while its own dialogues, its story, stay in English. So the home screen starts the fan games in English for now, until their own texts are translated too: they have no Language row, and `language_moonrise=`, `language_kinomi=` and `language_temple=` in `settings.txt` are written but not applied.

A fan game plays in a language from the command line, with `--rom`, `--patch`, `--language` and `--eu-rom`.
