"""Build the optional Chex Quest companion CD from an extracted Bandcamp FLAC album."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import wave


# Disc order follows the music lumps in the cartridge, not Bandcamp album order.
TRACKS = (
    "E1 Intro", "E1 Intermission", "E1 Victory", "E1M1 Landing",
    "E1M2 Storage", "E1M3 Lab", "E1M4 Arboretum", "E1M5 Caverns",
)


def make_data_track(empty, output):
    empty.mkdir()
    if shutil.which("genisoimage"):
        command = ["genisoimage", "-quiet", "-V", "CHEXQUEST", "-o", output, empty]
    elif shutil.which("hdiutil"):
        command = ["hdiutil", "makehybrid", "-iso", "-iso-volume-name", "CHEXQUEST",
                   "-o", output, empty]
    else:
        raise ValueError("Install genisoimage to create the data track.")
    subprocess.run(command, check=True)
    with output.open("r+b") as iso:
        iso.write(b"SEGADISCSYSTEM  ")
        # Six seconds of Mode 1 sectors, as on the original companion disc.
        iso.truncate(max(output.stat().st_size, 450 * 2048))


def build(album, output):
    if not album.is_dir():
        raise ValueError(f"Expected an extracted FLAC album directory: {album}")
    sources = []
    for title in TRACKS:
        matches = list(album.rglob(f"* {title}.flac"))
        if len(matches) != 1:
            raise ValueError(f"Expected one FLAC ending in ' {title}.flac', found {len(matches)} in {album}. See album/README.md.")
        sources.append(matches[0])

    output.mkdir(parents=True, exist_ok=True)
    cue = output / "ChexQuest.cue"
    disc = output / "Disc"
    if any(path.exists() or path.is_symlink() for path in (disc, cue)):
        raise ValueError(f"Move the existing Disc directory or ChexQuest.cue out of {output} first.")

    with tempfile.TemporaryDirectory(dir=output) as tmp:
        stage = Path(tmp)
        tracks = stage / "Disc"
        tracks.mkdir()
        make_data_track(stage / "empty", tracks / "Track01.iso")
        lines = ['FILE "Disc/Track01.iso" BINARY', '  TRACK 01 MODE1/2048', '    INDEX 01 00:00:00']
        for number, (title, source) in enumerate(zip(TRACKS, sources), start=2):
            filename = f"{number:02} {title}.wav"
            pcm = subprocess.check_output([
                "ffmpeg", "-nostdin", "-v", "error", "-i", source, "-map", "0:a:0",
                "-ac", "2", "-ar", "44100", "-f", "s16le", "-",
            ])
            if not pcm:
                raise ValueError(f"No audio decoded from {source}")
            with wave.open(str(tracks / filename), "wb") as wav:
                wav.setparams((2, 2, 44100, 0, "NONE", ""))
                wav.writeframesraw(pcm)
                wav.writeframes(bytes(-len(pcm) % 2352))  # Complete the final CD audio sector.
            lines.extend([f'FILE "Disc/{filename}" WAVE', f"  TRACK {number:02} AUDIO"])
            if number == 2:
                lines.append("    PREGAP 00:02:00")
            lines.append("    INDEX 01 00:00:00")

        staged_cue = stage / cue.name
        staged_cue.write_text("\n".join(lines) + "\n", encoding="ascii")
        tracks.rename(disc)
        try:
            staged_cue.rename(cue)
        except OSError:
            disc.rename(tracks)
            raise
    print(cue)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.cwd(), help="directory for ChexQuest.cue and Disc (default: current directory)")
    args = parser.parse_args()
    try:
        build(Path(__file__).resolve().parents[1] / "album", args.output)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")
