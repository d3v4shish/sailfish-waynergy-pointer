#!/bin/sh
set -eu

archive=$1
version=$2
repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
sha256=$(sha256sum "$archive" | awk '{print $1}')
manifest=$repo_root/installer/rootfs-manifest.ini

sed -e "s/^version=.*/version=$version/" \
    -e "s#^url=.*#url=https://github.com/d3v4shish/sailfish-waynergy-pointer/releases/download/v$version/bookworm-armhf-minbase.tar.gz#" \
    -e "s/^sha256=.*/sha256=$sha256/" \
    "$manifest" > "$manifest.new"
mv "$manifest.new" "$manifest"
