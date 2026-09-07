#!/bin/sh
set -eu

stage=/home/nemo/cursor-build
compositor=/usr/share/lipstick-jolla-home-qt5/compositor.qml
backup=/usr/share/lipstick-jolla-home-qt5/compositor.qml.waynergy-pointer.orig
environment=/var/lib/environment/compositor/waynergy-pointer.conf
state=/var/lib/waynergy-pointer

mkdir -p "$state"
if [ ! -e "$backup" ]; then
    install -m 0644 "$compositor" "$backup"
fi
if [ -e "$environment" ]; then
    echo "Refusing to overwrite existing $environment" >&2
    exit 1
fi

install -m 0644 "$stage/compositor.qml.waynergy-pointer" "$compositor"
install -m 0644 "$stage/waynergy-pointer.conf" "$environment"
install -m 0700 "$stage/waynergy-pointer-rollback.sh" "$state/rollback.sh"
rm -f /run/display/waynergy-pointer.commit

grep -q 'import Waynergy.Pointer 1.0' "$compositor"
grep -q 'PointerOverlay' "$compositor"
systemd-run --unit=waynergy-pointer-rollback --on-active=180s "$state/rollback.sh"
echo WAYNERGY-POINTER-ACTIVATED
