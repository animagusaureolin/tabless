"""Build the Tabless logo package: outlined wordmark SVGs, app icon SVG/PNG/ICO, favicon.

Wordmark: "Tabless" in Golos Text Medium, shaped with HarfBuzz (font kerning), tracking -3.5 %,
"Tab" underlined by a rounded lavender bar — the way the app marks the active tab.
Icon: a "T" whose crossbar is the lavender bar (one tab), stem in ink, on a dark tile.
Carried over from the lowkey package (2026-10-08): font, tracking, bar proportions, colours, tile.
Run: python build_logo.py   (needs fontTools, uharfbuzz, Pillow)
"""
import json, os
import uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, 'fonts', 'GolosText-Medium.ttf')
OUT = os.path.join(HERE, 'logo')
os.makedirs(OUT, exist_ok=True)

INK_DARK, LAV_DARK = '#e9edf2', '#b1a7f0'   # on dark backgrounds
INK_LIGHT, LAV_LIGHT = '#0c0e11', '#5a4fb3'  # on light backgrounds
TILE, TILE_EDGE = '#16191f', '#262c34'

TEXT = 'Tabless'
UNDERLINE = (0, 3)      # glyph range [start, end) under the bar: "Tab"
TRACK = -0.035          # em
BAR_H = 0.075           # em, underline thickness
BAR_GAP = 0.135         # em, from baseline down to the bar's top

# ---------- wordmark geometry (font units, y down after flip) ----------
font = TTFont(FONT)
upem = font['head'].unitsPerEm
gs = font.getGlyphSet()
hbfont = hb.Font(hb.Face(hb.Blob.from_file_path(FONT)))
buf = hb.Buffer()
buf.add_str(TEXT)
buf.guess_segment_properties()
hb.shape(hbfont, buf, {'kern': True, 'liga': False})
names = [font.getGlyphName(i.codepoint) for i in buf.glyph_infos]
track = TRACK * upem

paths, x, starts = [], 0.0, []
for name, pos in zip(names, buf.glyph_positions):
    starts.append(x)
    pen = SVGPathPen(gs)
    gs[name].draw(TransformPen(pen, (1, 0, 0, -1, x + pos.x_offset, -pos.y_offset)))
    paths.append(pen.getCommands())
    x += pos.x_advance + track


def bounds(i):
    bp = BoundsPen(gs)
    gs[names[i]].draw(bp)
    xmin, ymin, xmax, ymax = bp.bounds
    return xmin + starts[i], ymin, xmax + starts[i], ymax


boxes = [bounds(i) for i in range(len(names))]
ink_left, ink_right = boxes[0][0], boxes[-1][2]
u0, u1 = UNDERLINE
bar = dict(x=boxes[u0][0], y=BAR_GAP * upem, w=boxes[u1 - 1][2] - boxes[u0][0], h=BAR_H * upem)
asc = max(b[3] for b in boxes)
desc = max(max(-b[1] for b in boxes), bar['y'] + bar['h'])
vb = (ink_left, -asc, ink_right - ink_left, asc + desc)
glyph_d = ' '.join(paths)
r = bar['h'] / 2


def wordmark_svg(ink, acc, title='Tabless'):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb[0]:.1f} {vb[1]:.1f} {vb[2]:.1f} {vb[3]:.1f}" role="img" aria-label="{title}">'
            f'<path d="{glyph_d}" fill="{ink}"/>'
            f'<rect x="{bar["x"]:.1f}" y="{bar["y"]:.1f}" width="{bar["w"]:.1f}" height="{bar["h"]:.1f}" rx="{r:.1f}" fill="{acc}"/></svg>\n')


for fn, (ink, acc) in {
    'tabless-wordmark-on-dark.svg': (INK_DARK, LAV_DARK),
    'tabless-wordmark-on-light.svg': (INK_LIGHT, LAV_LIGHT),
    'tabless-wordmark-black.svg': ('#000000', '#000000'),
    'tabless-wordmark-white.svg': ('#ffffff', '#ffffff'),
}.items():
    open(os.path.join(OUT, fn), 'w', encoding='utf-8').write(wordmark_svg(ink, acc))

# ---------- icon: "T" with a lavender crossbar, 1024 grid ----------
# 32-grid mark: crossbar x7 y6 18x5 (accent), stem x13.5 y6 5x20 (ink); ×~23, centred
ICON = dict(size=1024, radius=230, cross=(302, 302, 420, 92), stem=(466, 302, 92, 420))


def icon_svg(tile, edge, ink, acc, size=1024):
    i = ICON
    (cx, cy, cw, ch), (sx, sy, sw, sh) = i['cross'], i['stem']
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 1024 1024" role="img" aria-label="Tabless">'
            f'<rect x="8" y="8" width="1008" height="1008" rx="{i["radius"]}" fill="{tile}" stroke="{edge}" stroke-width="16"/>'
            f'<rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="{sw // 2}" fill="{ink}"/>'
            f'<rect x="{cx}" y="{cy}" width="{cw}" height="{ch}" rx="{ch // 2}" fill="{acc}"/></svg>\n')


open(os.path.join(OUT, 'tabless-icon.svg'), 'w').write(icon_svg(TILE, TILE_EDGE, INK_DARK, LAV_DARK))
open(os.path.join(OUT, 'tabless-icon-light.svg'), 'w').write(icon_svg('#ffffff', '#dde3ec', INK_LIGHT, LAV_LIGHT))
open(os.path.join(OUT, 'favicon.svg'), 'w').write(
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">'
    '<style>.s{fill:#0c0e11}.b{fill:#5a4fb3}@media (prefers-color-scheme:dark){.s{fill:#e9edf2}.b{fill:#b1a7f0}}</style>'
    '<rect class="s" x="13.5" y="6" width="5" height="20" rx="2.5"/><rect class="b" x="7" y="6" width="18" height="5" rx="2.5"/></svg>\n')


def hex2rgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def icon_png(px, tile=TILE, edge=TILE_EDGE, ink=INK_DARK, acc=LAV_DARK):
    if px <= 32:   # hand-snapped to the pixel grid: whole-pixel strokes, no blur
        im = Image.new('RGBA', (px, px), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        d.rounded_rectangle((0, 0, px - 1, px - 1), max(3, round(px * 0.22)), fill=hex2rgb(tile) + (255,), outline=hex2rgb(edge) + (255,))
        t = 2 if px <= 16 else (3 if px <= 24 else 4)
        x0, x1 = round(px * 0.27), px - round(px * 0.27)
        top, bottom = round(px * 0.27), px - round(px * 0.25)
        sx = (px - t) // 2
        d.rectangle((sx, top, sx + t - 1, bottom - 1), fill=hex2rgb(ink) + (255,))
        d.rectangle((x0, top, x1 - 1, top + t - 1), fill=hex2rgb(acc) + (255,))
        return im
    s = 4  # supersample
    big = Image.new('RGBA', (px * s, px * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    k = px * s / 1024
    i = ICON
    d.rounded_rectangle((8 * k, 8 * k, 1016 * k, 1016 * k), i['radius'] * k, fill=hex2rgb(tile) + (255,),
                        outline=hex2rgb(edge) + (255,), width=max(1, round(16 * k)))
    for (x, y, w, h), col in ((i['stem'], ink), (i['cross'], acc)):
        d.rounded_rectangle((x * k, y * k, (x + w) * k, (y + h) * k), min(w, h) * k / 2, fill=hex2rgb(col) + (255,))
    return big.resize((px, px), Image.LANCZOS)


png_dir = os.path.join(OUT, 'png')
os.makedirs(png_dir, exist_ok=True)
sizes = [16, 24, 32, 48, 64, 128, 256, 512, 1024]
imgs = {}
for px in sizes:
    imgs[px] = icon_png(px)
    imgs[px].save(os.path.join(png_dir, f'tabless-icon-{px}.png'))
    icon_png(px, '#ffffff', '#dde3ec', INK_LIGHT, LAV_LIGHT).save(os.path.join(png_dir, f'tabless-icon-light-{px}.png'))
imgs[256].save(os.path.join(OUT, 'tabless.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
               append_images=[imgs[p] for p in (16, 24, 32, 48, 64, 128)])

json.dump({'viewBox': vb, 'glyphs': glyph_d, 'bar': bar, 'radius': r, 'upem': upem, 'icon': ICON},
          open(os.path.join(OUT, 'geometry.json'), 'w'))
print('wordmark', [round(v) for v in vb], 'bar', {k: round(v) for k, v in bar.items()}, 'files', sorted(os.listdir(OUT)))
