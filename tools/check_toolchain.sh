#!/usr/bin/env bash
#
# Verifie que les outils necessaires a la compilation de
# MikeaOS sont installes, et affiche clairement ce qui manque
# au lieu de laisser make echouer avec une erreur cryptique.

set -e

MISSING=0

check() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "MANQUANT : $1  ($2)"
        MISSING=1
    else
        echo "OK       : $1"
    fi
}

check nasm                  "assembleur, requis pour le bootloader et l'entree noyau"
check x86_64-elf-gcc        "compilateur croise C, requis pour le noyau"
check x86_64-elf-ld         "editeur de liens croise, requis pour l'ELF final"
check x86_64-elf-objcopy    "conversion ELF -> binaire brut"
check qemu-system-x86_64    "emulateur, requis pour 'make run'"

if [ "$MISSING" -eq 1 ]; then
    echo ""
    echo "Des outils manquent. Sur macOS (Homebrew) :"
    echo "  brew install nasm qemu x86_64-elf-gcc x86_64-elf-binutils"
    echo "Sur Linux, ces paquets sont generalement a compiler soi-meme"
    echo "(binutils/gcc en cross-compilation) ou via un tuto OSDev."
    exit 1
fi

echo ""
echo "Tous les outils necessaires sont presents."
