#!/usr/bin/env bash
#
# Compile MikeaOS et genere l'ISO (build/MikeaOS.iso).
# Necessite la toolchain croisee : nasm, x86_64-elf-gcc,
# x86_64-elf-ld, x86_64-elf-objcopy (voir tools/check_toolchain.sh).

set -e

cd "$(dirname "$0")/.."

make all
