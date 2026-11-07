# XaoS

XaoS 3.6 runs as a native X11 fractal explorer on T800/T805 Helios systems.
The X client, image filters and zoom cache run on the calling processor.
Spare Transputers calculate missing pixels through native Helios channels.

```sh
xaos -size 640x480 -processors all
xaos -size 480x360 -processors 4
xaos -processors 1
```

The count includes the calling processor. `all` is the default. Workers are
started on running T800-class processors that the session's Taskforce Manager
can obtain; system processors are excluded. A smaller available pool is used
when the requested count cannot be obtained. The window title shows the live
count. Workers and their reservations are released when XaoS exits. Reserving
all spare processors can prevent another login from obtaining a processor;
choose a smaller count when other sessions need to start.

Hold the left mouse button to zoom in, the right button to zoom out, and the
middle button to move the view. The menu bar provides formulas, colours,
filters, help and tutorials. Configuration is saved as `xaos.cfg` in the user's
home directory. Data is installed under `/helios/local/share/xaos`.

The Xlib driver sends a complete image to the X server. At 640x480 with a
32-bit X image this is 1,228,800 bytes per update, which must cross the links
between the client and the host. Additional render processors cannot remove
that display cost. Smaller windows reduce it. XaoS keeps two full-size pixel
buffers on the calling processor; workers do not pool their RAM. If a resize
cannot allocate those buffers, the previous window size is restored.
`-processors 1` uses XaoS's original serial renderer, including its boundary
tracing and solid guessing.
While moving, XaoS uses its original adaptive renderer and solid guessing
in both axes. Once movement and zoom coasting stop, the workers calculate
the missing pixels exactly in one full-resolution pass, retaining the zoom
cache. That pass still processes input and can be interrupted by a new mouse
gesture, resize or other explicit action. It is not cut into timed previews.
Speed depends on the view and formula.

## Build

Use the shared port builder with a Helios-NG SP2 SDK, its assembler and a built
helios-unix stage. X11 is built as a dependency.

```sh
python3 ports.py build xaos --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

The recipe pins the upstream release and supplies the native runtime under
`files/helios`. The patches connect that runtime to startup, the zoom engine
and the Xlib driver. PNG export and the optional custom-formula parser are
not built. Standard formulas, filters, replay files, examples and tutorials
are included.

## Distributed renderer

Each worker is a separate Helios task with its own formula state, coordinates
and palette. `hu_spawn` places it on an obtained processor. Two `hu_channel`
streams carry requests and replies through the native FIFO server; they do
not use `/internet` or TCP.

The coordinator keeps the existing image and reallocation tables. An index of
changed rows and columns describes only the missing pixels. A job contains a
range in that index, so coordinates do not have to travel with every pixel.
Workers receive formula state and coordinates once per frame, and palette
pixels only when the palette version changes. Results contain pixel values,
an epoch and a sequence number. One job per worker bounds queued data. Lost
jobs are recalculated locally; an unresponsive worker is retired after a
30-second job timeout. Remaining workers continue rendering.

## Checks and profiling

Run these on Helios, with spare processors available:

The distributed checks fail if no remote worker starts. Use `-processors 1`
explicitly to check only the calling processor.

```sh
xaos --helios-check -processors 4
xaos --helios-check-failure -processors 4
xaos --helios-benchmark -processors all
xaos -helios-profile -size 640x480 -processors all
```

The checks compare distributed pixels with the upstream calculation kernel
across formulas, Julia rotation, palette changes, pixel sizes and sparse
updates. Repeated time-limited previews must finish and match the reference
pixels without a forced recalculation. The failure check kills one of its own
workers and verifies local recalculation. The benchmark measures a 640x480
exact calculation separately from display. Profiling reports worker setup,
local calculation, polling and
X11 transfer times on standard error. It synchronizes with the X server for
each display measurement, so profiling changes normal Xlib buffering.

The preview policy probe uses the actual UI helper and zoom engine without
an X server. Build it after the port, using the same ports work directory:

```sh
python3 ports/xaos/tests/build.py --work work
```

Copy `work/xaos/tests/preview` to Helios and run:

```sh
preview -processors 2 -helios-profile
```

It checks timed adaptive previews while moving, completion
after release, zoom coasting, continuous rotation and explicit cancellation
of the full-resolution pass. It also runs on a single-processor emulator;
the distributed path falls back to local calculation when no worker is
available.

The X11 resize check runs from Linux against a private test display. Start
XaoS there at 640x480 on a 16 MiB processor, then run:

```sh
python3 ports/xaos/tests/resize.py --display :191 --output resize.json
```

It checks growing, shrinking, redraws, and repeated rejection of a size whose
two pixel buffers exceed the processor's RAM. It leaves the test client open.
