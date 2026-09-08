# How Deskflow input becomes a visible Sailfish pointer

This document explains the complete integration in this repository: the
graphics/input architecture it sits on, why the original keyboard-and-click
setup had no usable cursor, and what every substantial code change does.

It is written for the specific target used by this project: Sailfish OS 3.2 on
an `armv7hl` phone using Jolla Lipstick and Qt 5.6. Names, paths, compositor
internals, and the guarded SHA-256 values are deliberately target-specific.
Do not treat the Lipstick patch as a portable Wayland recipe.

## The short version

Deskflow reads a real keyboard and mouse on the computer that runs the
Deskflow server. When the server decides that the cursor has crossed into the
phone's screen, it sends protocol messages to Waynergy on the phone. Waynergy
turns those messages into a virtual Linux keyboard and mouse through
`/dev/uinput`. Sailfish's input stack treats those virtual devices as hardware
input and delivers the events to Lipstick, which selects the window under the
pointer and delivers clicks/keys to the right application.

That input path can work perfectly while no mouse image is drawn. A phone UI
is normally touch-first and Lipstick does not promise a desktop-style cursor
for an injected mouse. This project therefore adds a second, *visual*
side-channel: Waynergy publishes the coordinates it received to a local Unix
socket; a small QML plugin inside the Lipstick process reads them; and a QML
overlay draws a dot above the complete compositor scene.

```text
Desktop keyboard / mouse
          |
          v
Deskflow server on the computer
          |  encrypted Deskflow-compatible TCP connection
          v
Waynergy client in Debian armhf chroot on Sailfish
          |                                  |
          | Linux virtual input              | local WPTR datagram
          v                                  v
     /dev/uinput                       /run/display/waynergy-pointer.sock
          |                                  |
          v                                  v
Linux input / evdev                  Waynergy.Pointer QML plugin
          |                                  |
          v                                  v
Qt evdev input handler --> Lipstick compositor --> global PointerOverlay
          |                         |                  |
          v                         v                  v
application gets click/key      chooses target       visible dot
```

The two branches have different jobs. `/dev/uinput` makes the phone react to
input. The Unix socket makes the input observable as a pointer. The dot never
generates a click; it only represents the input that Linux and Lipstick are
already handling.

## The vocabulary: display server, compositor, Wayland, and X11

### What a compositor is

An application does not normally write pixels directly to the physical
display. It produces a window-sized image, often called a *surface* or
buffer. A compositor decides where that image appears, which window is on top,
which pixels are obscured, what happens during rotations and animations, and
when the final frame is submitted to the display hardware.

A compositor also has an input role. It knows the global scene geometry and
which surface is under the pointer or touch location. It can therefore route a
tap, click, keyboard focus change, gesture, or touch sequence to the proper
client. In a phone interface it also owns system surfaces such as the
launcher, lock screen, notifications, application switcher, and dialogs.

On this Sailfish phone, **Lipstick** is that shell/compositor. It is Jolla's
Qt/QML-based homescreen and window-management layer. The important practical
consequence is that a QML item added to an ordinary application belongs only
to that application. It disappears with the application, can be covered by
another window, and cannot faithfully represent a device-global pointer. A
QML item added to Lipstick belongs to the full system scene.

### Wayland in plain language

Wayland is a protocol and architecture for clients to communicate with a
display server/compositor. A Wayland application allocates a buffer, draws
into it, and tells the compositor that the buffer is ready. The compositor
places it into the final scene. The client does not get unrestricted access to
other windows, global input, or arbitrary screen pixels merely by connecting
to Wayland.

Wayland deliberately makes privileged actions explicit. A normal client is
not expected to inject a global keyboard event into another client, inspect
the full desktop, or move the real pointer. Compositors can expose optional
protocols for controlled virtual input; for example, Waynergy understands
some wlroots and KDE virtual-input protocols. Lipstick on this target does not
provide the compatible privileged protocol that Waynergy would need, which is
why this project uses Linux `uinput` instead.

Wayland is not synonymous with a particular compositor. Lipstick, KWin,
Mutter, Sway, and many others can be Wayland compositors, each with different
extensions and security policy. Supporting Wayland in general is not enough
to guarantee that virtual input works on every Wayland compositor.

### How X11 differs

X11 is the older display-system architecture. Historically, the X server owns
the screen, input devices, cursor, and windows; applications are X clients
that send drawing requests and receive events from that server. Its protocol
was designed in an era where clients often had broad visibility of the
desktop. Extensions and modern window managers changed the details, but its
security and input model remains comparatively permissive.

Wayland moves much more policy into the compositor and gives each client only
the objects it needs. This is why an X11-oriented "move the mouse globally"
solution cannot simply be pointed at a Wayland phone. The compositor needs a
trusted injection route. Here that trusted route is the Linux input subsystem,
not a generic Wayland client API.

| Question | X11, simplified | Wayland, simplified |
| --- | --- | --- |
| Who owns the display/input boundary? | X server | The compositor/display server |
| Who chooses the visible cursor? | Usually X server/window manager | Compositor policy |
| Can a normal client inject global input? | Historically easier; extension-dependent | Normally no; privileged protocol required |
| Who composites windows? | Often a separate compositing manager | Normally the Wayland compositor itself |
| What does this project use on Sailfish? | Not the primary path | Lipstick, plus kernel virtual input |

## From a mouse movement to an application click

The path below separates the layers because it explains several behaviours
that initially seem strange: keyboard input working while the cursor is
missing, clicks working even when the dot is offset, and a dot disappearing
when it was attached to an app window.

1. A physical mouse produces electrical reports. The desktop operating system
   turns them into input events.
2. Deskflow's server reads those events and tracks a virtual multi-screen
   layout. Crossing an edge transfers ownership of the keyboard/mouse to the
   selected client screen. The client screen name is important: this project
   defaults to `sailfish` and passes it to Waynergy as `-N sailfish`.
3. Deskflow sends a Deskflow/Synergy-compatible stream of mouse positions,
   buttons, scrolls, and key events over TCP to Waynergy. TLS and
   trust-on-first-use options are enabled in the installed service (`-e -t`).
4. Waynergy parses those messages. It maintains the Deskflow pointer position
   in the configured 720 by 1280 portrait coordinate space and invokes the
   selected input backend.
5. The selected `uinput` backend writes Linux `input_event` records to
   virtual keyboard and mouse devices. The kernel exposes those devices back
   to userspace like ordinary input hardware.
6. Lipstick's Qt platform/input machinery reads the corresponding `evdev`
   stream. For a pointer, it turns the event into Qt mouse events and routes
   it through the compositor's scene and focus rules. For keys, it routes the
   key event to the focused application or a system component.
7. The application finally receives a protocol-level pointer/button or key
   event. It does not need to know that Deskflow was involved; from the app's
   perspective this is an input device handled by the system.

The application does **not** receive a raw network packet from Deskflow. It
also does not read `/dev/uinput`. The kernel, Qt input handling, and Lipstick
form the trusted mediation layers between the remote desktop server and the
application.

## Deskflow and Waynergy

### Deskflow's role

Deskflow is a software KVM: one machine is the server attached to the real
keyboard and mouse, and each other machine runs a client. The server has a
logical rectangle for each client screen. When the server-side pointer reaches
the configured edge, Deskflow changes the active target and sends input to the
client instead of applying it locally.

It is important to distinguish **network reachability** from the Deskflow
screen layout. An IP address only determines whether the client can open the
TCP connection. The layout determines whether the pointer can enter the
phone's screen and which edge it leaves from. A correct ping does not prove
that the client name, port, TLS trust, or screen-edge arrangement is correct.

### Waynergy's role

Waynergy is a Wayland-oriented client for the Synergy-family protocol spoken
by Deskflow/Barrier-compatible servers. It translates the remote input stream
to a compositor-appropriate backend. Upstream supports backends based on
virtual-input protocols where they exist; this repository uses `-b uinput` as
the practical backend for this Sailfish target.

The packaged service runs the effective command shape below. The installer
substitutes the editable server, screen, dimensions, and port:

```sh
waynergy -b uinput -c 192.168.0.102 -p 24800 \
  -W 720 -H 1280 -N sailfish -e -t -n
```

`-c` and `-p` select the server endpoint; `-N` selects the Deskflow screen
name; `-W` and `-H` define the coordinate rectangle advertised to Deskflow;
`-e` enables crypto; `-t` enables TOFU certificate trust; and `-n` disables
clipboard handling. Keeping the 720 by 1280 rectangle equal to the physical
portrait output is important: Deskflow uses it for edge and pointer mapping.

### Why Waynergy is in a Debian chroot

The phone's Sailfish OS userspace has a different development package set,
ABI history, and packaging model from Debian Bookworm. The custom Waynergy
binary is therefore built in an armhf Debian Bookworm chroot, but it still
uses the same phone kernel. A chroot changes the filesystem root seen by the
process; it does not virtualize the kernel, CPU, network interfaces, or
`/dev`. The chrooted process can therefore open the host's `/dev/uinput` once
the device-node permissions permit it.

The installed wrapper executes the chroot's ARM dynamic loader and library
paths explicitly. This allows a Debian-linked Waynergy binary to run while
the surrounding Sailfish user session remains native Sailfish. The service
itself remains a `nemo` user service. It is not a full virtual machine and it
does not run a second graphical session.

## `/dev/uinput`: virtual Linux hardware

Linux has an input subsystem used by keyboards, mice, touch devices, gamepads,
and more. Physical device drivers feed that subsystem. `uinput` is a kernel
interface that lets a trusted userspace process create a *virtual* input
device and submit the same kinds of events that a driver would submit.

Waynergy opens `/dev/uinput` twice: one descriptor for a keyboard and one for
a mouse. Its `wl_input_uinput.c` code declares the event capabilities, creates
the devices, and writes records such as:

```text
EV_REL  REL_X / REL_Y       relative pointer movement
EV_KEY  BTN_LEFT            button press or release
EV_KEY  KEY_A               keyboard key press or release
EV_SYN  SYN_REPORT          end of one coherent input report
```

The key consequence is security. Anyone able to write to `/dev/uinput` can
usually impersonate a keyboard or mouse to the local session. This project
does not make it world-writable. The installer creates a narrowly named udev
rule only when an existing Waynergy rule is absent, granting the device to the
`nemo` account. The root helper checks that `nemo` can actually write it before
starting the service. Treat Deskflow server access, the `nemo` account, and
the udev permission as trusted boundaries.

### Why the mouse had to become relative

Deskflow reports absolute positions in the phone rectangle. Sailfish's Qt
evdev mouse path on this device behaves as a relative mouse path. Sending
absolute events as if they were an absolute touch/pointer device caused the
cursor state and click coordinates to disagree.

The repository changes `waynergy/src/wl_input_uinput.c` so that the virtual
mouse advertises relative capabilities and converts each consecutive absolute
Deskflow position into a delta:

```text
first remote position:       emit relative movement from 0,0 to x,y
later remote position x,y:   emit REL_X = x - previous_x
                              emit REL_Y = y - previous_y
```

The state is reset when the mouse virtual device is recreated. This is why
the project can preserve the behaviour Sailfish's `QEvdevMouseHandler`
expects while still accepting absolute positions from Deskflow.

## Why a working mouse had no visible cursor

On a desktop, the compositor often draws a hardware or software cursor by
default. On Sailfish, normal interaction is touch-led. A click can be routed
correctly through the virtual mouse while Lipstick has no reason to render a
desktop arrow. The first observable result was exactly that: keyboard input
worked, buttons activated UI controls, but no visible indicator followed the
remote pointer.

Drawing a cursor inside an ordinary application did not solve the problem.
That cursor is owned by the application's QML scene. Opening a new app, raising
a dialog, closing the source app, or direct-rendering a fullscreen surface can
hide or destroy it. A cursor is global state, so its drawing object must be
global state too.

The solution has two parts:

- Keep input injection in the kernel path, which determines what is clicked.
- Add a global Lipstick QML overlay, which determines what the human can see.

This deliberate separation is also why an offset dot cannot alter where a
click lands. Calibration fixes the visual projection; it cannot rewrite an
already delivered Linux mouse event.

## The custom pointer-feed protocol

### Why a separate socket exists

The plugin cannot reliably infer Deskflow's remote absolute position from a
single app's local scene. The remote position is known best at the point where
Waynergy parses Deskflow input. The visual overlay runs in Lipstick. The
smallest stable connection between those two processes is a local Unix domain
datagram socket.

Waynergy reads `[pointer] socket=` from its configuration. The installer writes
that configuration as:

```ini
[pointer]
socket=/run/display/waynergy-pointer.sock
```

Each motion publishes one fixed-size native-endian packet:

```text
u32 magic       0x57505452    # "WPTR"
u16 version     1
u16 flags       bit 0 = resync
u32 width
u32 height
i32 x
i32 y
```

The packet is 24 bytes. It is intentionally local-only: both producer and
consumer run on the same phone and same ABI, so the protocol need not solve
cross-architecture byte order. It does include a magic number, version, size
limits, and coordinate validation so an unrelated datagram cannot become a
plausible pointer position.

`wl_input.c` maintains the clamped pointer position and calls
`wlPointerFeed()` after both relative and absolute motion. `main.c` also calls
`wlPointerFeedResync()` when Deskflow announces that the Sailfish client is
active again. That re-emits the last position with the resync flag even if the
first packet after a screen transition is not a mouse movement.

### Socket ownership and lifecycle

`pointer-plugin/pointerfeed.cpp` binds the receiver socket in Lipstick. Before
unlinking a stale socket it verifies that the stale path belongs to the same
effective user. It creates a datagram socket with close-on-exec, sets its mode
to `0600`, and removes it when the plugin is destroyed. The code rejects
wrong-size packets, incorrect magic/version, unsupported flag bits, dimensions
outside 1 through 8192, and coordinates outside the supplied rectangle.

This is not a network listener. A Unix domain socket at `/run/display` is
local to the phone. Its permissions still matter: it is an input-display
indicator and should not be writable by unrelated users.

## The QML plugin and global overlay

### QML module structure

The module is installed under Qt's system import path:

```text
/usr/lib/qt5/qml/Waynergy/Pointer/
    libwaynergypointer.so
    qmldir
    PointerOverlay.qml
```

`qmldir` declares module `Waynergy.Pointer`, the native plugin, and the QML
overlay type. `plugin.cpp` registers two C++ types:

- `PointerFeed`: socket receiver, visibility timeout, and actual Qt event
  observations.
- `PointerCalibration`: persistent visual offset/scale settings.

Installing this as a system QML module instead of only in
`~/.local/lib/qt5/qml` matters. Lipstick is a system component. Its import path
must be available independently of an application window and without relying
on a user environment override that may be lost on restart.

### What `PointerOverlay.qml` draws

`PointerOverlay.qml` is a transparent `Item` that creates a 28-pixel marker:
a dark outer circle and smaller white inner circle. It has a very large z value
so it is layered above normal Lipstick content. It is intentionally a dot
rather than pretending to be a platform-native cursor theme.

The overlay has two visibility notions:

- `feed.visible` is true while fresh Waynergy pointer data is arriving.
- `allocated` keeps the full-size overlay scene allocated while needed and
  releases it shortly after the feed goes quiet.

The feed hides after two seconds without data. The overlay fades, then gives
up its size after a short 150 ms delay. This prevents a permanently visible
dot when Deskflow leaves the phone while avoiding unnecessary permanent
composition when it is inactive.

### Matching the coordinates that actually click

The remote packet gives the desired Deskflow coordinate, but the real virtual
mouse goes through Qt and can be clamped or interpreted in an intermediate
window coordinate system during a surface change. If the overlay followed only
the packet, it could be visually correct in an empty homescreen yet diverge
from the actual click position after a window transition.

`PointerFeed` installs an event filter on the Lipstick `QCoreApplication`.
For mouse move, press, and release events it records `QMouseEvent::windowPos()`
and, when available, the watched `QWindow` size. The QML overlay prefers those
observed coordinates and normalizes them to its full output; it falls back to
the Deskflow packet when no observed Qt position exists. The visual equation
is conceptually:

```text
dot_x = clamp((observed_x * output_width / observed_window_width)
              * x_scale + x_offset - dot_radius)
dot_y = clamp((observed_y * output_height / observed_window_height)
              * y_scale + y_offset - dot_radius)
```

This is a visual mapping only. It makes the marker track the coordinate space
that Lipstick actually saw, which is the closest useful representation of
where a click will land.

## Lipstick patch: why it is small but important

`lipstick/0001-global-pointer-overlay.patch` is not a rebuilt Lipstick. It is
a compact patch against the exact known stock `compositor.qml`. It makes four
changes.

1. It imports `Waynergy.Pointer 1.0`.
2. It inserts a global `PointerOverlay` inside a full-size compositor layer at
   a high z-order. Its width/height use the short and long physical axes
   (`min(root.width, root.height)` and `max(...)`) so portrait coordinates
   remain stable when a window reports an orientation-dependent root geometry.
3. It adds `pointerOverlay.allocated` to the conditions that disable direct
   rendering. Direct rendering can bypass normal scene composition for a
   fullscreen surface; if that were allowed while the pointer overlay is
   active, the overlay could be hidden behind a directly rendered app.
4. After a window is destroyed, it starts a 250 ms timer when the pointer is
   active. The timer calls `resyncAfterWindowClose()`, which reloads
   calibration, keeps the overlay allocated, and refreshes the feed.

The last two changes address the observed failure modes: a dot that died with
an app window, and a dot that shifted when a portrait window closed or the
scene geometry was rebuilt. A delay is used because the compositor needs a
short turn through its event/layout cycle before the replacement geometry is
meaningful.

## Calibration

`PointerCalibration` persists four values in:

```text
~/.config/waynergy-pointer/calibration.ini
```

```ini
[calibration]
xScale=1.0
yScale=1.0
xOffset=0.0
yOffset=0.0
```

The C++ class bounds scales to 0.25–2.0 and offsets to the 720 by 1280 target
range. It uses `QSettings` for storage and a `QFileSystemWatcher` to reload
external changes. `Calibrator.qml` exposes four Sailfish Silica sliders and a
reset action. It is a separate launcher application, but it imports the same
system QML module and writes the same file that Lipstick reads.

The practical calibration sequence is:

1. Move the remote pointer near the physical top-left and adjust offsets until
   the dot and click agree there.
2. Move near the opposite edge. Adjust scale to correct how far the dot
   travels, then revisit offset if necessary.
3. Test after opening and closing an application. A fixed mismatch is a
   calibration issue; a mismatch that changes with windows indicates a
   compositor/input-coordinate issue and is why the event-filter and resync
   work exists.

## The guided installer and why it is split in two

The package is not merely a shell script. It has a normal Sailfish application
front end and a narrow root helper because most configuration can be safe and
visible as `nemo`, while a few operations necessarily require developer-mode
root privileges.

### Unprivileged application

`installer-app/qml/main.qml` supplies editable host, port, screen name,
dimensions, and chroot fields. Its default host is `192.168.0.102`; it is an
example/configuration default, not a discovery mechanism or a claim that every
network uses that address.

`InstallerController` in C++:

- validates values before a root request is attempted;
- writes the request under
  `~/.local/share/sailfish-deskflow-setup/request.ini` with owner-only
  permissions;
- launches `/usr/bin/devel-su` and the root helper through `forkpty()`;
- watches the pseudo-terminal for the interactive password prompt and progress
  markers such as `PHASE=` and `RESULT=`;
- sends the entered password only to that terminal, overwrites the temporary
  byte buffer, and does not write it to a settings file;
- reads the same fixed application-data path as the root helper so a restarted
  UI can see an activation awaiting confirmation.

A PTY is used because `devel-su` is intentionally interactive. Passing a
password on a command line, in an environment variable, or to a file would
create far worse disclosure paths. Clearing a program buffer is a best effort,
not a cryptographic guarantee that every GUI/input-method copy of a string has
vanished; the important guarantee is that the installer does not deliberately
persist it.

### Root helper

`installer/root-helper` is a POSIX shell program invoked only through
`devel-su`. It accepts `--install`, `--restore`, or
`--rollback-if-unconfirmed` plus a config path. It validates the chroot path,
host, screen name, port, and dimensions again; the root process does not trust
the UI validation alone.

Before changing runtime configuration, it performs an exact compatibility
gate:

- The phone must report `armv7l`.
- The compositor file must hash to the known Sailfish 3.2 stock SHA-256, or to
  the known prior pointer-patched SHA-256 with a verified stock backup.
- Unknown changes cause an error rather than an attempt to merge arbitrary
  QML edits.

For a new chroot, the helper downloads only the public release rootfs URL and
verifies its embedded SHA-256 before extraction. The checked-in manifest uses
the literal placeholder `RELEASE_ASSET_SHA256` until a release asset is built;
this deliberately blocks a new download rather than silently trusting an
unpinned archive. An already-valid Bookworm armhf chroot is reused.

Inside the chroot the helper installs only the Waynergy build dependencies,
copies the vendored source, builds it with Meson/Ninja, and installs the result
as `/usr/local/bin/waynergy`. The Meson VCS-stamp step is replaced with a
deterministic version header because of the minimal build environment. Before
overwriting an existing Waynergy binary it saves it under
`/var/lib/sailfish-deskflow-setup/`.

The helper then writes:

- a small runtime file naming the chroot;
- a wrapper that invokes that chroot's ARM loader and library paths;
- a `nemo` systemd user service for Waynergy;
- a restricted udev rule for `uinput` when no existing Waynergy rule exists;
- the local pointer socket configuration.

It also migrates the older manual per-user QML module/environment override by
backing it up under `/var/lib/sailfish-deskflow-setup/` and moving it out of
the compositor's import path. This makes the packaged system QML module the
one Lipstick imports while preserving an exact Restore path.

### Guarded activation and rollback

Applying a broken compositor QML file can remove the visible UI. Therefore the
helper first saves the known stock compositor, applies the patch only after a
dry run, and starts a root-owned systemd rollback unit for 180 seconds. It
then restarts the `nemo` user's Lipstick service and reports
`PENDING_CONFIRMATION`.

Only after the user sees working UI and pointer behaviour does the setup app
create its unprivileged confirmation marker. When the timer fires, the helper
checks that marker. If it is missing, it restores the prior service, binary,
legacy QML state, and compositor file. If it is present, it clears the pending
state. The timer receives a root-owned copy of the validated request, so it
does not later act on a mutable user request file.

`Restore previous setup` reverses the owned runtime changes but intentionally
does not delete the Bookworm chroot. The chroot can be large and may be useful
for rebuilding; deleting it is a separate user choice.

### Everyday service control after setup

`waynergy-control/` is a separate native Sailfish application for normal
day-to-day operation after the privileged setup has succeeded. It does not
modify the compositor or require `devel-su`. It talks to the current `nemo`
user's systemd manager and controls the service that setup installed:

```text
systemctl --user start waynergy.service
systemctl --user stop waynergy.service
systemctl --user restart waynergy.service
systemctl --user enable/disable waynergy.service
```

The application asynchronously checks `is-active` and `is-enabled`, displays
the resulting running/autostart state, and exposes Start, Stop, Restart, and
**Start automatically after reboot** controls. `enable` makes the service part
of the `nemo` user's default systemd target; Sailfish brings up that graphical
user session after a normal phone boot. The service itself uses
`Restart=always`, so a temporary Deskflow network failure is retried without
requiring the control app to remain open.

The control app uses the existing graphical session's D-Bus environment and
falls back to the known Sailfish `nemo` runtime directory only when those
variables are absent. It is intentionally a user-service controller, not a
new root daemon: the only privileges it needs are the ones already granted to
the installed `nemo` service for its virtual-input device.

Sailfish launches Silica applications through `invoker` and
`mapplauncherd`, rather than simply `exec`ing their binaries. Consequently the
control project's qmake settings build a position-independent executable and
export `main` in its dynamic symbol table. Both properties are required for
the booster to load the application entry point. The controller also uses
`/bin/systemctl`, the location used by Sailfish OS 3.x, and surfaces a
service-manager start failure in the UI instead of leaving the status check
busy indefinitely.

## Packaging and release changes

The RPM specification builds two native pieces against the Sailfish target Qt:
the installer executable and `libwaynergypointer.so`. It installs the plugin
in Qt's system module path, the calibrator QML/desktop entry, the Waynergy
source, the Lipstick patch, and the root helper. The original pointer-plugin
qmake project had hard-coded paths to one developer's local sysroot; those
paths were removed so an SDK build uses its selected target Qt instead.

The repository also adds:

- `release/build-rootfs.sh`, which makes a minimal Debian Bookworm armhf
  archive.
- `release/update-rootfs-manifest.sh`, which computes the archive hash and
  writes the public GitHub Release URL/manifest values used by future RPMs.
- `release/build-source-archive.sh`, which creates a source archive using
  `git archive`; uncommitted local state cannot accidentally become a release.
- `.github/workflows/rootfs-release.yml`, which builds the rootfs under QEMU
  on GitHub Actions and attaches it and `SHA256SUMS` to tagged releases.

The public source repository contains the full source needed to reproduce
these artifacts. It does not include Jolla's proprietary Lipstick source or a
copy of the stock compositor; it contains only the compact patch and the
target-specific hash guard.

## What changed, file by file

| Area | Files | Change and reason |
| --- | --- | --- |
| Deskflow input | `waynergy/src/main.c`, `wl_input.c`, `wl_input_uinput.c`, `wayland.h` | Open `uinput` before privilege drop, create relative virtual input, track Deskflow pointer state, publish local pointer packets, and send resync on screen entry. |
| Pointer receiver | `pointer-plugin/pointerfeed.*`, `plugin.cpp`, `qmldir` | Supply a versioned Unix-socket receiver, Qt-event observation, QML type registration, and a system-loadable module. |
| Cursor rendering | `pointer-plugin/PointerOverlay.qml` | Draw a global, timeout-managed calibrated dot, preferring coordinates seen by Qt. |
| Calibration | `pointer-plugin/pointercalibration.*`, `calibrator/Calibrator.qml` | Persist and edit offset/scale independently of the input injection path. |
| Compositor integration | `lipstick/0001-global-pointer-overlay.patch` | Import the module into Lipstick, keep the overlay above the whole scene, handle direct rendering, and resync after a window closes. |
| Guided setup | `installer-app/*`, `installer/root-helper` | Add Sailfish UI, developer-mode PTY handoff, repeat validation, safe chroot build, service configuration, strict patch guard, migration, restore, and rollback. |
| Everyday service control | `waynergy-control/*` | Add unprivileged Start, Stop, Restart, active-state, and reboot-autostart control for the installed `nemo` systemd service. |
| Distribution | `rpm/*`, `release/*`, `.github/workflows/*` | Make the target package and verified rootfs reproducible and publishable. |
| Documentation | `README.md`, this document | Explain support boundaries, release process, recovery, and the complete architecture. |

## Boundaries and limitations

- This targets one stock Sailfish 3.2 Lipstick revision. A package update can
  change `compositor.qml`; the SHA-256 guard is supposed to refuse rather than
  risk applying a stale patch.
- `uinput` is powerful. Do not treat a Deskflow server on an untrusted network
  as harmless, and preserve TLS certificate verification.
- The visual dot is not a hardware cursor and cannot make a click land
  elsewhere. The event-filter/resync/calibration work reduces visual mismatch,
  but it cannot replace compositor-native cursor support.
- The rootfs manifest remains intentionally unusable for *new* chroots until
  the matching public rootfs release asset and hash are published. Existing
  valid Bookworm chroots can be used immediately by the installer source.
- The source is public and reproducible; a finished phone RPM still must be
  built in a matching Sailfish armv7hl SDK and tested on the supported phone.

## A useful mental model

If input works but the dot is absent, investigate the **visual branch**:
Lipstick import path, plugin loading, local socket, and overlay lifecycle.
If the dot moves but clicks do not follow, investigate the **input branch**:
Deskflow dimensions/layout, relative `uinput`, evdev interpretation, and
Lipstick's scene geometry. If neither branch works, start at network/TLS,
Waynergy service logs, and `/dev/uinput` permissions.

Keeping those three layers distinct—network/protocol, input injection, and
compositor rendering—is the central design decision behind the repository.
