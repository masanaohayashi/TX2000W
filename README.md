# TX2000W

A native re-implementation of the Yamaha MU2000 tone generator, as an AUv3 / VST3 plug-in and a standalone app for macOS, Windows and iOS.

## What it is

- **Native, not emulated.** TX2000W was written by studying the behaviour of [S-MU2000](https://github.com/tarboh/S-MU2000), which runs the real MU2000 firmware in emulation, and using it as the reference. TX2000W does not run the firmware: voices, XG/GS handling, effects and the front panel are written directly in C++.
- **The ROMs are still needed.** The waveforms and some tables (voice records, effect type data and so on) are read from the MU2000 ROMs at startup. No ROM data is included in this repository; you supply it from your own hardware.
- **Much lighter.** It takes roughly **1/8 to 1/20 of the CPU** that the emulation needs (see below).

## Accuracy

The output does not match S-MU2000 sample for sample. In listening, though, the difference should be hardly noticeable.

S-MU2000 is the reference. Measured against it:

- Stepping through all 100 preset performances, each note's held level and release tail stay within 1.1 dB, and 98 of the 100 stay within 1 dB.
- Across 354 effect reference renders, the level stays within 0.36 dB.

The remaining differences come from the firmware's CPU timing, for example exactly when it restarts the effect LFOs after a performance change. The real unit varies by a similar amount depending on when a button is pressed.

## CPU load

The same MIDI files, rendered offline on one core of an Apple M2 Max. The figures are CPU seconds for 30 seconds of audio, with startup excluded.

| MIDI file | S-MU2000 (emulation) | TX2000W | Ratio |
|---|---|---|---|
| Test file A (many voices) | 3.57 s | 0.42 s | about 1/8 |
| Test file B (fewer voices) | 2.07 s | 0.09 s | about 1/23 |

The emulation costs about the same however many notes are playing. TX2000W's cost grows with the number of sounding voices.

## ROMs

| File | Size | Contents |
|---|---|---|
| `mu2000_flash.bin` | 4 MB | Program ROM (tables) |
| `dump/xv364a0.ic49` | 8 MB | Wave ROM |
| `dump/xv365a0.ic50` | 8 MB | Wave ROM |
| `dump/xw848a0.ic53` | 8 MB | Wave ROM |
| `dump/xw849a0.ic54` | 8 MB | Wave ROM |
| `hd44780u_b04.bin` | | LCD character set (optional) |

Where the ROM folder goes:

- **macOS:** `~/Library/Group Containers/group.tokyo.studio-r.synths/TX2000W/roms`
- **Windows:** `%APPDATA%\STUDIO-R\TX2000W\roms`
- **iOS:** import the ROMs in the app. They are kept in the app's `Documents/roms`.
- **Any platform:** the `S_MU2000_ROMS` environment variable overrides the folder.

## Building

```bash
git clone --recursive https://github.com/masanaohayashi/TX2000W.git
```

Open `juce/S-MU2000.jucer` in the Projucer and export to Xcode (macOS, iOS) or Visual Studio 2026 (Windows). The regression tests run against a Release build:

```bash
python3 juce/Tools/run_tests.py
```

## Special thanks

Many thanks to [tarboh](https://github.com/tarboh), the author of [S-MU2000](https://github.com/tarboh/S-MU2000), and to everyone who contributed to it. TX2000W could not have been written without S-MU2000 to study and to measure against.

## License

TX2000W is free software under the GNU General Public License v3.0 or later. See [LICENSE](LICENSE).

Parts taken from S-MU2000 and MAME keep their BSD 3-Clause notices. JUCE is a submodule under AGPLv3 or the JUCE commercial licence. See [NOTICE.txt](NOTICE.txt).

Yamaha, MU2000 and XG are trademarks of Yamaha Corporation. This project is not affiliated with, endorsed by, or supported by Yamaha.
