"""Tile-aware CPS RGB888 -> two Genesis palettes, without early RGB333 loss.

Only original RGB values drive fitting. The quadratic error penalizes chroma
changes as well as brightness, avoiding an olive average for green foliage.
Normal Genesis Plus GX output is 0,34,...,238 (vdp_render.c MAKE_PIXEL).
"""
from collections import Counter
from functools import lru_cache
from itertools import product

LEVELS = tuple(i * 34 for i in range(8))


def color_error(a, b):
    r, g, blue = (x-y for x, y in zip(a, b))
    return r*r + g*g + blue*blue + 2*(r-g)**2 + 2*(g-blue)**2


@lru_cache(maxsize=None)
def nearest_hardware(rgb):
    choices = [sorted(set((min(7,int(v)//34), min(7,int(v)//34+1)))) for v in rgb]
    return min((tuple(LEVELS[i] for i in p) for p in product(*choices)),
               key=lambda c: (color_error(rgb,c), c))


def fit_palette(hist, count=15, required=()):
    colors=sorted(hist)
    required=list(dict.fromkeys(required))
    assert len(required)<=count and all(all(v in LEVELS for v in c) for c in required)
    if not colors: return required+[(0,0,0)]*(count-len(required))
    candidates=sorted({nearest_hardware(c) for c in colors}|set(required))
    distances=[[color_error(c,p) for p in candidates] for c in colors]
    weights=[hist[c] for c in colors]
    chosen=[candidates.index(c) for c in required]
    best=[min((d[j] for j in chosen),default=10**9) for d in distances]
    for _ in range(min(count,len(candidates))-len(chosen)):
        remaining=[j for j in range(len(candidates)) if j not in chosen]
        j=min(remaining,key=lambda j:sum(w*min(e,d[j]) for w,e,d in zip(weights,best,distances)))
        chosen.append(j)
        best=[min(e,d[j]) for e,d in zip(best,distances)]
    palette=[candidates[j] for j in chosen]
    # Refine in source RGB, then quantize the centroids just once per iteration.
    for _ in range(6):
        groups=[[] for _ in palette]
        for c in colors:
            j=min(range(len(palette)),key=lambda j:color_error(c,palette[j]))
            groups[j].append(c)
        updated=[]
        for index,(old,group) in enumerate(zip(palette,groups)):
            if index<len(required):
                updated.append(old);continue
            n=sum(hist[c] for c in group)
            mean=tuple(sum(c[k]*hist[c] for c in group)/n for k in range(3)) if n else old
            updated.append(nearest_hardware(mean))
        if updated==palette: break
        palette=updated
    # Do not spend two hardware slots on the same rounded color.
    palette=list(dict.fromkeys(palette))
    while len(palette)<min(count,len(candidates)):
        remaining=[c for c in candidates if c not in palette]
        c=min(remaining,key=lambda p:sum(hist[c]*min(color_error(c,p),min(color_error(c,q) for q in palette)) for c in colors))
        palette.append(c)
    return palette+[(0,0,0)]*(count-len(palette))


def fit_tile_palettes(cells, fixed_palettes=(), fixed_allowed_after=0, required_colors=((),()), forced_assignments=None):
    # Optional fixed pages may serve trailing sprite groups without changing
    # either player palette. Background tiles remain on the two fitted pages.
    assert 0 <= fixed_allowed_after <= len(cells)
    assert all(len(p) == 15 for p in fixed_palettes)
    # Every tile must select one complete palette; optimize that constraint.
    assert len(required_colors)==2
    forced_assignments=forced_assignments or {}
    assert all(0<=n<len(cells) and s in (0,1) for n,s in forced_assignments.items())
    histograms=[Counter(tile) for tile in cells]
    assignments=[int(sum(n for c,n in h.items() if c[1]>c[0]*1.2 and c[1]>c[2]*1.2)<32) for h in histograms]
    for n,s in forced_assignments.items():assignments[n]=s
    all_colors=sorted({c for h in histograms for c in h})
    best=None
    for iteration in range(8):
        hist=[Counter(),Counter()]
        for slot,h in zip(assignments,histograms):
            if slot < 2: hist[slot].update(h)
        palettes=[fit_palette(h,required=required_colors[n]) for n,h in enumerate(hist)]+[list(p) for p in fixed_palettes]
        lookup={c:[min((color_error(c,p),i+1) for i,p in enumerate(pal)) for pal in palettes] for c in all_colors}
        errors=[[sum(n*lookup[c][s][0] for c,n in h.items()) for s in range(len(palettes))] for h in histograms]
        next_assign=[min(range(2 if n < fixed_allowed_after else len(palettes)),key=lambda s:e[s]) for n,e in enumerate(errors)]
        for n,s in forced_assignments.items():next_assign[n]=s
        total=sum(e[slot] for e,slot in zip(errors,next_assign))
        if best is None or total<best[0]: best=(total,palettes,next_assign,lookup)
        if assignments==next_assign:break
        assignments=next_assign
    return best[1],best[2],best[3]
