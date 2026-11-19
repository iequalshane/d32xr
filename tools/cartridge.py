"""Convert the five-level 1996 Chex Quest IWAD to Resurrection cartridge data."""
import argparse
import hashlib
from walls import wall_image
from music import soundtrack, compress
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
FILES = ROOT / "content/files"
pack = struct.pack
read = struct.unpack_from

# Zero bytes are transparent in the 32X overwrite framebuffer. The original
# Chex palette has the same black at 247, including every palette flash.
OPAQUE_PIXELS = bytes([247, *range(1, 256)])


def name(raw):
    return raw.split(b"\0")[0].decode("ascii").upper()


def directory(blob):
    count, start = read("<II", blob, 4)
    return [(name(key), blob[offset:offset + size])
            for offset, size, key in struct.iter_unpack("<II8s", blob[start:start + count * 16])]


def columns(patch):
    width, height, left, top = read("<HHhh", patch)
    result = []
    for pos in read(f"<{width}I", patch, 8):
        runs = []
        while patch[pos] != 255:
            row, size = patch[pos:pos + 2]
            if size:
                runs.append((row, patch[pos + 3:pos + 3 + size]))
            pos += size + 4
        result.append(runs)
    return width, height, left, top, result


def bitmap(patch):
    w, h, _, _, cols = columns(patch)
    data = bytearray(w * h)
    for x, runs in enumerate(cols):
        for y, pixels in runs:
            for j, value in enumerate(pixels[:h-y].translate(OPAQUE_PIXELS)):
                data[(y+j)*w+x] = value
    return w, h, data


def jagobj(patch):
    w, h, image = bitmap(patch)
    return pack(">8H", w, h, 3, 0, 0, 0, 0, 0) + image


def sprite(patch):
    width, _, left, top, cols = columns(patch)
    return encode_posts(width,left,top,cols)


def encode_posts(width,left,top,cols):
    posts, pixels, offsets = bytearray(), bytearray(), []
    seen = {}
    for runs in cols:
        key = tuple(runs)
        if key not in seen:
            seen[key] = 6 + width*2 + len(posts)
            parts = [(y+i, run[i:i+128]) for y, run in runs for i in range(0,len(run),128)]
            for j, (y, run) in enumerate(parts):
                posts.extend(pack(">BBH", y, (len(run)-1) | (128 if j+1 == len(parts) else 0), len(pixels)))
                pixels.extend(run)
            if not parts:
                posts.append(255)
        offsets.append(seen[key])
    return pack(">Hhh",width,left,top) + pack(f">{width}H",*offsets) + posts, bytes(pixels)


class Cartridge:
    def __init__(self):
        self.entries = []
        self.pool = bytearray()
        self.stored = {}

    def store(self, data):
        data = bytes(data)
        if data not in self.stored:
            self.pool.extend(bytes(-len(self.pool) % 4))
            self.stored[data] = len(self.pool)
            self.pool.extend(data)
        return self.stored[data]

    def add(self, key, data=b""):
        if key.startswith("DEMO") or key in {"TITLE","INTERPIC","BOSSBACK","LINEDEFS","SECTORS"}:
            packed = compress(bytes(data),large=False)
            if len(packed)<len(data):
                self.entries.append((self.store(packed), len(data), bytes([ord(key[0])|128])+key[1:].encode()))
                return
        self.entries.append((self.store(data), len(data), key.encode()))

    def finish(self, pixel_offsets):
        directory_size = 12 + 16*(len(self.entries)+1)
        table = b"".join(pack(">I", directory_size+pos) for pos, size in pixel_offsets)
        self.add("POFFSETS", table + b"".join(pack(">H",size) for pos,size in pixel_offsets))
        return (pack(">4sII",b"IWAD",len(self.entries),12) +
                b"".join(pack(">II8s",directory_size+p,n,k) for p,n,k in self.entries) +
                self.pool)


def textures(cart, lumps, used, masks, lighting):
    texture_ids = {"-":0}
    mask_sprites = []
    patches = [name(n[0]) for n in struct.iter_unpack("8s", lumps["PNAMES"][4:])]
    records = []
    cart.add("T_START")
    for key in ("TEXTURE1","TEXTURE2"):
        data = lumps.get(key)
        if not data:
            continue
        for pos in read(f"<{read('<I',data)[0]}I",data,4):
            key, _, w, h, _, count = read("<8sIHHIH",data,pos)
            texname = name(key)
            if texname not in used:
                continue
            pixels = bytearray(w*h)
            opaque = bytearray(w*h)
            for j in range(count):
                x,y,idx,_,_ = read("<hhHhh",data,pos+22+j*10)
                pw, ph, _, _, cols = columns(lumps[patches[idx]])
                for col, runs in enumerate(cols):
                    if not 0 <= col+x < w:
                        continue
                    for start, run in runs:
                        for row,value in enumerate(run,start+y):
                            if 0 <= row < h:
                                pixels[(col+x)*h+row] = value
                                opaque[(col+x)*h+row] = 1
            texture_ids[texname] = len(records)+1
            if texname in masks:
                cols = []
                for x in range(w):
                    runs = []
                    col = opaque[x*h:(x+1)*h]
                    y = 0
                    while y<h:
                        if not col[y]:
                            y += 1
                            continue
                        end = col.find(0,y)
                        if end < 0: end = h
                        runs.append((y,bytes(pixels[x*h+y:x*h+end])))
                        y = end
                    cols.append(runs)
                mask_sprites.append((f"MASK{texture_ids[texname]}",encode_posts(w,0,0,cols)))
            cart.add(texname,pixels if texname == "SKY1" else wall_image(pixels,w,h,lumps["PLAYPAL"],lighting))
            records.append(pack("<8sIHHIH2x",key,0,w,h,0,0))
    cart.add("T_END")
    records.insert(0, pack("<8sIHHIH2x",b"-",0,1,1,0,0))
    count = len(records)
    assert count <= 256
    cart.add("TEXTURE1",pack("<I",count)+b"".join(pack("<I",4+count*4+i*24) for i in range(count))+b"".join(records))

    return texture_ids, mask_sprites


def console_font(lumps):
    # The Mega Drive text plane indexes 8x8 tiles starting at ASCII space.
    # The font uploader expects eight tiles per row and packed 4-bit pixels.
    width, height = 64, 96
    pixels = bytearray(width * height)
    palette = [0]
    for code in range(32, 127):
        patch = lumps.get(f"STCFN{ord(chr(code).upper()):03}")
        if not patch:
            continue
        w, h, glyph = bitmap(patch)
        cell_x = (code - 32) % 8 * 8
        cell_y = (code - 32) // 8 * 8
        glyph_width = min(w, 8)
        for y in range(h):
            for x in range(glyph_width):
                color = glyph[y*w + x*w//glyph_width]
                if color not in palette:
                    palette.append(color)
                pixels[(cell_y+y)*width + cell_x+x] = palette.index(color)
    assert len(palette) <= 16
    rgb = lumps["PLAYPAL"]
    colors = b"".join(rgb[c*3:c*3+3] for c in palette).ljust(48, bytes(1))
    packed = bytes(a*16+b for a,b in zip(pixels[::2],pixels[1::2]))
    return pack(">8H",width,height,2,0,0,0,0,0) + packed + colors


def menu_font(cart, lumps):
    # Crop the textured capitals from the DOS menu and intermission artwork.
    # x coordinates are half-open ranges in the original, unscaled patches.
    crops = {
        "WILV03": [("A",0,11), ("B",24,32), ("M",88,102)],
        "WILV04": [("C",0,9), ("F",97,105), ("K",165,176)],
        "M_LOADG": [("D",31,42), ("L",0,9)],
        "M_NGAME": [("E",11,20), ("G",41,50), ("N",0,10), ("W",20,36)],
        "M_RDTHIS": [("H",55,66), ("R",0,10), ("!",81,86)],
        "M_QUITG": [("I",23,28), ("Q",0,11), ("T",28,37), ("U",11,22)],
        "M_OPTION": [("O",0,10), ("P",11,21)],
        "M_SAVEG": [("S",0,9), ("V",20,30)],
        "M_ROUGH": [("Y",111,122)],
        "M_ULTRA": [("X",8,18)],
        "M_SCRNSZ": [("Z",79,87)],
    }
    glyphs = {}
    for key, spans in crops.items():
        w, h, pixels = bitmap(lumps[key])
        for ch, left, right in spans:
            right = min(w, right)
            glyphs[ch] = (right-left, h, b"".join(
                pixels[y*w+left:y*w+right] for y in range(h)))
    w, h, pixels = glyphs["U"]
    hook = bytearray(pixels)
    for y in range(7):
        hook[y*w:y*w+6] = bytes(6)
    glyphs["J"] = w, h, hook
    # The original HUD alphabet supplies the remaining characters and digits.
    small = []
    for code in range(33, 94):
        glyph = glyphs.get(chr(code))
        if glyph is None:
            w, h, pixels = bitmap(lumps.get(f"STCFN{code:03}", lumps["STCFN063"]))
            width, height = (w*3+1)//2, (h*3+1)//2
            glyph = width, height, bytes(pixels[(y*h//height)*w+x*w//width]
                for y in range(height) for x in range(width))
        w, h, pixels = glyph
        cart.add(f"SLIME{code:03}", pack(">8H",w,h,3,0,0,0,0,0) + pixels)
        # Use the hand-pixelled green HUD alphabet at its native text size.
        patch = lumps.get(f"STCFN{code:03}", lumps["STCFN063"])
        w, h, pixels = bitmap(patch)
        # Doom draws patches at pen_y - top_offset. This places the comma
        # below the apostrophe even though both source patches are 4px tall.
        top_offset = read("<h", patch, 6)[0]
        tile = bytearray(64)
        width = min(8, w)
        for y in range(max(0, top_offset), min(h, 8 + top_offset)):
            for x in range(width):
                tile[(y-top_offset)*8+x] = pixels[y*w+x*w//width]
        columns = [x for x in range(width) if any(tile[x::8])]
        left, right = columns[0], columns[-1] + 1
        small.append((right-left, b"".join(tile[y*8+left:y*8+right] for y in range(8))))
    for code, (width, pixels) in enumerate(small, 33):
        cart.add(f"MINI{code:03}", pack(">8H",width,8,3,0,0,0,0,0) + pixels)


def interface(cart, lumps):
    def add(dest, src):
        cart.add(dest, jagobj(lumps[src]))
    for dest,src in [("TITLE","TITLEPIC"),("BOSSBACK","CREDIT"),("INTERPIC","WIMAP0"),("STBAR","STBAR")]:
        add(dest,src)
    # ST_Init indexes this contiguous block by sbobj_e, starting at MINUS.
    add("MINUS","STTMINUS")
    for i in range(10):
        add(f"NUM_{i}",f"STTNUM{i}")
    add("PERCENT","STTPRCNT")
    for i,key in enumerate(("CKEY_B","CKEY_Y","CKEY_R","SKEY_B","SKEY_Y","SKEY_R")):
        add(key,f"STKEYS{i}")
    for i in range(10):
        add(f"WINUM{i}",f"WINUM{i}")
    for i in range(5):
        add(f"WILV0{i}",f"WILV0{i}")
    for key in ("WIPCNT","WICOLON","WITIME","WIPAR"):
        add(key,key)
    for i in range(10):
        add(f"MICRO_{i}",f"STYSNUM{i}")
    for i in range(65,91):
        add(f"CHAR_{i:03}",f"STCFN{i:03}")
    for i, char in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ.!-?", 97):
        add(f"CHAR_{i:03}", f"STCFN{ord(char):03}")
    faces = [form.format(i) for i in range(5) for form in
             ("STFST{}0","STFST{}1","STFST{}2","STFTL{}0","STFTR{}0","STFOUCH{}","STFEVL{}","STFKILL{}")]
    for i,src in enumerate(faces+["STFGOD0","STFDEAD0"]+["STFDEAD0"]*6):
        add(f"FACE{i:02}",src)
    for i,src in enumerate(("M_JKILL","M_ROUGH","M_HURT","M_ULTRA","M_NMARE")):
        add(f"SKILL{i}",src)
    for dest,src in {"M_DOOM":"M_DOOM","M_SKULL1":"M_SKULL1","M_SKULL2":"M_SKULL2",
                     "PAUSED":"M_PAUSE","I_SECRET":"WISCRT2","I_KILLS":"WIOSTK","I_ITEMS":"WIOSTI",
                     "I_FINISH":"WIF","I_LEVEL":"WIENTER","O_SLIDER":"M_THERMO","O_STRAML":"M_THERML",
                     "O_STRAMM":"M_THERMM","O_STRAMR":"M_THERMR"}.items():
        add(dest,src)
    w, h, logo = bitmap(lumps["M_DOOM"])
    for scale, key in ((2,"M_LOGO2"),(3,"M_LOGO3")):
        width, height = w//scale, h//scale
        pixels = bytes(logo[(y*scale+scale//2)*w+x*scale+scale//2]
                       for y in range(height) for x in range(width))
        cart.add(key,pack(">8H",width,height,3,0,0,0,0,0)+pixels)
    menu_font(cart, lumps)
    for key in ("M_NGAME","M_OPTION","M_LOADG","M_SAVEG","M_RDTHIS","M_QUITG","M_SKILL"):
        add(key,key)
    # Compose JOIN GAME from the same menu artwork. Shorten the left stem
    # of the U in QUIT to make a J, preserving its hooked base and shadow.
    letters = [("M_QUITG",11,11), ("M_OPTION",0,10), ("M_QUITG",23,5),
               ("M_NGAME",0,11), ("M_NGAME",36,48)]
    width = sum(span for _,_,span in letters)
    image = bytearray(width*15)
    left = 0
    for key,x,span in letters:
        stride,height,src = bitmap(lumps[key])
        for y in range(height):
            row = src[y*stride+x:y*stride+x+span]
            if left == 0 and y < 7:
                row[:6] = bytes(6)
            image[y*width+left:y*width+left+span] = row
        left += span
    cart.add("M_JOIN",pack(">8H",width,15,3,0,0,0,0,0)+image)
    cart.add("CONCHARS",console_font(lumps))
    cart.add("M_HELP",(FILES/"m_help").read_bytes())


def attract_demo(recording):
    # Translate the DOS 35 Hz commands into the console's 15 Hz pad stream.
    skill, episode, level = recording[1:4]
    commands = list(struct.iter_unpack("bbbB", recording[13:-1]))
    output = bytearray(pack(">II", skill, level))
    for tick in range((len(commands) * 15) // 35):
        frame = commands[tick * 35 // 15:(tick + 1) * 35 // 15]
        forward, side, turn, _ = frame[-1]
        buttons = 0
        for *_, action in frame:
            buttons |= (action & 3) << 4
        if forward:
            buttons |= 4 if forward > 0 else 8
        if side:
            buttons |= 0x4000 if side > 0 else 0x2000
        if turn:
            buttons |= 2 if turn > 0 else 1
        if abs(forward) > 25 or abs(side) > 24:
            buttons |= 0x80
        output.extend(pack(">I", buttons))
    output.extend(pack(">I", 0x200000))
    return output


def convert(source):
    blob = source.read_bytes()
    if hashlib.md5(blob).hexdigest() != "25485721882b050afa96a56e5758dd52":
        raise ValueError("Supply chex.wad from the original 1996 release")
    entries = directory(blob)
    lumps = dict(entries)
    keys = [k for k,_ in entries]
    cart = Cartridge()
    for number in range(1, 5):
        key = f"DEMO{number}"
        cart.add(key, attract_demo(lumps[key]))
    cart.add("PLAYPALS",lumps["PLAYPAL"] + lumps["PLAYPAL"][:768])
    cmap = lumps["COLORMAP"].translate(OPAQUE_PIXELS)
    high = bytes(cmap[row*256+(i^128)] for row in range(33) for i in range(256))
    low = bytes(value for value in high for _ in range(2))
    for data in (low,low,high,high):
        cart.add("COLORMAP",data)
    interface(cart,lumps)
    cart.add("DMAPINFO",(ROOT/"campaign.txt").read_bytes()+b"\0")
    used = {"SKY1"}
    flats = {"FLOOR4_8","CEIL5_1"}
    masks = set()
    for i in range(1,6):
        start = keys.index(f"E1M{i}")
        mapdata = dict(entries[start+1:start+11])
        sides = list(struct.iter_unpack("<hh8s8s8sH",mapdata["SIDEDEFS"]))
        for x,y,a,b,c,sector in sides:
            used.update(map(name,(a,b,c)))
        for v1,v2,flags,special,tag,front,back in struct.iter_unpack("<7h",mapdata["LINEDEFS"]):
            if back>=0:
                masks.update(name(sides[index][4]) for index in (front,back))
    used.update("SW2"+k[3:] for k in tuple(used) if k.startswith("SW1"))
    for group in (("FIREMAG1","FIREMAG2","FIREMAG3"),("SLADRIP1","SLADRIP2","SLADRIP3"),
                  ("BLODGR1","BLODGR2","BLODGR3","BLODGR4"),("FIREBLU1","FIREBLU2")):
        if used.intersection(group): used.update(group)
        if masks.intersection(group): masks.update(group)
    texture_ids, mask_sprites = textures(cart,lumps,used,masks,cmap)
    for i in range(1,6):
        start = keys.index(f"E1M{i}")
        cart.add(f"MAP{i:02}")
        mapdata = dict(entries[start+1:start+11])
        for key,data in entries[start+1:start+11]:
            if key == "THINGS" and i in (3,4,5):
                # The second pad needs a start in these originally solo maps.
                x,y = {3:(-272,0),4:(-1168,448),5:(-2912,-208)}[i]
                angle = next(t[2] for t in struct.iter_unpack("<5h",data) if t[3]==1)
                data += pack("<5h",x,y,angle,2,7)
            if key in ("VERTEXES","BLOCKMAP"):
                words = struct.iter_unpack("<H",data)
                data = b"".join(pack(">H",word) for word, in words)
            elif key == "REJECT":
                n = len(mapdata["SECTORS"])//26
                bits = bytearray((n*(n+1)//2+7)//8)
                bit = 0
                for row in range(n):
                    for col in range(row,n):
                        index = row*n+col
                        if data[index//8] & (1<<(index%8)):
                            bits[bit//8] |= 1<<(bit%8)
                        bit += 1
                data = bits
            elif key == "SEGS":
                data = b"".join(pack(">HHH",line*2+side,v1,v2) for v1,v2,angle,line,side,off in struct.iter_unpack("<6H",data))
            elif key == "SSECTORS":
                data = b"".join(pack(">H",count) for count,first in struct.iter_unpack("<2H",data))
            elif key == "NODES":
                for fname,payload in (("nodes",data),("vertices",mapdata["VERTEXES"])):
                    (BUILD/fname).write_bytes(payload)
                subprocess.run([BUILD/"compnodes",BUILD/"nodes",BUILD/"vertices",BUILD/"compact"],check=True)
                data = (BUILD/"compact").read_bytes()
            elif key == "SIDEDEFS":
                data = b"".join(pack(">HBBBBH",sector,texture_ids[name(a)],texture_ids[name(b)],
                                     texture_ids[name(c)],y&255,(x&4095)|((y&0xf00)<<4))
                                for x,y,a,b,c,sector in struct.iter_unpack("<hh8s8s8sH",data))
            elif key == "SECTORS":
                for floor,ceil,a,b,light,special,tag in struct.iter_unpack("<hh8s8shhh",data):
                    flats.update(map(name,(a,b)))
            cart.add(key,data)
    cart.add("ENDMAPS")
    cart.add("F_START")
    for key,data in entries[keys.index("F_START")+1:keys.index("F_END")]:
        if key in flats or key.startswith(("NUKAGE","FWATER","SWATER","LAVA","BLOOD","SLIME")):
            cart.add(key,data.translate(OPAQUE_PIXELS))
    cart.add("F_END")
    cart.add("M_START")
    for song,data in soundtrack(lumps,BUILD):
        cart.add(song,data)
    cart.add("M_END")
    cart.add("DS_START")
    for key,data in lumps.items():
        if key.startswith("DS") and len(data)>8 and data[:2]==b"\x03\0":
            rate,count = read("<HI",data,2)
            pcm = data[8:8+count]
            target = BUILD/(key+".wav")
            if not target.exists():
                subprocess.run(["ffmpeg","-v","error","-f","u8","-ar",str(rate),"-ac","1",
                                "-i","pipe:0","-c:a","adpcm_ima_wav","-block_size","256",
                                "-fflags","+bitexact",str(target)],input=pcm,check=True)
            cart.add(key[2:],target.read_bytes())
    cart.add("DS_END")
    pixels = []
    cart.add("S_START")
    for key,data in entries[keys.index("S_START")+1:keys.index("S_END")]:
        if key[:4] in {"CYBR","SPID","HEAD","SKUL"}:
            continue
        header,body = sprite(data)
        cart.add(key,header)
        pixels.append((cart.store(body),len(body)))
    for key,(header,body) in mask_sprites:
        cart.add(key,header)
        pixels.append((cart.store(body),len(body)))
    cart.add("S_END")
    return cart.finish(pixels)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source",type=Path)
    parser.add_argument("output",type=Path)
    args = parser.parse_args()
    args.output.write_bytes(convert(args.source))
