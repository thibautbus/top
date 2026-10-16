# Changelog

What changed in each version of The Oracles Project, the newest first. The release page of each version on GitHub carries the same text, under the installation notes.

## 1.2.0 (unreleased)

Most of this release answers what players asked for after 1.1.0, many of them on Retroid, AYN and Anbernic handhelds. Thank you for every report, screenshot and log, and to everyone who tested the builds and posted feedback: they shaped this version.

### New

- **Languages.** Ages and Seasons can now play in French, German, Italian and Spanish, in the official translations, or in the European edition's English. Choose your European ROM in the game's **Cartridge** page, under **European ROM**, then pick the **Language** row, where each language is listed by its own name. The texts are taken from your European ROM once, on your own device, and nothing of it is shipped. The screens that carry text are translated too: the file select, the save menu, the game over screen and the dungeons' names. Mistakes of the official translations (a place named for another, an item named several ways, a wrong word) are corrected. The title screen and the credits stay as the US game has them. English (US), the default, is the cartridge's own text, untouched.
- **Cheats.** Ages and Seasons have a **Cheats** page, in the game's menu after Mods and in the pause menu: **No damage** (enemies, spikes and falls no longer cost hearts; Link is still knocked back), **Ammo never runs out** (seeds, bombs and Bombchus are not used up) and **Rupees never run out** (buying costs nothing; mods still charge). Cheats play with your usual save, and what you keep stays in it. From the pause, a change applies as soon as you resume.
- **Full-width status bar.** In Enhanced, the status bar's colour now runs across the whole width of the view instead of leaving two dark bands beside it. **Display > HUD**: Full width (the new default) or Original, the bar as the game draws it. It works in the five games and changes at once from the pause.
- **Hold to swim.** **Controls > Swimming**: Tap, as the game wants it, or Hold, where holding a direction swims at full speed, instead of tapping A with the flippers or tapping the direction with Ages' mermaid suit. It works in Ages and Seasons, in Faithful and Enhanced.
- **A controller button for the menu.** The pause menu now opens from a controller without going back to the home screen: the left stick's click (L3) by default, and Guide (the Home button) where the system lets it through. Change the button in **Controls > Menu**: any button the game and the item hotkeys leave free, or a trigger. The first game you start with a controller says once which button it is ("Press L3 for the menu.").
- **Diagonal transitions.** With **Continuous transitions**, walking or swimming diagonally into the next room now carries on diagonally through the scroll, instead of sliding straight along the edge for half a second. Walls still hold Link where the game would. It works in Ages, Seasons and the three fan games.
- **Scaling.** **Display > Advanced > Scaling**: Sharp (the default), the game at its largest whole scale with every pixel exact, as before, or Fill, the image as large as the screen allows in its proportions, the pixels slightly softer. Fill applies in fullscreen. On a screen where the view would otherwise sit small in a black frame (Far 4:3 on a 640x480 handheld), Fill lets it fill the screen.
- **New version notice.** When a newer version is published, the home screen says so next to the version at the bottom left ("1.3.0 available"). Select it (Down from Exit, then Enter or A, or click or tap it) to open its release page in your browser; the launcher stays open. The launcher asks GitHub once at launch, in the background, and sends nothing else; `update_check=off` in `settings.txt` turns this off (not on Android for now, where `settings.txt` is out of reach).
- **Screen margin.** **Display > Advanced > Screen margin**: Off (the default), Auto, Small or Large. For a device whose screen hides its top or bottom edge, cutting off the status bar or the top of the inventory: Auto keeps clear of the notch or curved edge the system reports, Small and Large keep a fixed margin at the top and bottom. It applies at once, from the pause too.

### Improved

- **Far view fitted to the screen.** On 4:3 screens of about 1280 pixels across, Far showed at twice its size in a black frame. It now takes a size made for the screen and fills it at three times: 426x320 on a 1280x960 screen (Retroid Pocket Nova and Mini, Anbernic RG477M) and 412x360 on a 1240x1080 screen (Retroid Pocket Mini V2 and Classic, AYN Thor's lower screen). Display shows the size the game will take, and in the 4:3 layout View's help says "Far fits this screen". Every other screen keeps Far's usual size.
- **Key hints that follow your device.** The key hints at the bottom of the launcher's pages, the pause menu's included, now show only the keys of the device you used last: the keyboard's after a key, the controller's (D-pad, A, B, the menu button) after a button. Controls shows Escape and F2 to F11 only once a keyboard is used.
- **Display** now reads Profile, Quality, Window, View, HUD, Color correction; **Continuous transitions** moved to Display's Advanced page, with Core, Vsync, Neighbor workers, Scaling and Screen margin.
- On a screen where every Window choice comes to the same size, choosing Fill now starts the game fullscreen, as Display's diagram shows it.
- The neighbouring rooms fill in more reliably, under the sea in Ages in particular.
- **Zipped ROMs.** Choose ROM, Choose patch and European ROM now take a `.zip` too, as most ROMs are kept: the one ROM or patch inside it is read, nothing is unpacked to disk, and the save goes beside the zip. A zip holding several ROMs, or compressed other than the usual way, is refused with the reason.

### Fixed

- In the drawn-back view (Medium and Far), far rooms are prepared a little before they come into view, so their corners show black much less often at the edges of the screen, swimming in particular.
- At each door, while the next room loads, the status bar keeps its look from play (full width, with the item hotkeys) instead of switching for a moment to its original, narrow form.
- After the season changes in Seasons (crossing into an area of another season, or with the Rod of Seasons), and after a game loads, a piece of the season's or era's banner no longer stays drawn over the scenery for a frame.
- Enemies of the room Link leaves no longer vanish at the edge of the game's own screen during the scroll: they stay where they were and fade out once it ends, as the ones of the room entered fade in.
- Walking diagonally or on stairs in Enhanced, Link no longer shakes a pixel back and forth against the screen. The fix in 1.1.0 was not enough; the camera now follows Link where the picture draws him.
- Entering or leaving a house or a shop with the game's menus enlarged (the default on Android) no longer shows the room enlarged before it shrinks back: the room stays at the view's scale while it loads.
- With the Fast (mGBA) core on Windows, mods could misbehave; they now play as on the other platforms.
- In Gifts of Kinomi, the caves made of several rooms (east of the map) now show in Medium and Far with the rooms they open onto, instead of a single screen framed in black.

### Android

- **A new application.** On Android, 1.2.0 has a new identifier, `org.theoraclesproject`, so Android sees it as a new application: it installs beside 1.1.0 instead of over it, and starts without your ROMs, saves and settings.
- **Back opens the pause menu**, also on handhelds whose Back button belongs to the built-in controller (Retroid Pocket), where it used to open the game's map. The controller's Select button stays the game's Select. A controller that Android reports without a Select of its own keeps its Back as Select: its left stick's click or Guide opens the menu.
- **The system bars stay hidden.** When the status bar stays over the game (after the notification shade, a system dialog, or back in the app), it is hidden again when the app gets the focus back, or three seconds after it showed. A swipe from the edge still shows the bars for a moment.
- **Screen margin** (above) is for handhelds whose screen hides an edge, such as the AYN Thor: try Small, then Large if the picture is still cut. The log (`the-oracles-project.log`) now records the screen's safe area, the margin and the band the game keeps to: please send it with a report.
- The launcher's pages no longer show keyboard keys on a handheld played with its controller.
- The left stick's click or Guide opens the pause menu in game (above), without going back to the home screen.
- **Mods now work on Android:** put each mod in its own folder in `Android/data/org.theoraclesproject/files/mods`, from a computer over USB (since Android 11, most file managers cannot open `Android/data`), then switch it on in the game's Mods page, as on a computer. The Mods page shows that folder, and Cartridge the ROMs' folder, `Android/data/org.theoraclesproject/files/roms`, where your saves are, as a computer sees them over USB.

### Notes

- **Defaults.** The status bar is Full width in Enhanced (Display > HUD > Original brings back the old look). Continuous transitions, on by default, now include the diagonal. Far takes the fitted size on the screens above. The menu button is L3; if your settings already gave L3 to Select or to an item hotkey, it stays theirs, Guide alone opens the menu, and Controls > Menu lets you choose another button. Swimming starts on Tap, Screen margin on Off and Scaling on Sharp, so the game plays as before until you change them.
- **Saves and settings.** Your saves (`.sav`) and `settings.txt` from 1.1.0 carry over as they are (on Android, a new application, they do not: see Android above), and cheats use the same save. Savestates (F5) from 1.1.0 still load, with one exception: a savestate taken in Far on a 1280x960 or 1240x1080 screen loads only at the size it was taken at; add `far=fixed` to `settings.txt` to keep Far's previous size and load it. On Android, where `settings.txt` is out of reach, such a savestate does not load: continue from the game's own save (`.sav`). A savestate taken in a language loads only in that language.
- **On macOS**, the application has a new identifier too, `org.theoraclesproject`: your settings and saves stay where they were, but macOS may treat it as a new application and ask again, the first time it opens, as it did when you first installed it (right-click, then Open).
- **Fan games** keep English for now: their own dialogues are not translated yet, so the Language row is only for Ages and Seasons. Cheats and Hold to swim are for the original Ages and Seasons too.

## 1.1.0 (2026-10-09)

- **Two cores.** The games run on SameBoy (Accurate) as well as mGBA's Game Boy core (Fast), behind one interface; Fast plays by default on Android. Display's Advanced page chooses the core.
- **Three views in the shape of the screen.** The widescreen view has three levels, Near, Medium and Far, in 16:9 and in 4:3, and the launcher has a 4:3 layout.
- **Quality.** Display's Quality sets the view and the core for the device in one choice, Low to Max, chosen by the device at the first opening; after a game that ran slowly, the launcher suggests a lighter one.
- **Neighbouring rooms.** Two background instances prepare the rooms around (Neighbor workers, in Display's Advanced page); a neighbour is rendered again only when what it reads has changed; the rooms of the past stay drawn across time warps; the view follows the screen shake and full fades, and keeps the neighbours' animations in step and their rooms when the colour correction changes.
- **Controllers.** Every controller is read, and controllers are opened again after a game.
- **Fixes.** A diagonal walk no longer wobbles Link in the widescreen view; vsync stays off on Android when it is set to auto; the launcher writes its report to a file on Android.
- Settings files and saves of 1.0.0 still load, and 1.1.0 installs over 1.0.0 on Android.

## 1.0.0 (2026-09-29)

The first release, for Windows, macOS, Linux and Android.

- **Ages and Seasons** (US) from your own ROM, in the SameBoy core, with a native renderer.
- **Two profiles.** Faithful, the original image, or Enhanced, a widescreen view that shows the neighbouring rooms, computed in a background instance.
- **Gameplay options.** Continuous transitions and item hotkeys.
- **Mods** in Lua.
- **Fan games.** Gifts of Kinomi, Moonrise Regalia and Temple of Seasons, from their BPS patch.
- **A launcher** to choose the ROMs, the profile, the window, the keys and the hotkeys.
- **Routes** that replay and check sessions.
