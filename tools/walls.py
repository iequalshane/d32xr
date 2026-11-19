"""Choose a sixteen-entry local palette for the 32X four-bit wall drawer."""
from collections import Counter
from struct import pack


def wall_image(pixels,w,h,pal,cmap):
    if len(pixels)<3072 or h%2:
        return pixels
    colors = list(zip(pal[::3],pal[1::3],pal[2::3]))[:256]
    freq = Counter(pixels)
    buckets = [list(freq)]
    while len(buckets)<16:
        candidates = []
        for i,group in enumerate(buckets):
            if len(group)<2:
                continue
            for axis in range(3):
                values = [colors[c][axis] for c in group]
                candidates.append(((max(values)-min(values))**2*sum(freq[c] for c in group),i,axis))
        if not candidates:
            break
        _,index,axis = max(candidates)
        group = sorted(buckets.pop(index),key=lambda c:colors[c][axis])
        half = sum(freq[c] for c in group)/2
        acc = 0
        for cut,c in enumerate(group[:-1],1):
            acc += freq[c]
            if acc>=half:
                break
        buckets.extend([group[:cut],group[cut:]])
    representatives = []
    for group in buckets:
        total = sum(freq[c] for c in group)
        mean = [sum(colors[c][i]*freq[c] for c in group)/total for i in range(3)]
        representatives.append(min(group,key=lambda c:sum((v-m)**2 for v,m in zip(colors[c],mean))))
    lut = {c:min(range(len(representatives)),key=lambda i:sum((a-b)**2 for a,b in zip(colors[c],colors[representatives[i]]))) for c in freq}
    packed = bytes(lut[a]*16+lut[b] for a,b in zip(pixels[::2],pixels[1::2]))
    representatives.extend([0]*(16-len(representatives)))
    lighting = bytes(cmap[level*256+c] for level in range(33) for c in representatives for _ in range(2))
    return pack(">8H",w,h,2,0,9,0,0,0)+packed+lighting
