#!/bin/sh
# Does not print credential matches or access credential files.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "${PYTHON:-python3}" "$SCRIPT_DIR/scan.py" "$@"
