# Porting a program to Helios

A port has three parts: patches explain changes to upstream code, `files/`
contains new source and support files, and a JSON recipe explains how to build
and install the result. The common tools handle source downloads, patch
application, dependencies, compilation and verification. Start with a small
program, get it running, and add optional features afterwards.

## Read a small working example

[Telnet's recipe](../ports/telnet/port.json) and [terminal
patch](../ports/telnet/01-helios.patch) are a compact example. This extends an
existing Helios port, so it demonstrates adapting an OS interface without having
to understand a complete Unix-to-Helios conversion first.

From the repository root:

```sh
python3 ports.py show telnet
python3 ports.py prepare telnet
```

`show` names the source and patches. `prepare` fetches the pinned revision,
verifies it, applies the patches in order, and leaves readable sources in
`work/sources/telnet`. It does not need a compiler.

Read the patch before the recipe. It addresses three related terminal problems:

- `TerminalRead` converts Helios key sequences into the escape sequences a
  remote terminal expects.
- `TerminalNewMode` requests raw output so the remote program's escape sequences
  reach xterm without being interpreted by another terminal layer.
- The Telnet loop obtains rows and columns through `GetAttributes`, then sends
  a NAWS window-size message when the dimensions change. The small
  `telnet-naws.h` helper encodes the protocol and escapes its special bytes.

Changes to the existing terminal functions belong in a source patch. The new
protocol helper belongs under `files/` at its destination in the source tree.
Compiler paths and install destinations belong in the recipe.

Read `port.json` in this order:

1. `upstream` selects the exact source revision and subdirectory. Its `include`
   list selects the original source files needed by this port.
2. `patches` gives the reading/application order, purpose and checksum of each
   adaptation. The tree checksum detects accidental changes to the result.
3. `dependencies` lists other ports needed first; Telnet has none.
4. `files` adds [telnet-naws.h](../ports/telnet/files/telnet-naws.h) as a normal
   source file. It is not hidden inside a patch.
5. `build.groups` selects the C files and HCC compiler settings. `includes`
   supplies the Helios SDK and the application directory.
6. `build.targets` links the group with the SDK libraries and runtime. The
   output `${stage}/bin/telnet` means `/helios/bin/telnet` after installation.

Build and verify it with the SDK and assembler you built from Helios-NG SP2:

```sh
python3 ports.py build telnet \
    --sdk /path/to/sdk/prod/TRAN --asm /path/to/sdk/host/bin/asm
python3 ports.py verify telnet \
    --sdk /path/to/sdk/prod/TRAN --asm /path/to/sdk/host/bin/asm
```

Compilation logs and objects are in `work/telnet/obj`; linker logs are in
`work/telnet/logs`. `work/telnet/stage` contains the installable result.
Verification checks the recorded inputs and outputs; it does not run the program
on Helios. Test the installed client with typing, cursor keys, a resize and
exit.

For a small port of an ordinary Unix X application, read
[Xmahjongg](../ports/xmahjongg/port.json) next. Its recipe separates C and C++
compilation, obtains headers and libraries from the shared X11 dependency, and
installs the program and data. Its patch adapts timing and configuration.

## Make one focused change

Keep the verified source cache untouched. Work on a copy:

```sh
cp -a work/sources/telnet work/telnet-edit
# Edit work/telnet-edit and test the change.
python3 ports.py diff telnet --edited work/telnet-edit \
    --patch 02-resize-fix.patch \
    --description "Handle the specific resize case being fixed"
```

The new patch is relative to the existing patch series. Any new files in your
working copy are saved as ordinary files under `ports/telnet/files/`. Review it
before building. It should contain only the change you intended. Run `build
telnet` again with the same SDK arguments; the changed patch invalidates the old
build.

To revise that last patch, edit the same clean copy and run `diff` without
`--patch`. Earlier patches remain intact. Keep a patch and its relevant tests
together. Split unrelated work into another patch with a name that explains it.
A small correction to an earlier adaptation can be appended to the series;
substantial reordering is better done explicitly with Git.

For a change confined to an added file, edit it directly and record the source
checksum after review:

```sh
# Edit ports/telnet/files/telnet-naws.h.
python3 ports.py refresh telnet
```

This workflow needs no patch regeneration or source copy. `refresh` verifies
that the patched upstream and supplied files still form a valid source tree.

Do not generate patches from `work/telnet/src`: that is a compilation tree and
may contain generated files. For a complex port, use a separate Git checkout for
development and review the clean source changes before exporting them.

## Start a new port

Choose a fixed upstream release or Git commit. Keep its tests, licence and
unrelated platform code intact. A feature can be left out of the build without
deleting its upstream implementation.

Copy a small recipe and change its name, version and upstream information. For
an archive, record its SHA-256 with `sha256sum`. Extract the selected upstream
directory and compute its source-tree checksum using the same helper as the
builder:

```sh
python3 - /path/to/extracted/upstream <<'PY'
import sys
sys.path.insert(0, 'tools')
from sources import tree_hash
print(tree_hash(sys.argv[1]))
PY
```

Initially use that digest for both `upstream.tree_sha256` and
`patched_tree_sha256`, and set `patches` to `[]`. For Git, extract the pinned
commit with `git archive` first. If using `upstream.include`, compute the tree
hash from the same selected files, preserving executable bits and symlinks. The
[recipe reference](ports.md) describes the source fields precisely.

Create `ports/NAME/port.json`, run `prepare NAME`, and make your first change in
a copy of its result. `diff NAME --patch 01-helios.patch --edited ...` generates
the patch checksums for you. A new port need not change the builder.

Use the simplest suitable build path:

- List compiler groups and link targets for a small C/C++ program. Shared flags
  go in `build.compile`; exceptional flags stay with their group or source.
- Use a `run` action for an existing useful build system. Frotz's
  [Makefile.helios](../ports/frotz/files/Makefile.helios) is an example. Keep commands as argument arrays.
- Generate parsers, embedded resources and data during `build.prepare`.
  X11 runs Bison and Flex; Irssi uses its upstream header generator. Generator
  output can be captured with the action's `stdout` field.
- Declare another port as a dependency instead of copying its headers or
  libraries. `${dep:x11}/include` and `${dep:x11}/lib` are working examples.
- Put installable files under `${stage}`, relative to `/helios`. Keep desktop
  preferences and machine-specific configuration in the distribution assembler.

Explicit source lists make feature selection easy to review. `build.compile` and
`build.link` avoid repeated settings, while `text-copy` can install a whole set
of text templates with one pattern. The format is ordinary JSON, not a shell
script or generated build description.

## Choose the right compatibility layer

Helios's POSIX interface already supplies many ordinary C and Unix calls. Use
helios-unix where its documented contracts fit the program: native task
launching, descriptor actions, polling, terminal handling, name resolution and
TLS. Its installed manual pages include `helios-unix(7)`, `hu_spawn(3)`,
`hu_poll(3)`, `hu_terminal(3)`, `hu_resolve(3)` and `hu_tls(3)`.

Understand the caller's expectations before replacing an API. A Unix `fork`
assumption needs a design change: launch a task with explicit arguments and
communication channels. Do not substitute a pretend shared-address-space fork.
For local communication use native channels. For TLS use the provider API,
rather than embedding a machine-specific host request in each application.

[Irssi's network patch](../ports/irssi/02-network.patch) shows the changes at
its network call sites. The [resolver
adapter](../ports/irssi/files/helios/irssi-resolver.c) and [TLS
adapter](../ports/irssi/files/helios/irssi-tls.inc) are ordinary source files
you can read alongside it. [slrn's runtime
patch](../ports/slrn/01-runtime.patch) shows terminal and task adaptations.
NetHack separates [runtime changes](../ports/nethack/01-runtime.patch) from
[data-layout corrections](../ports/nethack/02-data-layout.patch). Its [ABI
checks](../ports/nethack/files/tests/abi.c) and [build
script](../ports/nethack/files/tools/build.py) are supplied directly. Its host
data generators must match the target's integer widths and byte order; a
successful host compilation alone does not establish that.

## Check that another person can reproduce it

Prepare from an empty `--work` directory to exercise the source URLs and pins.
Build there, run the relevant port tests, then test the program on Helios. Check
input, resize, network disconnects, child-task completion and exit where
applicable. Keep runtime findings distinct from compiler or linker success.

Before submitting, read `ports.py show NAME` and each patch as a newcomer would.
Remove generated outputs, duplicate dependency files, unrelated cleanup and
changes to upstream files that the build does not require. Explain a tricky
Helios assumption at the affected code. Retain application licences and tests;
the shared tooling's MIT licence does not replace them.