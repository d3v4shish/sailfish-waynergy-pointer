#!/bin/sh
set -eu

chroot=/home/nemo/chroots/deskflow-bookworm
build=/opt/build-waynergy

# Meson's VCS stamp imports Python, but this deliberately minimal chroot has no
# entropy device.  The version string is informational, so generate the same
# header deterministically before Ninja evaluates that target.
chroot "$chroot" /bin/sh -lc "cd $build && sed 's/@VCS_DESC@/local-pointer-feed/' ../src/waynergy-master/include/ver.h.in > include/ver.h && sed -i '/vcstagger/c\\ COMMAND = /bin/true' build.ninja && ninja && install -m 0755 waynergy /usr/local/bin/waynergy && sha256sum waynergy /usr/local/bin/waynergy"
echo WAYNERGY-BUILD-APPLIED
