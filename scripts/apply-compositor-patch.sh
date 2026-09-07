#!/bin/sh
set -eu

# Run as root. The argument is the path to the staged repository; default to
# the conventional developer-mode staging path.
repo=${1:-/home/nemo/cursor-build/sailfish-waynergy-pointer}
compositor=/usr/share/lipstick-jolla-home-qt5/compositor.qml
backup=${compositor}.waynergy-pointer.orig
patch_file=$repo/lipstick/0001-global-pointer-overlay.patch

test -r "$patch_file"
if [ ! -e "$backup" ]; then
    install -m 0644 "$compositor" "$backup"
fi

if ! patch --dry-run -p1 -d "$(dirname "$compositor")" < "$patch_file"; then
    echo "Patch does not match this Lipstick compositor version; restoring nothing." >&2
    exit 1
fi
patch -p1 -d "$(dirname "$compositor")" < "$patch_file"
grep -q 'import Waynergy.Pointer 1.0' "$compositor"
echo "Lipstick pointer overlay patch applied. Restart the user lipstick.service to load it."
