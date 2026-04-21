#!/usr/bin/env bash
# run-sd-tools.sh — convert every .QOI capture in this directory to PNG and JPG.
#
# This just runs the two converters (qoi-to-png.py, qoi-to-jpg.py) that live
# beside it on all the .QOI files here, so you don't have to remember the
# python invocation or which venv has numpy/pillow.
#
# usage:
#   ./run-sd-tools.sh            # convert every .QOI in this dir
#   ./run-sd-tools.sh /Volumes/ADITI_PI   # or point it at the SD card / a dir
#
# (Per the qoi-to-png.py note: if reading straight off /Volumes/... gives
#  "Invalid argument", eject + reinsert the card first, or copy the files off.)

set -euo pipefail

# Directory this script lives in (the sd_tools dir).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Where to look for .QOI files: an arg if given, otherwise this directory.
SRC="${1:-$HERE}"

# Pick a Python that has numpy + pillow. Prefer the project venvs, then the
# system python3.
pick_python() {
    for py in \
        "$HERE/../laptop_code/venv/bin/python3" \
        "$HERE/../venv/bin/python3" \
        python3; do
        if "$py" -c "import numpy, PIL" >/dev/null 2>&1; then
            echo "$py"
            return 0
        fi
    done
    return 1
}

PY="$(pick_python)" || {
    echo "error: no python3 with numpy + pillow found (tried project venvs and system python3)" >&2
    exit 1
}

echo "using python: $PY"
echo "source:       $SRC"
echo

echo "== QOI -> PNG =="
"$PY" "$HERE/qoi-to-png.py" "$SRC"
echo

echo "== QOI -> JPG =="
# qoi-to-jpg.py only knows how to convert the .QOI files sitting next to it,
# so it only runs for the default (this directory) case.
if [ "$SRC" = "$HERE" ]; then
    "$PY" "$HERE/qoi-to-jpg.py"
else
    echo "(skipping JPG for external source; only the in-dir converter makes JPGs)"
fi
