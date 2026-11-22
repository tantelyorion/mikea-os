#!/usr/bin/env python3
"""
Empaqueteur d'assets Mikea OS  ->  assets/assets.pak

Les photos de fond d'ecran (RVB brut) et les sons (PCM 8 bits) ne sont
plus compiles DANS le noyau (ils en occupaient ~260 Ko sur 500 Ko) : ils
vivent sur le disque de demarrage, juste apres le noyau, et sont lus a la
demande par kernel/assets/assets.c.

Disposition du disque de boot (secteurs de 512 octets) :
    0        boot.bin
    1-64     stage2
    97-1096  noyau (1000 secteurs max)
    1100+    assets.pak  (ce fichier)           <-- ASSETS_PAK_LBA
    <2048    fin des secteurs copies par l'installeur (INSTALLER_SECTOR_COUNT)

Format du pack :
    secteur 0 : en-tete  magic "MKAP", version=1, count, base_lba
                puis count entrees de 32 octets :
                  char name[16]; u32 sector (relatif au pack); u32 size;
                  u16 width; u16 height; u32 reserve
    secteurs suivants : donnees alignees sur 512 octets.

Usage :
    python3 tools/pack_assets.py            # reconstruit assets/assets.pak
    python3 tools/pack_assets.py extract    # (migration) extrait depuis les anciens .c
Ajoutez vos propres fonds : deposez un .rgb (width*height*3 octets) dans
assets/wallpapers/ et declarez-le dans WALLPAPERS ci-dessous.
"""
import os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAK_LBA = 1100
MAX_SECTORS = 2048 - PAK_LBA          # place disponible avant la fin de l'image

# (nom dans le pack, fichier, largeur, hauteur)
WALLPAPERS = [
    ("wp_brume",     "assets/wallpapers/brume.rgb",     112, 84),
    ("wp_collines",  "assets/wallpapers/collines.rgb",  112, 84),
    ("wp_cameleon",  "assets/wallpapers/cameleon.rgb",  112, 84),
    ("wp_lune",      "assets/wallpapers/lune.rgb",      112, 84),
    ("wp_nebuleuse", "assets/wallpapers/nebuleuse.rgb", 112, 84),
    ("wp_satellite", "assets/wallpapers/satellite.rgb", 112, 84),
]
SOUNDS = [
    ("snd_startup",  "assets/sounds/startup.pcm"),
    ("snd_shutdown", "assets/sounds/shutdown.pcm"),
]


def extract():
    """Migration unique : retrouve les octets dans les anciens tableaux C."""
    src = open(os.path.join(ROOT, "gui/assets/wallpaper_images.c"), encoding="utf-8").read()
    os.makedirs(os.path.join(ROOT, "assets/wallpapers"), exist_ok=True)
    for m in re.finditer(r"WALLPAPER_(\w+)_DATA\[\d+\] = \{(.*?)\};", src, re.S):
        data = bytes(int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2)))
        path = os.path.join(ROOT, "assets/wallpapers", m.group(1).lower() + ".rgb")
        open(path, "wb").write(data)
        print("extrait", path, len(data))
    src = open(os.path.join(ROOT, "kernel/drivers/soundblaster/sound_data.c"), encoding="utf-8").read()
    os.makedirs(os.path.join(ROOT, "assets/sounds"), exist_ok=True)
    for name in ("startup", "shutdown"):
        m = re.search(name + r"_pcm\[\] = \{(.*?)\};", src, re.S)
        data = bytes(int(v) for v in re.findall(r"\b(\d{1,3})\b", m.group(1)))   # decimal
        expected = int(re.search(name + r"_pcm_len = (\d+);", src).group(1))
        if len(data) != expected:
            sys.exit("%s : %d octets extraits, attendu %d" % (name, len(data), expected))
        path = os.path.join(ROOT, "assets/sounds", name + ".pcm")
        open(path, "wb").write(data)
        print("extrait", path, len(data))


def pack():
    entries = []
    for name, rel, w, h in WALLPAPERS:
        data = open(os.path.join(ROOT, rel), "rb").read()
        if len(data) != w * h * 3:
            sys.exit("%s : %d octets, attendu %d (%dx%dx3)" % (rel, len(data), w * h * 3, w, h))
        entries.append((name, data, w, h))
    for name, rel in SOUNDS:
        data = open(os.path.join(ROOT, rel), "rb").read()
        if not data:
            sys.exit("%s est vide" % rel)
        entries.append((name, data, 0, 0))
    if len(entries) > 15:
        sys.exit("15 entrees maximum")

    body, table, sector = bytearray(), [], 1
    for name, data, w, h in entries:
        table.append(struct.pack("<16sIIHHI", name.encode(), sector, len(data), w, h, 0))
        padded = data + b"\0" * (-len(data) % 512)
        body += padded
        sector += len(padded) // 512
    if sector > MAX_SECTORS:
        sys.exit("pack trop gros : %d secteurs (max %d avant la fin de l'image)" % (sector, MAX_SECTORS))

    header = struct.pack("<4sIII", b"MKAP", 1, len(entries), PAK_LBA) + b"".join(table)
    header += b"\0" * (512 - len(header))
    out = os.path.join(ROOT, "assets/assets.pak")
    open(out, "wb").write(header + body)
    print("ecrit %s : %d entrees, %d secteurs (%d Ko), place libre : %d secteurs"
          % (out, len(entries), sector, sector // 2, MAX_SECTORS - sector))


if __name__ == "__main__":
    extract() if sys.argv[1:] == ["extract"] else pack()
