#!/bin/sh
set -eu

touch /run/display/waynergy-pointer.commit
systemctl stop waynergy-pointer-rollback.timer waynergy-pointer-rollback.service 2>/dev/null || true
systemctl reset-failed waynergy-pointer-rollback.service 2>/dev/null || true
sha256sum /usr/share/lipstick-jolla-home-qt5/compositor.qml > /var/lib/waynergy-pointer/compositor.sha256
echo WAYNERGY-POINTER-COMMITTED
