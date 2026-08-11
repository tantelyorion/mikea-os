#!/usr/bin/env bash
#
# Compile MikeaOS et genere l'image disque (build/MikeaOS.img).
# Necessite la toolchain croisee : nasm, x86_64-elf-gcc,
# x86_64-elf-ld, x86_64-elf-objcopy (voir tools/check_toolchain.sh).

set -e

cd "$(dirname "$0")/.."

make all
