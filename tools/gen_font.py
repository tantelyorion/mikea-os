#!/usr/bin/env python3
"""
Generateur des polices anti-aliasees de Mikea OS.

Produit assets/fonts/font_ui.h : glyphes en niveaux de gris 4 bits
(16 niveaux de couverture), tailles rognees (bbox), largeurs
proportionnelles, pour un rendu en entiers dans le noyau (SSE desactive).

Usage :  python3 tools/gen_font.py
Dependance (hote seulement) : Pillow.
Polices sources (OFL) : Poppins Regular (interface), DejaVu Sans Mono (grille).
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "fonts", "font_ui.h")

UI_TTF = "/usr/share/fonts/truetype/google-fonts/Poppins-Regular.ttf"
MONO_TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

# (identifiant C, ttf, taille px)
FONTS = [
    ("UI_SMALL", UI_TTF, 13),
    ("UI_BODY",  UI_TTF, 16),
    ("UI_TITLE", UI_TTF, 22),
    ("MONO",     MONO_TTF, 18),
]

EXTRA = "àâäçéèêëîïôöùûüÿœÀÂÄÇÉÈÊËÎÏÔÖÙÛÜŒ«»…–—’‘“”€°·×÷•"
CODEPOINTS = [c for c in range(32, 127)] + sorted(ord(c) for c in EXTRA)


def build(ttf, size):
    font = ImageFont.truetype(ttf, size, layout_engine=ImageFont.Layout.BASIC)
    asc, desc = font.getmetrics()
    glyphs, blob = [], bytearray()
    pad = size
    for cp in CODEPOINTS:
        ch = chr(cp)
        adv = int(round(font.getlength(ch)))
        img = Image.new("L", (size * 3, size * 3), 0)
        d = ImageDraw.Draw(img)
        d.text((pad, pad * 2), ch, font=font, fill=255, anchor="ls")
        bb = img.getbbox()
        if bb is None:
            glyphs.append((cp, 0, 0, 0, 0, adv, len(blob)))
            continue
        l, t, r, b = bb
        w, h = r - l, b - t
        xoff, yoff = l - pad, t - pad * 2          # yoff < 0 : au-dessus de la ligne de base
        px = [min(15, (img.getpixel((l + x, t + y)) + 8) // 17) for y in range(h) for x in range(w)]
        if len(px) % 2:
            px.append(0)
        off = len(blob)
        for i in range(0, len(px), 2):
            blob.append((px[i] << 4) | px[i + 1])
        glyphs.append((cp, w, h, xoff, yoff, adv, off))
    return asc, desc, glyphs, blob


def main():
    lines = [
        "/* GENERE par tools/gen_font.py -- ne pas editer a la main.",
        "   Glyphes anti-aliases 4 bits/pixel, largeurs proportionnelles.",
        "   Polices : Poppins (SIL OFL 1.1), DejaVu Sans Mono (licence libre Bitstream Vera). */",
        "#ifndef MIKEA_FONT_UI_H", "#define MIKEA_FONT_UI_H", "",
        '#include "../../include/types.h"', "",
        "typedef struct { u16 cp; u8 w, h; signed char xoff, yoff; u8 adv; u16 off; } ui_glyph;",
        "typedef struct { const ui_glyph* glyphs; u16 count; const u8* data; u8 ascent, height; } ui_font;", "",
    ]
    total = 0
    names = []
    for ident, ttf, size in FONTS:
        asc, desc, glyphs, blob = build(ttf, size)
        total += len(blob) + len(glyphs) * 9
        n = ident.lower()
        names.append((ident, n, asc, asc + desc, len(glyphs)))
        lines.append(f"static const u8 {n}_data[{len(blob)}] = {{")
        for i in range(0, len(blob), 24):
            lines.append("    " + ",".join(str(b) for b in blob[i:i+24]) + ",")
        lines.append("};")
        lines.append(f"static const ui_glyph {n}_glyphs[{len(glyphs)}] = {{")
        for g in glyphs:
            lines.append("    {%d,%d,%d,%d,%d,%d,%d}," % g)
        lines.append("};\n")
    lines.append("static const ui_font UI_FONTS[%d] = {" % len(names))
    for ident, n, asc, hgt, cnt in names:
        lines.append(f"    {{ {n}_glyphs, {cnt}, {n}_data, {asc}, {hgt} }},  /* {ident} */")
    lines.append("};\n")
    for i, (ident, *_r) in enumerate(names):
        lines.append(f"#define FONT_{ident} {i}")
    lines += ["", "#endif", ""]
    with open(OUT, "w") as f:
        f.write("\n".join(lines))
    print("ecrit", OUT, "-> ~%d octets de donnees (rodata)" % total)


if __name__ == "__main__":
    main()
