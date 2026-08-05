#!/usr/bin/env bash
#
# Lance MikeaOS dans QEMU. Compile d'abord si l'ISO n'existe
# pas encore.

set -e

cd "$(dirname "$0")/.."

if [ ! -f build/MikeaOS.iso ]; then
    make all
fi

make run
