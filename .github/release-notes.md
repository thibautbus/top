A faithful, moddable port of Oracle of Ages and Oracle of Seasons for Windows, macOS, Linux and Android: the original picture, or a widescreen view with the rooms around Link, at three distances, Near, Medium and Far, in the shape of your screen, 16:9 or 4:3; item hotkeys and other gameplay options, languages, Lua mods and fan games from their BPS patch. Your own ROM runs untouched in an emulator core, and Display › Quality sets the view and the core for your device in one choice, from Low to Max.

The game needs your own ROM of Oracle of Ages or Oracle of Seasons (US), and a fan game its BPS patch; none is included.

## Installing

- **Windows** (`the-oracles-project-X.Y.Z-windows.zip`, 64-bit): unzip the folder and run `the-oracles-project.exe`. The executable is not signed, so SmartScreen may warn the first time: More info, then Run anyway.
- **macOS** (`the-oracles-project-X.Y.Z-macos.zip`, one application for Apple Silicon and Intel, macOS 11 or later): unzip and move The Oracles Project to Applications. The application is not notarised. On macOS 11 to 14, open it the first time by right-clicking it and choosing Open. On macOS 15 or later, open it once, then choose Open Anyway in System Settings › Privacy & Security. Or run `xattr -dr com.apple.quarantine "The Oracles Project.app"`.
- **Linux** (`the-oracles-project-X.Y.Z-linux-x86_64.AppImage`): make the file executable (`chmod +x the-oracles-project-X.Y.Z-linux-x86_64.AppImage`, or in the file's properties) and run it; SDL 3 comes with it. If the AppImage does not start, use the `.tar.gz`, or run the AppImage with `--appimage-extract-and-run`: a system without FUSE (some containers and minimal installs) cannot mount it.
- **Linux, archive** (`the-oracles-project-X.Y.Z-linux-x86_64.tar.gz`): extract the folder and run `./the-oracles-project`; SDL 3 comes with it. The `the-oracles-project.desktop` file inside is optional: set its two paths, then copy it to `~/.local/share/applications/`.
- **Linux ARM64** (`the-oracles-project-X.Y.Z-linux-arm64.AppImage` and `the-oracles-project-X.Y.Z-linux-arm64.tar.gz`; Raspberry Pi 5, Asahi Linux...): experimental, not yet tried on real hardware; feedback welcome. They install as the two files above.
- **Android** (`the-oracles-project-X.Y.Z-android.apk`, Android 10 or later): install the APK, allowing its installation from your browser or file manager when Android asks.

`SHA256SUMS.txt` holds the SHA-256 of each file.

## First launch

Open The Oracles Project: the home screen lists Oracle of Ages, Oracle of Seasons and Fan games. Pick a game, open its **Cartridge** page, pick your ROM with **Choose ROM…** (a fan game asks for its patch too), then **Play**. The game starts in Enhanced, the widescreen view; **Display** changes the view, the window and the quality. The [manual](https://github.com/thibautbus/top/blob/main/docs/PLAYING.md) has every page, key and setting, and the [FAQ](https://github.com/thibautbus/top#faq) the questions players ask most.

## Updating

Install the new version as you installed the old one: unzip the Windows folder wherever you like, put the macOS application or the Linux file in place of the old one, and on Android install the APK over the installed application (from 1.2.0 on: 1.1.0 and earlier were a different application, whose files the new one does not read). Your saves, beside your ROMs, and your settings, in your user folder, are kept, and the new version finds the ROMs you chose. What's new, below, says whether the savestates of the version before still load.

## Cores

Two emulator cores run the game: [SameBoy](https://sameboy.github.io/), unmodified, the accurate reference, and [mGBA](https://mgba.io/)'s Game Boy core, lighter, with one patch (its boot ROM), for small devices. Fast (mGBA) plays by default on Android, Accurate (SameBoy) elsewhere; Display › Advanced › Core changes it.
