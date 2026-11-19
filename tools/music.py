"""Render the PC score on one Genesis FM chip and pack its register stream."""
import struct
import subprocess
from pathlib import Path

SONGS = ("D_INTRO","D_INTER","D_VICTOR",*(f"D_E1M{i}" for i in range(1,6)))


def compress(data, large=True):
    # Resurrection uses a 32 KiB look-back window for VGM and 4 KiB for WADs.
    window, limit = (32768,256) if large else (4096,16)
    history = {}
    pos = 0
    out = bytearray()
    while True:
        index = len(out)
        out.append(0)
        for bit in range(8):
            if pos == len(data):
                out[index] |= 1 << bit
                out.extend(bytes(3 if large else 2))
                return bytes(out)
            best, distance = 0, 0
            key = data[pos:pos+3]
            for past in reversed(history.get(key,[])):
                if pos-past > window:
                    break
                length = 3
                while length < min(limit,len(data)-pos) and data[past+length] == data[pos+length]:
                    length += 1
                if length > best:
                    best,distance = length,pos-past-1
                    if best >= min(limit,len(data)-pos):
                        break
            if best >= (4 if large else 3):
                out[index] |= 1 << bit
                if large:
                    out.extend(struct.pack(">HB",distance,best-1))
                else:
                    out.extend(struct.pack(">H",distance*16+best-1))
            else:
                best = 1
                out.append(data[pos])
            for j in range(pos,pos+best):
                chain = history.setdefault(data[j:j+3],[])
                chain.append(j)
                if len(chain)>64:
                    del chain[:32]
            pos += best


def soundtrack(lumps, build):
    folder = build/"music"
    folder.mkdir(exist_ok=True)
    for song in SONGS:
        source = folder/(song+".mus")
        source.write_bytes(lumps[song])
        output = source.with_suffix(".zgm")
        if not output.exists():
            subprocess.run([build/"opn-build/midi2vgm","-na","-vm","3",
                            build/"libopn/fm_banks/Doom32x-fixx.wopn",source],
                           cwd=folder, stdout=subprocess.DEVNULL, check=True)
            vgm = bytearray(Path(str(source)+".vgm").read_bytes())
            # Loop from the first register write after the last event.
            start = 0x34 + struct.unpack_from("<I",vgm,0x34)[0]
            struct.pack_into("<I",vgm,0x1c,start-0x1c)
            struct.pack_into("<I",vgm,0x20,struct.unpack_from("<I",vgm,0x18)[0])
            output.write_bytes(compress(bytes(vgm)))
        yield song,output.read_bytes()
