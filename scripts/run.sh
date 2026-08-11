#!/usr/bin/env bash
#
# Lance MikeaOS dans QEMU. Compile d'abord si l'image n'existe
# pas encore.

set -e

cd "$(dirname "$0")/.."

if [ ! -f build/MikeaOS.img ]; then
    make all
fi

make run
