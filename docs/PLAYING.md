# Playing

You need your own ROM of Oracle of Ages or Oracle of Seasons (US). The port recognises the original US images by their SHA-1 and plays them in every profile; another image whose header is an Oracle's (a hack, a randomizer seed) plays in Faithful only, without the Enhanced view or the gameplay options.

```bash
build/the-oracles-project                                     # the launcher's home screen
build/the-oracles-project --rom "/path/to/Oracle of Ages.gbc" # the game straight away, without the home screen
```

## The launcher

Without `--rom`, `the-oracles-project` opens the home screen in a 16:9 window of 1280x720 at the first opening, resizable down to 960x540, whose size is remembered: Oracle of Ages, Oracle of Seasons and Fan games, the chosen entry on the right with its title, its state (`Ready`, with the date of its save as the last session, `ROM not found`, or for a fan game whose Oracle's ROM is chosen, `Patch not found`) and its menu, the two others on the left. A greyed item gives its reason when highlighted. Arrows, Enter and Escape on the keyboard, the d-pad, A and B on a controller, hover and click with the mouse; F11 toggles fullscreen.

**Cartridge** is the game's cartridge: the ROM (file, folder and status: original ROM, all profiles; unrecognised ROM, Faithful only; refused, with the loader's reason; none), **Choose ROM…**, which opens the system's file dialog (Windows', macOS'; on Linux the desktop's through its portal, or zenity's if installed; without one, a message suggests dropping the file on the window); the save, its date, and **Open folder**; **Play**. The ROM is remembered per game. A ROM also comes by dropping its file on the window: on a game's Cartridge page it counts as Choose ROM… for that game; on the home screen it is identified and filed under its game. A refused ROM shows its reason and is not remembered.

**Display** sets, for every game the home screen starts: the profile, Faithful (160x144) or Enhanced; the drawn-back view (480x270) with continuous transitions and the smooth camera; the window, 2x, 3x or 4x the profile's surface, or fullscreen at the screen's largest whole scale (a scale that does not fit the screen with the title bar is reduced at launch, and Display says so); colour correction; continuous transitions, greyed in Faithful, which take Link swimming through them too; vsync. At the first opening Display is on Enhanced, fullscreen, with colour correction and continuous transitions on and vsync auto; the item hotkeys start off. The sizes and the diagram are those of what will play for the game shown, on the screen the window is on. The choices apply at the next Play.

**Controls** sets the keys and buttons for every game: the eight buttons of the game on the keyboard and on a controller (A, B, Select and Start; the d-pad and the left stick always move), the four item hotkey slots and their two modifiers, and the item hotkeys line of the game shown. Enter on a cell waits for a key (or a button, in a controller column), Escape cancels; a key already bound elsewhere on the same device is taken from there and the other cell stays empty. **Reset to defaults** restores the grid and the hotkeys. Everything is written when it changes.

**The pause menu.** Escape in a game the home screen started pauses it: the menu lies over the game's last image, darkened, with Resume, Save state, Load state ("from <time>" when a state exists), Controls, Display and Quit to launcher, which ends the session (save written, report on the standard error) and returns to the home screen. Under 960 pixels wide, Controls and Display are left out. Escape or B resumes. From the pause, keys and colour correction apply at once, the rest at the next Play. The pause holds the session between two frames: the core does not run, no frame is counted, and a route being recorded contains none.

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
| `--zoom-out` | the drawn-back view, 480x270 (implies `--enhanced`); `--fullscreen --scale 4` fills 1920x1080 exactly |
| `--continuous-transitions` | Link keeps walking through the scrolling transitions (implies `--enhanced`) |
| `--continuous-swim` | the same, and Link keeps swimming through them too (implies `--continuous-transitions`); the home screen's choice turns it on |
| `--item-hotkeys=off\|use\|equip` | the item hotkeys for this run |
| `--camera 1\|2` | the Enhanced camera's profile |
| `--mods DIR` | a mod, repeated for several ([`MODDING.md`](MODDING.md)) |
| `--record ROUTE`, `--play ROUTE` | record the session's inputs, replay them ([`ROUTES.md`](ROUTES.md)) |
| `--scale N`, `--fullscreen` | the window |
| `--colour-correction on\|off`, `--vsync auto\|on\|off`, `--mute` | image, pacing, sound |
| `--frames N`, `--no-window`, `--screenshot PATH.ppm` | quit after N frames; no window and no pacing; the last frame as an image at exit |

A gameplay option given without `--rom` (`--continuous-transitions`, `--item-hotkeys`, `--enhanced`, `--zoom-out`, `--camera`) overrides Display and the settings for the games the home screen starts. `--save`, `--frames`, `--no-window`, `--screenshot` and `--play` belong to the game `--rom` starts and are refused without it. `--record ROUTE` without `--rom` records the first session the home screen starts. `--vsync`, `--colour-correction` and `--camera` hold for the run they are given in and never change the settings file.

On Windows the game's window is sized in pixels, so the scale is the one asked at 125 % or 150 % too; the launcher's window keeps its size in points. On macOS the windows ask for a Retina display's pixels: two pixels per point.

## Profiles and options

**Faithful** shows the core's image, 160x144: the game as it is.

**Enhanced** shows a wide surface, 256x144: the status bar in a band of sixteen lines at the top, centred, and below it the world, the play area placed by a smooth camera in world coordinates with the neighbouring rooms drawn around it. The neighbours are computed by the game itself in a second instance of the core; a neighbour not ready yet is black, never a wrong terrain. Large rooms (dungeons) are shown whole, centred, their scrolls sliding the band; interiors of one room are centred and still. Menus, cutscenes and the file select show the core's image framed. F3 toggles between the wide world and the framed core. The camera has two profiles (`--camera`, `camera=` in the settings): 1 waits for Link to cross a dead zone of sixteen pixels and catches up; 2, the default, follows Link continuously and comes to rest without stepping back.

**The drawn-back view** (`--zoom-out`) gives the Enhanced surface 480x270: at scale 4 it fills a 1080p screen. Outdoors the camera frames three rooms across and two down, Link on the middle line; a large room is shown whole, centred; elsewhere the normal band stays, centred. A savestate taken at one size is refused at the other.

**Continuous transitions** (`--continuous-transitions`, both games): in the wide view the game's own scroll no longer shows, and what remains of a transition is Link frozen for about forty frames. The option doubles the scroll's step and moves Link a pixel a frame in the transition's direction, his walk animated, so he keeps walking; beyond the pixels the game moves him itself, he only walks onto tiles without collision. The game's state then differs from a native session's: a route recorded with the option replays with it, and a savestate taken with it is refused without it.

**The swim through them** (`--continuous-swim`, which Display's continuous transitions turn on; a separate option so that the routes recorded with `--continuous-transitions` alone replay as they were played): Link swimming at the surface, in both games, flippers or Ages' mermaid suit, diving or not, keeps swimming at his own speed through the transition, his swimming animation going on, and beyond the pixels the game moves him itself only into water; on Ages' sea floor he goes on at his own speed too, onto tiles without collision. A stroke of the flippers that carries him to an edge takes the transition the game would take with the direction held, when the direction held neither points to the edge nor away from it; the mermaid suit, driven by the direction pad, keeps the game's behaviour. With the option, Link is also held back from a wall leftward and upward, and walks onto stairs and along a bridge. A savestate does not count the swim: a state taken with it loads without it.

**Item hotkeys** (`--item-hotkeys=off|use|equip`, in Faithful and Enhanced, originals only): four keys (`A`, `S`, `Q`, `W`; `x`, `y` and the two triggers on a controller) act on four slots. Held with left Shift or left Ctrl, a key takes the item of B or of A into its slot; with the inventory open on the items, it takes the item under the cursor. In `use` mode the key is one more item button: the item moves onto one of the two buttons the game reads in the current state, that button is held for the player as long as the key is, and the button's own item comes back after thirty quiet frames. In `equip` mode the key equips the item on its target, and pressed again brings the previous item back. In Enhanced, a hotbar in the status bar's gutters shows the four slots: the item's icon as the game draws it, the key, and the button the item is on; a dotted frame for an empty slot, a faded icon for an absent item, a frame that blinks while waiting and red after a refusal. The slots and the keys are remembered; the Controls line of a game turns them on (`use`) or off.

## Keys

Keyboard by default: arrows, X = A, Z = B, Enter = Start, Backspace = Select; F2 colour correction, F3 wide world or framed core, F5 save state, F7 load state, F11 fullscreen, Escape quit (the pause menu for a game the home screen started). Controller by default: d-pad or left stick, A, B, Start, Back = Select. Key names are SDL's (`Right`, `X`, `Return`, `Space`, `F1`...), button names SDL controllers' (`a`, `b`, `x`, `y`, `back`, `start`, `leftshoulder`...); on a Nintendo controller `a` is the button labelled A. An unknown name is reported and replaced by the default.

Colour correction off is the raw RGB555 conversion; on, SameBoy's "modern balanced" rendering, close to an original screen. `vsync auto` presents each frame at the display's rate when it refreshes between 59 and 61 Hz, close enough to the Game Boy's 59.7275 Hz for the audio rate control to absorb the difference; on another display, or with `off`, the host paces at 59.7275 Hz. Under vsync the host checks that the presentation waits for the display, and if it does not, turns vsync off and paces itself for the rest of the session.

## Savestates

F5 saves and F7 loads a state (`<save>.state`, beside the `.sav`). A state is composite: the core's state in its own format, the host's state (the camera, empty in Faithful), and the versions of the format, the core, the game, the ROM and the active mods, gameplay options included. A file made with another version, another ROM or other options is refused with a message that says which. Loading a state stops a route being replayed or recorded. There is no savestate during a mod's conversation.

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
transitions=on
window_scale=full
launcher_window=1280x720
key_right=Right ... key_a=X key_b=Z key_select=Backspace key_start=Return
pad_a=a pad_b=b pad_select=back pad_start=start
```

The save is written when it changes, about every five seconds and at exit, through a temporary file renamed afterwards. At exit the launcher reports the late frames and, for audio, the underruns (silences heard); the core produces slightly more or fewer samples per second depending on the queue's fill, so that it never drops or starves.

## Android

The Android application is the same launcher, landscape and fullscreen (F11 and Display's window scale do nothing there).

- The mods folder is in the application's private storage, out of reach over USB: Mods lists no mod on Android yet.
- Android's Back button is the launcher's Escape: back in the home screen, the pause menu in a game, a capture cancelled in Controls. A real controller's back button stays its Select.
- Choose ROM and Choose patch open Android's document picker; the chosen file is copied, under its name, into `Android/data/io.github.thibautbus.theoraclesproject/files/roms` (or `patches`), where its save and savestates go too, reachable over USB; Open folder says that path. A chosen file never overwrites anything: the same content under the same name is taken as it is, another content takes the first free name (`Name (2).gbc`), and the player's original is never touched.
- When Android sends the application to the background, the save is written at once; on return the game is in its pause menu.
- Touch controls, for a phone without a controller: at the first touch in a game, a d-pad on the left, A and B on the right, Select and Start at the bottom centre and a pause button at the top right, half transparent over the image, lighter while pressed; the d-pad gives eight directions and keeps a finger that slides off it, a finger slides from A to B, several fingers press together. A key or a controller hides them; the launcher and the pause menu are touched as they are clicked.
- The application declares fragile user data: at uninstall, Android offers to keep its data, ROMs, patches and saves included.
