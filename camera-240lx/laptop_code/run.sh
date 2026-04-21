#!/bin/bash
# set up the venv (first run only) and launch the photobooth display.
# usage: ./run.sh [serial-port]    e.g. ./run.sh /dev/tty.usbserial-310
set -e
cd "$(dirname "$0")"

if [ ! -d venv ]; then
    echo "creating venv..."
    python3 -m venv venv
    new_venv=1
fi

source venv/bin/activate

# install deps if the venv is fresh or anything is missing
if [ -n "$new_venv" ] || ! python -c "import serial, numpy, cv2, PIL" 2>/dev/null; then
    echo "installing requirements..."
    pip install -q -r requirements.txt
fi

python laptop-side-display.py "$@"
