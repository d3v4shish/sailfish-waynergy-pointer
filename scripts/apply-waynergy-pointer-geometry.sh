#!/bin/sh
set -eu

install -m 0644 /home/nemo/cursor-build/compositor.qml.waynergy-pointer \
    /usr/share/lipstick-jolla-home-qt5/compositor.qml
grep -q 'screenWidth: Math.min(root.width, root.height)' /usr/share/lipstick-jolla-home-qt5/compositor.qml
grep -q 'pointerResyncTimer' /usr/share/lipstick-jolla-home-qt5/compositor.qml
sha256sum /usr/share/lipstick-jolla-home-qt5/compositor.qml \
    > /var/lib/waynergy-pointer/compositor.sha256
echo WAYNERGY-POINTER-GEOMETRY-APPLIED
