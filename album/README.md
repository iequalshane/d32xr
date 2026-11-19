# Chex Quest soundtrack files

Download Andrew Benon’s [Chex Quest (Official Game Soundtrack) from Bandcamp](https://andrewbenon.bandcamp.com/album/chex-quest-official-game-soundtrack). Choose **FLAC** and extract the ZIP into this directory. You can keep the album subfolder and all the original filenames.

The tool looks for one file with each of the following filename endings. The album numbers below are from the Bandcamp download. The CD uses track 01 for data.

| CD track | Album filename ending |
| --- | --- |
| 02 | `03 E1 Intro.flac` |
| 03 | `02 E1 Intermission.flac` |
| 04 | `09 E1 Victory.flac` |
| 05 | `04 E1M1 Landing.flac` |
| 06 | `05 E1M2 Storage.flac` |
| 07 | `06 E1M3 Lab.flac` |
| 08 | `07 E1M4 Arboretum.flac` |
| 09 | `08 E1M5 Caverns.flac` |

From the repository root, run `python3 tools/cd_audio.py`. The tool converts these eight tracks to 44.1 kHz, 16-bit stereo WAV and writes `ChexQuest.cue` and `Disc/` beside the cartridge. See the [optional CD soundtrack instructions](../README.md#optional-cd-soundtrack) for prerequisites and playback.
