# Playing

You need your own ROM of Oracle of Ages or Oracle of Seasons (US). The port recognises the original US images by their SHA-1 and plays them in every profile; another image whose header is an Oracle's (a hack, a randomizer seed) plays in Faithful only, without the Enhanced view or the gameplay options.

```bash
build/the-oracles-project                                     # the launcher's home screen
build/the-oracles-project --rom "/path/to/Oracle of Ages.gbc" # the game straight away, without the home screen
```

## The launcher

Without `--rom`, `the-oracles-project` opens the home screen in a 16:9 window of 1280x720 at the first opening, resizable down to 960x540, whose size is remembered: Oracle of Ages, Oracle of Seasons and Fan games, the chosen entry on the right with its title, its state (`Ready`, with the date of its save as the last session, `ROM not found`, or for a fan game whose Oracle's ROM is chosen, `Patch not found`) and its menu, the two others on the left. A greyed item gives its reason when highlighted. Arrows, Enter and Escape on the keyboard, the d-pad, A and B on a controller, hover and click with the mouse; F11 toggles fullscreen.

**Cartridge** is the game's cartridge: the ROM (file, folder and status: original ROM, all profiles; unrecognised ROM, Faithful only; refused, with the loader's reason; none), **Choose ROM…**, which opens the system's file dialog (Windows', macOS'; on Linux the desktop's through its portal, or zenity's if installed; without one, a message suggests dropping the file on the window); the save, its date, and **Open folder**; **Play**. The ROM is remembered per game. A ROM also comes by dropping its file on the window: on a game's Cartridge page it counts as Choose ROM… for that game; on the home screen it is identified and filed under its game. A refused ROM shows its reason and is not remembered.

**Display** sets, for every game the home screen starts: the profile, Faithful (160x144) or Enhanced, with continuous transitions and the smooth camera; the window, 2x, 3x or 4x the profile's surface, or fullscreen at the screen's largest whole scale (a scale that does not fit the screen with the title bar is reduced at launch, and Display says so); the view in Enhanced, Near, Medium or Far, each with its size in the screen's shape, greyed in Faithful; colour correction; continuous transitions, greyed in Faithful, which take Link swimming through them too. Advanced, under the diagram and after the transitions for the keys (Enter or A, right, or a click), shows Display's Advanced rows in their place, Escape or B returning to Advanced: the core, Accurate (SameBoy), the reference, or Fast (mGBA), lighter, for small devices, a savestate loading only on the core it was taken on; vsync; the neighbour workers, Auto, 1 or 2, the ghosts that prepare the rooms around in Enhanced, each on its own processor core, two filling the view faster after a warp or a load (Auto takes two on a device of four processor cores or more, one otherwise, and says which: "Auto · 2"). At the first opening Display is on Enhanced, Far, fullscreen, with colour correction and continuous transitions on, vsync auto, the workers auto and the core of the platform, Accurate on the desktops and Fast on Android, whose devices go down to small handhelds; the item hotkeys start off. The sizes and the diagram are those of what will play for the game shown, on the screen the window is on. The choices apply at the next Play.

**Controls** sets the keys and buttons for every game: the eight buttons of the game on the keyboard and on a controller (A, B, Select and Start; the d-pad and the left stick always move), the four item hotkey slots and their two modifiers, and the item hotkeys line of the game shown. Enter on a cell waits for a key (or a button, in a controller column), Escape cancels; a key already bound elsewhere on the same device is taken from there and the other cell stays empty. **Reset to defaults** restores the grid and the hotkeys. Everything is written when it changes.

**The pause menu.** Escape in a game the home screen started pauses it: the menu lies over the game's last image, darkened, with Resume, Save state, Load state ("from <time>" when a state exists), Controls, Display and Quit to launcher, which ends the session (save written, report on the standard error) and returns to the home screen. Under 960 pixels wide, Controls and Display are left out. Escape or B resumes. From the pause, keys and colour correction apply at once, the rest (profile, window, view, transitions, vsync) at the next Play, as a note under Display's title says; Display's Advanced rows show the game's core and neighbour workers, dimmed and inert, since a game keeps those it started with. The pause holds the session between two frames: the core does not run, no frame is counted, and a route being recorded contains none.

## Fan games

The Fan games entry lists, in alphabetical order, Gifts of Kinomi 1.1.2 (built on Ages), Moonrise Regalia 1.0.6 (built on Ages) and Temple of Seasons 1.073 (built on Seasons), each with its state; chosen, a fan game has the menu of a game, and Escape or B returns to the list. A fan game is distributed as a BPS patch: you keep the original ROM it is built on and the patch.

Its Cartridge page asks for both: **Base ROM** is the ROM of its Oracle (Ages' for Gifts of Kinomi and Moonrise Regalia, Seasons' for Temple of Seasons), the same the Oracle's page shows and changes, chosen once for the Oracle and every game made on it; **Patch** is the game's `.bps` file (**Choose patch…**), read as a BPS patch whose checksums hold; before one is chosen, the page names the patch the game expects ("No patch: the BPS patch of Moonrise Regalia 1.0.6 is expected"). Below, its folder and the image the two make, built in memory as the session builds it: recognised, unrecognised (Faithful only), refused with the reason (a patch made for another base), or "The game needs both files." A patch dropped on a fan game's page becomes its patch; dropped elsewhere, it goes to the fan game whose image it makes from its Oracle's ROM already chosen. The save goes beside the patch, under its name (`Moonrise Regalia 1.0.6.sav`), not beside the base ROM, which the Oracle's own save uses; Open folder opens the patch's folder.

The three fan games play in Faithful and Enhanced, with continuous transitions and the drawn-back view. The item hotkeys are refused where a game's profile does not qualify them, which is the case of the three today: Controls shows them off for a fan game. Mods play on the original US ROMs only.

From the command line, `--rom BASE --patch PATCH.bps` applies the patch in memory at each launch, never writes the image, and refuses with the reason a patch made for another base, cut short or damaged (the format's three CRC32 are checked).

## Mods

**Mods**, in the menu of Ages and of Seasons, lists the mods that add houses, characters and minigames to the game ([`MODDING.md`](MODDING.md)); a fan game's menu greys it ("Mods play on the original ROMs"). Beside it, the menu says how many are active ("2 active").

The mods are in the **mods folder**, beside `settings.txt`: `~/.local/share/the-oracles-project/mods/` on Linux, `~/Library/Application Support/the-oracles-project/mods/` on macOS, `%APPDATA%\the-oracles-project\mods\` on Windows. Each mod is a folder of its own inside it, named with a–z, 0–9, `-` and `_`, with a `main.lua` in it. **Open folder**, at the top of the page, makes the folder if it is not there yet and opens it in the system's file manager. The two examples of the repository, [`mods/claw-game`](../mods/claw-game/) and [`mods/fortune-teller`](../mods/fortune-teller/), are what a player copies there, each folder as it is.

The page lists each mod found, in the order of their names, which is the order they play in: a switch, its name, the game it adds houses to ("Houses in Ages and Seasons"), and its description, or why it is refused for this game ("Refused: " and the loader's reason), a refused mod staying off. Left and right, Enter or a click switch the highlighted mod; eight at most are active together. The choice is kept in `settings.txt` at once, per game. The folder is read again each time the page opens.

**Play**, at the bottom of the page, starts the game with the active mods, on a save of its own: `NAME.mods.sav` beside the ROM, copied from the game's save the first time; the save without mods is never written by a game with mods. **Start game**, in the menu, plays the game without mods, on its usual save. Play is greyed without a ROM ("Choose a ROM in Cartridge first") and on a ROM that is not the original US one ("Mods play on the original ROMs").

To try the examples: copy `mods/claw-game` and `mods/fortune-teller` into the mods folder, open Ages' Mods page, switch both on, and Play. The claw game's house is on the big island in the south of Lynna City, by the bridge, and the fortune teller's on the island east of the red house; the fortune teller counts Link's visits, kept with the save in `NAME.mods.store`.

## The command line

`--rom` starts a game without the home screen; Escape then ends the session. Without options it plays in Faithful, in a window at scale 4, whatever Display chose; colour correction and vsync are the settings'. Options:

| Option | Effect |
| --- | --- |
| `--save PATH` | the save file; by default the ROM's path with `.sav` (a raw SRAM image, compatible with other emulators) |
| `--patch PATCH.bps` | a fan game's patch, applied in memory to `--rom` |
| `--enhanced` | the Enhanced profile, 256x144 |
| `--zoom-out` | the drawn-back view, 480x270, or 480x360 on a 4:3 screen (implies `--enhanced`); `--fullscreen --scale 4` fills 1920x1080 exactly; the same as `--view far` |
| `--view near\|medium\|far` | the Enhanced view's level (implies `--enhanced`): how much of the world it shows, in the screen's shape (4:3 or 16:9), in fullscreen as in a window |
| `--aspect auto\|16:9\|4:3` | the view's shape: the screen's (`auto`), or the one named whatever the screen; `aspect=` in the settings, which Display does not show |
| `--ghosts auto\|1\|2` | Enhanced: the ghosts preparing the rooms around at once, each on its own processor core, for this run; `ghosts=` in the settings, `auto` (the default) two on a device of four processor threads or more, one otherwise. Two fill the view faster after a warp or a load (the time travels of Ages); the log says the count at the game's start. Display's Neighbour workers, in its Advanced rows, set `ghosts=` |
| `--core sameboy\|mgba` | the core the game runs on: SameBoy, Accurate, the reference, or mGBA, Fast, lighter, for small devices; `core=` in the settings; a savestate loads only on the core it was taken on |
| `--continuous-transitions` | Link keeps walking through the scrolling transitions (implies `--enhanced`) |
| `--continuous-swim` | the same, and Link keeps swimming through them too (implies `--continuous-transitions`); the home screen's choice turns it on |
| `--item-hotkeys=off\|use\|equip` | the item hotkeys for this run |
| `--camera 1\|2` | the Enhanced camera's profile |
| `--mods DIR` | a mod, repeated for several ([`MODDING.md`](MODDING.md)) |
| `--record ROUTE`, `--play ROUTE` | record the session's inputs, replay them ([`ROUTES.md`](ROUTES.md)) |
| `--scale N`, `--fullscreen` | the window |
| `--colour-correction on\|off`, `--vsync auto\|on\|off`, `--mute` | image, pacing, sound |
| `--frames N`, `--no-window`, `--screenshot PATH.ppm` | quit after N frames; no window and no pacing; the last frame as an image at exit |

A gameplay option given without `--rom` (`--continuous-transitions`, `--item-hotkeys`, `--enhanced`, `--zoom-out`, `--view`, `--aspect`, `--core`, `--camera`) overrides Display and the settings for the games the home screen starts. `--save`, `--frames`, `--no-window`, `--screenshot` and `--play` belong to the game `--rom` starts and are refused without it. `--record ROUTE` without `--rom` records the first session the home screen starts. `--vsync`, `--colour-correction`, `--camera`, `--aspect` and `--core` hold for the run they are given in and never change the settings file.

On Windows the game's window is sized in pixels, so the scale is the one asked at 125 % or 150 % too; the launcher's window keeps its size in points. On macOS the windows ask for a Retina display's pixels: two pixels per point.

## Profiles and options

**Faithful** shows the core's image, 160x144: the game as it is.

**Enhanced** shows a wide surface, 256x144: the status bar in a band of sixteen lines at the top, centred, and below it the world, the play area placed by a smooth camera in world coordinates with the neighbouring rooms drawn around it. The neighbours are computed by the game itself in a second instance of the core; a neighbour not ready yet is black, never a wrong terrain. Large rooms (dungeons) are shown whole, centred, their scrolls sliding the band; interiors of one room are centred and still. Menus, cutscenes and the file select show the core's image framed: at the view's scale, or alone at its own largest whole scale, as Faithful shows it, with `menus=large` in the settings, the default on Android, where the view's scale on a small screen makes them hard to read (`menus=view` keeps the view's). F3 toggles between the wide world and the framed core. The camera has two profiles (`--camera`, `camera=` in the settings): 1 waits for Link to cross a dead zone of sixteen pixels and catches up; 2, the default, follows Link continuously and comes to rest without stepping back.

**The drawn-back view** (`--zoom-out`) gives the Enhanced surface 480x270: at scale 4 it fills a 1080p screen. On a 4:3 screen it is 480x360, the same three rooms across with 90 lines more of the world. Outdoors the camera frames three rooms across and two down (nearly three in 4:3), Link on the middle line; a large room is shown whole, centred; elsewhere the normal band stays, centred. A savestate taken at one size is refused at the other.

**The view's levels** (`--view`, `view=` in the settings): the Enhanced surface is near (256x144), medium (384x216) or far (480x270, the drawn-back view), each in the screen's shape: on a 4:3 screen near is 213x160, medium 320x240 and far 480x360 (on a 640x480 screen, three, two and one times; far fills 960x720 twice and 1440x1080 three times). The shape is the screen's, nearer 4:3 or 16:9, unless `aspect=16:9` or `aspect=4:3` in the settings, or `--aspect` for one run, names one whatever the screen (a 4:3 view tried on a wide screen, or a screen whose shape is misread). A window takes the surface's shape. Medium draws back as far does: two rooms or more across outdoors, a large room whole. Near in 4:3 is narrower than a large room's 240 pixels and shorter than its 176 lines: the band follows the game's window across and up and down, inside the room, all its 144 lines shown; a room of one screen (most interiors and dungeon rooms) shows the game's 128 lines, the band black above and below them. The farther the view, the more work for the device: on a small handheld, medium plays where far may not hold the frame rate. Far is the default.

**Continuous transitions** (`--continuous-transitions`, both games): in the wide view the game's own scroll no longer shows, and what remains of a transition is Link frozen for about forty frames. The option doubles the scroll's step and moves Link a pixel a frame in the transition's direction, his walk animated, so he keeps walking; beyond the pixels the game moves him itself, he only walks onto tiles without collision. The game's state then differs from a native session's: a route recorded with the option replays with it, and a savestate taken with it is refused without it.

**The swim through them** (`--continuous-swim`, which Display's continuous transitions turn on; a separate option so that the routes recorded with `--continuous-transitions` alone replay as they were played): Link swimming at the surface, in both games, flippers or Ages' mermaid suit, diving or not, keeps swimming at his own speed through the transition, his swimming animation going on, and beyond the pixels the game moves him itself only into water; on Ages' sea floor he goes on at his own speed too, onto tiles without collision. A stroke of the flippers that carries him to an edge takes the transition the game would take with the direction held, when the direction held neither points to the edge nor away from it; the mermaid suit, driven by the direction pad, keeps the game's behaviour. With the option, Link is also held back from a wall leftward and upward, and walks onto stairs and along a bridge. A savestate does not count the swim: a state taken with it loads without it.

**Item hotkeys** (`--item-hotkeys=off|use|equip`, in Faithful and Enhanced, originals only): four keys (`A`, `S`, `Q`, `W`; `x`, `y` and the two triggers on a controller) act on four slots. Held with left Shift or left Ctrl, a key takes the item of B or of A into its slot; with the inventory open on the items, it takes the item under the cursor. In `use` mode the key is one more item button: the item moves onto one of the two buttons the game reads in the current state, that button is held for the player as long as the key is, and the button's own item comes back after thirty quiet frames. In `equip` mode the key equips the item on its target, and pressed again brings the previous item back. In Enhanced, a hotbar in the status bar's gutters shows the four slots: the item's icon as the game draws it, the key, and the button the item is on; a dotted frame for an empty slot, a faded icon for an absent item, a frame that blinks while waiting and red after a refusal. In near 4:3 the gutters are too narrow for it: no hotbar, the keys working all the same. The slots and the keys are remembered; the Controls line of a game turns them on (`use`) or off.

## Keys

Keyboard by default: arrows, X = A, Z = B, Enter = Start, Backspace = Select; F2 colour correction, F3 wide world or framed core, F5 save state, F7 load state, F11 fullscreen, Escape quit (the pause menu for a game the home screen started). Controller by default: d-pad or left stick, A, B, Start, Back = Select; every controller plugged in plays, several at once included (a handheld whose system counts another input as a controller, a phone with a Bluetooth pad). Key names are SDL's (`Right`, `X`, `Return`, `Space`, `F1`...), button names SDL controllers' (`a`, `b`, `x`, `y`, `back`, `start`, `leftshoulder`...); on a Nintendo controller `a` is the button labelled A. An unknown name is reported and replaced by the default.

Colour correction off is the raw RGB555 conversion; on, SameBoy's "modern balanced" rendering, close to an original screen. `vsync auto` presents each frame at the display's rate when it refreshes between 59 and 61 Hz, close enough to the Game Boy's 59.7275 Hz for the audio rate control to absorb the difference; on another display, or with `off`, the host paces at 59.7275 Hz. Under vsync the host checks that the presentation waits for the display, and if it does not, turns vsync off and paces itself for the rest of the session.

## Savestates

F5 saves and F7 loads a state (`<save>.state`, beside the `.sav`). A state is composite: the core's state in its own format, the host's state (the camera, empty in Faithful), and the versions of the format, the core, the game, the ROM and the active mods, gameplay options included. A file made with another version, another ROM or other options is refused with a message that says which; one taken on the other core, Accurate (SameBoy) or Fast (mGBA), is refused by the core's name, the core to choose said on the standard error. Loading a state stops a route being replayed or recorded. There is no savestate during a mod's conversation.

## Settings

The player's settings are `settings.txt` in the user's settings directory (on Linux `~/.local/share/the-oracles-project/`, on macOS `~/Library/Application Support/the-oracles-project/`, on Windows `%APPDATA%\the-oracles-project\`; `ORACLES_SETTINGS_DIR` names another), written with all its values at the first launch so that it can be edited:

```
colour_correction=1
vsync=auto
camera=2
rom_ages=/path/to/Oracle of Ages.gbc
rom_seasons=
patch_kinomi=
patch_moonrise=/path/to/Moonrise Regalia 1.0.6.bps
patch_temple=
item_hotkeys_ages=off
item_hotkeys_seasons=off
mods_ages=claw-game,fortune-teller
mods_seasons=
profile=enhanced
view=far
menus=view
aspect=auto
ghosts=auto
transitions=on
window_scale=full
launcher_window=1280x720
key_right=Right ... key_a=X key_b=Z key_select=Backspace key_start=Return
pad_a=a pad_b=b pad_select=back pad_start=start
```

`ghosts=` is Display's Neighbour workers (`auto`, `1` or `2`). `core=sameboy` or `core=mgba` appears once the player chooses a core in Display's Advanced rows: without it, the application's default plays (Accurate on the desktops, Fast on Android), so that an update that changes the default reaches a player who never chose. A game's save (`.sav`, the cartridge's RAM) is the same file on both cores: a game saved on one opens on the other; a savestate does not.

The save is written when it changes, about every five seconds and at exit, through a temporary file renamed afterwards. At exit the launcher reports the late frames and, for audio, the underruns (silences heard); the core produces slightly more or fewer samples per second depending on the queue's fill, so that it never drops or starves.

## Android

The Android application is the same launcher, landscape and fullscreen (F11 and Display's window scale do nothing there).

- The mods folder is in the application's private storage, out of reach over USB: Mods lists no mod on Android yet.
- Android's Back button is the launcher's Escape: back in the home screen, the pause menu in a game, a capture cancelled in Controls. A real controller's back button stays its Select.
- Choose ROM and Choose patch open Android's document picker; the chosen file is copied, under its name, into `Android/data/io.github.thibautbus.theoraclesproject/files/roms` (or `patches`), where its save and savestates go too, reachable over USB; Open folder says that path. A chosen file never overwrites anything: the same content under the same name is taken as it is, another content takes the first free name (`Name (2).gbc`), and the player's original is never touched.
- When Android sends the application to the background, the save is written at once; on return the game is in its pause menu.
- What the launcher reports, the end-of-game report on late frames and audio underruns included (written when a game is left through its pause menu), goes to `Android/data/io.github.thibautbus.theoraclesproject/files/the-oracles-project.log`, reachable over USB or with a file manager; the previous run's is kept beside it as `the-oracles-project.previous.log`. It is the file to send with a report of a game that stutters.
- Touch controls, for a phone without a controller: at the first touch in a game, a d-pad on the left, A and B on the right, Select and Start at the bottom centre and a pause button at the top right, half transparent over the image, lighter while pressed; the d-pad gives eight directions and keeps a finger that slides off it, a finger slides from A to B, several fingers press together. A key or a controller hides them; the launcher and the pause menu are touched as they are clicked.
- The application declares fragile user data: at uninstall, Android offers to keep its data, ROMs, patches and saves included.
