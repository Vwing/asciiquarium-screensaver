# Asciiquarium Screensaver

A Windows screensaver port of Asciiquarium, the ASCII aquarium animation originally written by Kirk Baucom.

This port is based on Florian Hübner's fork: https://github.com/nothub/asciiquarium/

This project builds a `.scr` screensaver wrapper plus a companion rendering executable. Keep both files together.

## Install From Release

Download the release zip and extract it somewhere permanent, such as:

```text
C:\Tools\Asciiquarium\
```

Right-click `Asciiquarium.scr` and choose **Install**.

Windows will open the Screen Saver Settings dialog. Select **Asciiquarium** if it is not already selected.

## Build

Requirements:

- Windows
- Visual Studio 2019 or 2022 with C++ build tools
- CMake

From this directory:

```bat
build.bat
```

Build outputs are written to:

```text
build\Release\Asciiquarium.scr
build\Release\AsciiquariumApp.exe
```

To install a local build, keep those two files in the same folder, then right-click `Asciiquarium.scr` and choose **Install**.

## Files

- `Asciiquarium.scr` is the Windows screensaver entry point.
- `AsciiquariumApp.exe` renders the aquarium.
- `asciiquarium-original` is the original Perl script used as the behavior reference.
- `CHANGES.txt`, `LICENSE.txt`, and this README include upstream project information and licensing.

## Known Bugs

- Colors are different than the curses-rendered Perl script.

## License

Asciiquarium is licensed under the GNU General Public License. See [LICENSE.txt](LICENSE.txt).

Original project: http://robobunny.com/projects/asciiquarium

Florian Hübner's fork: https://github.com/nothub/asciiquarium/
