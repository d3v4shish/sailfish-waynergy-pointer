#!/bin/sh
# Make the source archive consumed by the Sailfish RPM spec.
set -eu

version=${1:?usage: build-source-archive.sh VERSION OUTPUT_DIRECTORY}
out=${2:?usage: build-source-archive.sh VERSION OUTPUT_DIRECTORY}
repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
name=sailfish-deskflow-setup-$version
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT INT TERM

mkdir -p "$out"
git -C "$repo_root" archive --format=tar --prefix="$name/" HEAD | tar -x -C "$stage"
# Local, uncommitted installer work must never silently become a release
# artifact. The caller should commit it before making a release archive.
tar -C "$stage" -czf "$out/$name.tar.gz" "$name"
