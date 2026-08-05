#!/usr/bin/env bash
#
# Nettoie tous les fichiers generes par la compilation.

set -e

cd "$(dirname "$0")/.."

make clean
