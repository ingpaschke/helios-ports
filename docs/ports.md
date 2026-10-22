# Port recipes

Each `ports/<name>/port.json` has `format: 1`, `name`, `version`, `upstream`,
`patches`, optional `files`, `patched_tree_sha256`, `dependencies` and `build`.
Paths within recipes use `/`, independent of the directory in which the builder
is invoked.

## Sources

An archive upstream specifies `kind: "archive"`, `url`, `filename`, `sha256`,
`subdirectory` and `tree_sha256`. A Git upstream specifies `kind: "git"`, `url`,
a full commit ID in `revision`, `subdirectory` and `tree_sha256`. `subdirectory`
selects the root within the archive/repository; use an empty string for the
whole tree. Optional `include` patterns select paths relative to that root
(default `**`). They use Python's `fnmatchcase` semantics.

`patches` is an ordered list of objects with `file`, a one-sentence
`description`, and `sha256`. Filenames are relative to the port directory.
Numbered filenames show the reading order; the list defines application order.
Use patches for changes to upstream files. Git binary patches also preserve file
modes and symlinks. New files belong under `files/` as ordinary files, so their
code can be read directly. Set `"files": "files"` in the recipe to include that
tree. Paths under it are relative to the upstream source root; for example
`files/helios/platform.c` becomes `helios/platform.c` in the prepared source.
These files are copied after patching and must not overwrite upstream or patched
files. An unmodified upstream can use an empty list. The final
`patched_tree_sha256` checks the reconstructed result, including supplied files.
Tree digests cover file contents, executable bits and symlink targets; they
exclude `.git` and `__pycache__`. The source manager's `tree_hash()` computes
this format.

To add a port, pin an upstream archive or revision and use `tools/sources.py` to
extract and inventory it. Set the initial patched tree hash equal to the
upstream hash and use an empty patch list. Add its recipe, prepare it, edit a
clean copy and run `ports.py diff` to generate the Helios patch and checksums.
Do not put generated build products in that copy.

`diff` separates new files into `files/` and keeps upstream changes in the
patch. You can also edit the supplied files directly and run `ports.py refresh
NAME`; it updates patch checksums and the reconstructed tree hash after
validating patch application. Upstream checksums remain fixed. `refresh` is
explicit: normal builds reject unrecorded source changes.

Use `diff --patch 02-purpose.patch --description "Purpose of this change"` to
append a patch against the current prepared tree. Without `--patch`, `diff`
replaces only the last patch, retaining its description and all earlier patches.
An empty final change removes that patch. Updating an earlier patch is
deliberately rejected: rework its series with Git, then update the recipe
checksums, or append a small corrective patch. `ports.py show NAME` prints the
series and its purposes without build details.

## Dependencies and inputs

`dependencies` lists other port names. They are prepared and built before the
consumer. `requires_compat: true` requires a helios-unix stage;
`requires_host_sdk: true` requires the ILP32 host SDK. `cxx: true` includes the
toolchain's C++ headers and runtime in the build fingerprint. `host_tools` lists
host executables whose contents are fingerprinted. `build_inputs` lists
additional input files, with the substitutions below available.

The common fingerprint includes the recipe, patches, builder, SDK headers and
libraries, assembler, llvm-t800 compilers and runtime. Dependency build records
and outputs are checked before reuse. Declare external inputs that are not
covered by these fields with `build_inputs`.

## Substitutions

| Variable | Meaning |
| --- | --- |
| `${original}` | Verified, patched source cache; treat as read-only |
| `${src}` | Private source copy used for this build |
| `${work}`, `${stage}` | Port's build directory and installation tree |
| `${sdk}` | Helios target SDK root, containing `include` and `lib` |
| `${asm}`, `${asm_dir}` | Assembler executable and its directory |
| `${toolchain}`, `${tools}` | llvm-t800 root and its `sysroots/helios` |
| `${clang}`, `${hcc}`, `${runtime}` | Compilers and `llvmrt-generic.lib` |
| `${compat}`, `${host_sdk}` | Selected compatibility stage and host SDK |
| `${python}`, `${jobs}` | Host Python and compilation parallelism |
| `${dep:name}` | Direct dependency's build directory |
| `${src:name}` | Direct dependency's prepared build source, including generated inputs |
| `${source_dir}` | Directory of the source currently being compiled; compile flags only |

Commands are argument arrays executed directly, without a shell.

## Build steps

`build.prepare` runs before compilation, then `build.groups` compiles sources,
`build.targets` links them, and `build.install` fills the stage. A source-only
port can leave these empty. Existing build systems can be invoked through a
`run` action; any necessary modifications to them belong in the source patch.

Actions in `prepare` and `install` support:

- `run`: `command` array and optional `cwd` (default `${src}`). Optional `stdout`
  writes generated output to a file; diagnostics remain in the build log.
- `copy`: `source` file or directory and `destination`.
- `glob-copy` / `glob-link`: `source` directory, `pattern`, `destination` directory.
- `symlink`: `source` link text and `destination` link path.
- `text-copy`: `source`, `destination`, and an optional `replace` mapping.
  With `pattern`, `source` is a directory and `destination` an output directory.
  Optional `strip_suffix` removes a literal suffix from each output filename.
  For example, Irssi installs `*.in` help templates without `.in` and substitutes
  its version. An empty match fails the build.

Each compilation group has a unique `name`, `sources`, optional `flags`, and
`compiler` (`clang` by default, or `hcc`). Source entries are paths or objects
with `file` and extra `flags`. HCC groups also supply `includes`. Clang groups
carry include options in `flags`. Sources within a group compile in parallel;
groups run in order.

`build.compile` supplies settings shared by groups. Its `flags` and `includes`
are prepended to each group's own lists, followed by per-source flags. Other
settings (such as `compiler`) are defaults that a group can replace. This keeps
common platform and include flags in one place.

Targets have `name`, `type` (`archive` or `executable`), `output` and `groups`.
Executable targets can specify `stack`, `heap`, `start` (default
`${sdk}/lib/c0.o`), extra `objects` and `libraries` in link order. The assembler
packs archives and links executables. The latter receive executable mode.
`build.link` supplies defaults for executable targets; any field given by an
individual target replaces that default (including the whole library list).

The builder records all linked targets and installed files. `build.outputs`
lists additional files or directories to record, such as generated dependency
headers or libraries produced by an external build system. Missing declared
outputs fail the build. Every installed file, its contents, mode and symlink
target is checked before reuse; adding unexpected stage files also invalidates
the build. Successful records are written only after all steps finish.

Use unique `--work` directories for concurrent independent builds. Logs are
under each port's `logs` and `obj` directories. A failed command reports the log
path and its final lines.