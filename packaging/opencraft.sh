#!/bin/sh
set -eu
prefix=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
assets="$prefix/share/opencraft"
binary="$prefix/bin/opencraft-bin"
data="${OPENCRAFT_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/opencraft}"
if [ -z "${OPENCRAFT_DATA_DIR:-}" ] && [ ! -e "$data" ]; then
    legacy="${XDG_DATA_HOME:-$HOME/.local/share}/openminer"
    if [ -d "$legacy" ]; then
        mv "$legacy" "$data"
    fi
fi
mkdir -p "$data/config" "$data/saves" "$data/logs"
for directory in mods resources; do
    if [ ! -e "$data/$directory" ] || [ -L "$data/$directory" ]; then
        ln -sfn "$assets/$directory" "$data/$directory"
    fi
done
cd "$data"
exec "$binary" "$@"
