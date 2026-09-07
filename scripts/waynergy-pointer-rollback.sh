#!/bin/sh
set -eu

commit=/run/display/waynergy-pointer.commit
compositor=/usr/share/lipstick-jolla-home-qt5/compositor.qml
backup=/usr/share/lipstick-jolla-home-qt5/compositor.qml.waynergy-pointer.orig
environment=/var/lib/environment/compositor/waynergy-pointer.conf

[ -e "$commit" ] && exit 0

install -m 0644 "$backup" "$compositor"
rm -f "$environment" /run/display/waynergy-pointer.sock
su - nemo -c 'XDG_RUNTIME_DIR=/run/user/100000 systemctl --user restart lipstick.service'
