#!/bin/sh
# Build on an armhf-capable Debian Bookworm runner (native or binfmt/QEMU).
set -eu

out=${1:-release-output}
work=$(mktemp -d)
root=$work/rootfs
trap 'rm -rf "$work"' EXIT INT TERM

mkdir -p "$out" "$root"
debootstrap --arch=armhf --variant=minbase --components=main,contrib \
    bookworm "$root" http://deb.debian.org/debian
cat > "$root/etc/apt/sources.list" <<'EOF'
deb http://deb.debian.org/debian bookworm main contrib
deb http://deb.debian.org/debian bookworm-updates main contrib
deb http://deb.debian.org/debian-security bookworm-security main contrib
EOF
rm -rf "$root/var/cache/apt/archives" "$root/var/lib/apt/lists" "$root/tmp"/*
tar -C "$root" --numeric-owner --xattrs --acls -czf "$out/bookworm-armhf-minbase.tar.gz" .
sha256sum "$out/bookworm-armhf-minbase.tar.gz" > "$out/SHA256SUMS"
