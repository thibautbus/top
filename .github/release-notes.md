A faithful, moddable port of Oracle of Ages and Oracle of Seasons for PC, macOS and Android. Your own ROM runs in an unmodified [SameBoy](https://sameboy.github.io/) core; around it, a native host observes the game and presents it: the original image, or a widescreen view with the neighbouring rooms, with gameplay options, Lua mods and fan games from their BPS patch.

The game needs your own ROM of Oracle of Ages or Oracle of Seasons (US), and a fan game its BPS patch; none is included.

## Installing

- **Windows** (`the-oracles-project-X.Y.Z-windows.zip`, 64-bit): unzip the folder and run `the-oracles-project.exe`. The executable is not signed, so SmartScreen may warn the first time: More info, then Run anyway.
- **macOS** (`the-oracles-project-X.Y.Z-macos.zip`, one application for Apple Silicon and Intel, macOS 11 or later): unzip and move The Oracles Project to Applications. The application is not notarised: open it the first time by right-clicking it and choosing Open, or run `xattr -dr com.apple.quarantine "The Oracles Project.app"`.
- **Linux** (`the-oracles-project-X.Y.Z-linux-x86_64.tar.gz`): extract the folder and run `./the-oracles-project`; SDL 3 comes with it. The `the-oracles-project.desktop` file inside is optional: set its two paths, then copy it to `~/.local/share/applications/`.
- **Android** (`the-oracles-project-X.Y.Z-android.apk`, Android 10 or later): install the APK, allowing its installation from your browser or file manager when Android asks.

`SHA256SUMS.txt` holds the SHA-256 of each file.
