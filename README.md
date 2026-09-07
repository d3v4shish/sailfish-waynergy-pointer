# Sailfish Waynergy pointer

An experimental Deskflow/Barrier client setup for Sailfish OS. It combines a
small Waynergy fork using the `uinput` backend with a Lipstick QML extension
that displays a real pointer indicator and provides a calibration UI.

It was developed against Sailfish OS 3.2 / Lipstick Qt 5.6. It modifies the
system compositor and virtual input path, so use it only on a device you can
recover over SSH or developer mode.

## What it does

- Connects a Sailfish phone to a Deskflow or Barrier server through Waynergy.
- Creates virtual keyboard and relative mouse devices through `/dev/uinput`.
- Publishes Deskflow's absolute pointer position over a local Unix datagram
  socket (`/run/display/waynergy-pointer.sock`).
- Adds a global Lipstick overlay, instead of a window-owned overlay, so the
  dot survives application changes.
- Reads the actual virtual-mouse events emitted by Lipstick and normalizes
  their logical window coordinates to the output. This keeps the dot aligned
  with clicks and the Deskflow edge boundary.
- Includes a Sailfish calibration application for any residual fixed offset
  or scale difference.

## Repository layout

| Path | Purpose |
| --- | --- |
| `waynergy/` | MIT-licensed Waynergy 0.0.17 source with Sailfish pointer-feed and relative-uinput changes. |
| `pointer-plugin/` | Qt 5 QML extension (`Waynergy.Pointer`) and global pointer overlay. |
| `calibrator/` | QML calibration application and desktop entry. |
| `lipstick/` | Patch for the stock Jolla Lipstick `compositor.qml`; no Jolla source is included. |
| `scripts/` | Device-side staging, build, activation, rollback, and commit helpers. |
| `examples/` | Sanitized service, wrapper, and configuration templates. |

## Security

`uinput` can generate keyboard and pointer events outside Wayland's normal
privileged-input protocols. Treat access to its device node as privileged.
Use Deskflow TLS, review the server's certificate trust prompt, and do not
publish `~/.config/waynergy`, TLS keys, or device-specific calibration files.

The pointer socket is local-only and the plugin creates it owner-readable and
writable (`0600`). It is not a network service.

## Install outline

1. Configure Deskflow or Barrier with a client whose name matches
   `--name`/`-N` below. Put it on the intended edge in the Deskflow layout.
2. Build the Waynergy fork for the phone architecture. Its normal Meson build
   is unchanged; the Sailfish deployment used a Debian ARM chroot because the
   host system lacks development packages.
3. Install the resulting binary and use the `uinput` backend. A service and a
   wrapper template are in `examples/`.
4. Build `pointer-plugin/` with the device's Qt 5 development qmake and a
   matching Qt 5 `moc` executable, then
   install its library, `qmldir`, and `PointerOverlay.qml` under
   `~/.local/lib/qt5/qml/Waynergy/Pointer/`.
5. Apply `lipstick/0001-global-pointer-overlay.patch` to the exact matching
   stock `compositor.qml`. Back it up first. Set `QML2_IMPORT_PATH` for the
   compositor using `examples/waynergy-pointer.conf`.
6. Install the calibrator files under
   `~/.local/share/waynergy-pointer-calibrator/` and
   `~/.local/share/applications/`, then restart the user `lipstick.service`.
7. Move the Deskflow cursor onto the phone and use **Pointer calibration** if
   the marker needs a fixed offset or range correction.

The scripts reflect the tested `/home/nemo` chroot layout. Read and adapt them
before running: operating-system package layouts and the stock Lipstick QML
file can differ by Sailfish release.

## Example connection

```sh
waynergy -b uinput \
  --host deskflow-server.example --port 24800 --name Sailfish \
  --width 720 --height 1280 \
  --enable-crypto --enable-tofu --no-clip
```

Keep the advertised width and height equal to the physical portrait output.
Deskflow uses those dimensions to decide when the pointer leaves the phone.

The companion `config.ini` needs:

```ini
[pointer]
socket=/run/display/waynergy-pointer.sock
```

## Pointer protocol

Waynergy sends a 24-byte native-endian datagram after mouse motion:

```text
u32 magic     0x57505452  # WPTR
u16 version   1
u16 flags     1 means resync
u32 width
u32 height
i32 x
i32 y
```

It is deliberately local and versioned. The resync flag is emitted when
Deskflow re-enters the Sailfish screen, allowing the overlay to reapply its
saved calibration before the next motion packet.

## Recovery

The activation helper saves a stock compositor backup and arranges a short
rollback window. Do not mark an activation committed until Lipstick has
restarted and the device UI is usable. If the compositor fails, restore the
saved `compositor.qml` over SSH and restart `systemctl --user restart
lipstick.service` as the Sailfish user.

## Licensing

The Waynergy subtree retains its upstream MIT copyright and license in
`waynergy/LICENSE`. New integration files are MIT-licensed under this
repository's `LICENSE`. Sailfish OS and Jolla Lipstick assets are not included;
only a compact patch against a stock compositor file is supplied.
