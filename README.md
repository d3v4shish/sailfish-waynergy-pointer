# Sailfish Waynergy pointer

An experimental, GitHub-ready installer for sharing a Deskflow/Barrier mouse
and keyboard with a Sailfish OS phone. It builds a custom Waynergy client in a
Debian Bookworm armhf chroot, feeds pointer positions to Lipstick, and shows a
global visible cursor with calibration controls.

The packaged target is deliberately narrow: Sailfish OS 3.2 on `armv7hl`, with
the Jolla `lipstick-jolla-home-qt5` compositor revision whose stock
`compositor.qml` SHA-256 is recorded in `installer/root-helper`. It is not a
generic installer for other Sailfish versions.

For a detailed explanation of the compositor, Wayland and X11 models,
Deskflow-to-application input path, `uinput`, visible-pointer design, and
every integration change, read
[the architecture and integration guide](docs/architecture-and-integration.md).

## What is included

| Path | Purpose |
| --- | --- |
| `installer-app/` | Native Sailfish Silica setup application. It prompts for the developer password through a pseudo-terminal; the password is never written to disk. |
| `waynergy-control/` | Native Sailfish user-service control app: start, stop, restart, status, and reboot autostart. |
| `installer/root-helper` | Root-only installer, rollback, migration, chroot provisioning, and guarded Lipstick patch logic. |
| `waynergy/` | Waynergy 0.0.17 source with the local pointer feed and Sailfish-friendly relative-`uinput` changes. |
| `pointer-plugin/` | The system-installed `Waynergy.Pointer` Qt 5 QML extension and global overlay. |
| `calibrator/` | Pointer calibration UI. Offset and scale are saved through the plugin. |
| `lipstick/` | A compact patch for the supported stock compositor; no Jolla source or binary is included. |
| `rpm/` | Sailfish RPM specification. |
| `release/` and `.github/workflows/` | Reproducible Bookworm armhf rootfs and source-archive tooling. |

## Runtime behaviour

The app’s defaults are the tested portrait phone profile:

```text
Deskflow server: 192.168.0.102
Port:            24800
Screen name:     sailfish
Output:          720 × 1280
Chroot:          /home/nemo/chroots/deskflow-bookworm
```

All fields are editable before installation. The generated user service runs
Waynergy as `nemo` through the chroot dynamic loader with:

```sh
waynergy -b uinput -c SERVER -p PORT -W 720 -H 1280 -N sailfish -e -t -n
```

The keyboard and click path uses `uinput`. Waynergy also sends its local,
versioned `WPTR` datagrams to `/run/display/waynergy-pointer.sock`; the
Lipstick QML extension consumes those datagrams and draws the cursor globally.
It rereads calibration on a Deskflow re-entry and when the overlay is
resynchronized after a window change.

## Install on the phone

1. Enable Developer mode and install the built
   `sailfish-deskflow-setup-*.armv7hl.rpm`.
2. Open **Deskflow setup**, check the preflight report, and adjust the server
   settings if necessary.
3. Choose **Install / update**. The password prompt is forwarded directly to
   `devel-su` using a PTY, then cleared from the UI process.
4. After Lipstick restarts, move the Deskflow pointer onto the phone and test
   both keyboard and pointer/click input. Use **Pointer calibration** from the
   launcher to correct any fixed offset or range difference.
5. Within 180 seconds, choose **Pointer and UI work — keep setup**. Without
   that confirmation the root-owned timer restores the previous service,
   Waynergy binary, and compositor file.

After installation, use **Waynergy control** from the launcher to start, stop,
or restart the service. Its **Start automatically after reboot** switch runs
`systemctl --user enable/disable waynergy.service`; the installed service is
already enabled by default and has `Restart=always` to retry a failed network
connection.

The installer uses an existing valid Bookworm armhf chroot as-is. If the
selected chroot does not exist, it downloads a release rootfs only after its
SHA-256 equals the value embedded in `rootfs-manifest.ini`. The checked-in
manifest intentionally contains a placeholder checksum: build and publish the
rootfs release, update the manifest, then build the distributable RPM. This
prevents a fresh install from trusting an unpinned download.

An older manual installation is migrated only when its compositor is either
the known stock revision or the known earlier pointer patch. Its user QML path
and compositor environment override are saved under
`/var/lib/sailfish-deskflow-setup/` before the packaged system QML module takes
over. Unknown compositor edits cause the installer to stop without changing
Lipstick.

## Build and release

Build the rootfs on an armhf-capable Bookworm host, or use the tagged GitHub
Actions workflow (which uses QEMU):

```sh
./release/build-rootfs.sh release-output
./release/update-rootfs-manifest.sh release-output/bookworm-armhf-minbase.tar.gz 0.1.0
```

Commit the resulting manifest before making the RPM source archive. The
workflow publishes `bookworm-armhf-minbase.tar.gz` and `SHA256SUMS` on tags.
The URL convention in `update-rootfs-manifest.sh` is the public GitHub release
asset URL for this repository.

Use the Sailfish SDK (matching OS 3.2 armv7hl) to build the RPM. First create a
clean committed source archive, then place it in the SDK RPM `SOURCES`
directory and build the supplied spec:

```sh
./release/build-source-archive.sh 0.1.0 ~/rpmbuild/SOURCES
rpmbuild -ba rpm/sailfish-deskflow-setup.spec
```

The source archive script uses `git archive`, so only committed changes enter
a release artifact. The target SDK must provide Qt 5 Core, Gui, Qml, Quick,
Silica, qmake, and the matching `moc`; building the plugin against a desktop
Qt is not supported.

## Recovery and removal

Use **Restore previous setup** in the app to restore any service and Waynergy
binary present before this package took over, restore the guarded compositor
backup, and return legacy per-user QML files if they were migrated. It leaves
the Debian chroot in place by design.

If the compositor fails before the UI appears, connect through SSH/developer
mode, restore the saved `/var/lib/sailfish-deskflow-setup/compositor.qml.orig`
to `/usr/share/lipstick-jolla-home-qt5/compositor.qml`, and restart the
user `lipstick.service`. The automatic 180-second rollback is intended to
cover this failure path as long as the phone remains running.

`uinput` can synthesize system input. Keep the device on a trusted network,
use Deskflow TLS, and do not expose `/dev/uinput`, the pointer socket, or
Deskflow credentials to untrusted users.

## License

The Waynergy subtree retains upstream’s MIT license. New integration code is
MIT-licensed under this repository’s `LICENSE`. Sailfish OS and Jolla assets
are not included.
