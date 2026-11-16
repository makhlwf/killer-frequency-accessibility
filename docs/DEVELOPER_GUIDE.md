# Developer guide

## Build a matched package

Use Windows 64-bit, PowerShell, Python 3, and a MinGW-w64 toolchain providing `gcc`, `g++`, `nm`, and `objdump` on PATH. Clone the repository separately from your game when possible. Close the game before rebuilding installed files; Windows locks loaded DLLs.

From the repository root:

```powershell
./scripts/setup-dependencies.ps1
./build.ps1 -OutputDirectory build/release/bin
./scripts/test.ps1
./scripts/package.ps1 -InputDirectory build/release/bin -OutputPath dist/KillerFrequencyAccessibility.zip
```

Dependency setup also runs during the build. The default build output is `bin`; the explicit directory above separates a fresh release build from local runtime files. Extract the player ZIP and follow the [player installation guide](PLAYER_GUIDE.md). Build and test success establish compilation and the covered algorithmic behavior, not complete gameplay accessibility.

Prism supplies NVDA support in the player package. An optional local controller fallback can be supplied with `./build.ps1 -NvdaControllerPath 'C:/path/to/nvdaControllerClient64.dll'`; this explicitly supplied binary is not packaged. Do not depend on another developer's absolute filesystem paths.

## Architecture and boundaries

- `src/launcher.cpp` starts/attaches to the game and loads the base DLL and companion. The original executable/assets remain unchanged.
- `src/access.cpp` resolves the public IL2CPP runtime API, hooks choice/subtitle/document/menu paths, queues speech, and polls commands on the game update thread.
- `src/navigation.h` integrates bounded floor/clearance route planning, normal interactions, and assisted opening-door gestures. `src/route_planner.h` provides the route algorithm.
- `src/music_fix.cpp` adds contextual record/turntable assistance, simplified destinations, phone/soundboard selection, and music slider controls.
- `tools/music_symbols.py` generates the private companion binding from the exact built base DLL. Map-description preparation generates a textual lookup from reviewed source descriptions.

Resolve gameplay methods through runtime metadata; do not transplant addresses from a different game build. Unity and managed gameplay operations must execute on the game's main thread. Startup threads must attach to the IL2CPP domain before allocating API calls and detach before exit. Keep managed objects across updates through GC handles, release them on completion/cancellation, and revalidate scene state before acting.

The companion is tightly coupled to the base DLL's private symbols and C++ layout. Its generated manifest carries symbol offsets, PE timestamp/image size, and expected function bytes. `Destination` has an explicit 88-byte ABI assertion. These guards detect some mismatches; they do not make arbitrary compiler/build combinations compatible. Always build and distribute the pair together. Never copy an old generated symbol header into a fresh build.

Choice strings are localized separately from callback IDs. Preserve the original answer ID, callback lifetime, complete expected control set, and question visibility checks. A visible label is not proof of a successful callback.

Phone and soundboard interaction uses native `Interactables.Button.Activate`, not a claim that selecting a label connects a call. Phone selectors identify the `FirstFloor_MainStudio` hierarchy and `PhonesLines_Button_One/Two/Three` paths; indicators use native line IDs. Soundboard selectors identify the functional Button_1–8 and Group1–3 objects. Preserve foreground, reach, player-control, menu/question, cooldown, exception, and object-identity guards.

Placement requires native `CanDock`/`Dock` checks and confirmation that the exact deck holds the selected vinyl and the player no longer holds it. Playback and stop require native `IsPlaying` confirmation. The volume selector targets `Record_Slider` through `Slideable.MoveSliderToNormalizedValue` and reads actual state back. Never announce requests as confirmed outcomes.

Door and sleeve interactions send bounded native mouse input. Keep the independent input-release watchdog effective during stalled updates, cancellation, timeout, focus loss, and manual movement. Do not move Unity work onto that watchdog thread.

## Reproduce one failure at a time

1. Back up current saves and record the exact package, game fingerprints, scene, and player state.
2. Produce one failing example, retaining a small timestamped log excerpt. Distinguish wrong selection, unreachable route, failed interaction, and missing confirmation.
3. Change the smallest relevant path, build the matched pair, and run the relevant existing tests.
4. Restart with the complete fresh package. Repeat the same example in foreground gameplay, then check cancellation and the adjacent normal interaction.
5. Record the observed native state and user-visible speech. Update coverage only for what the observation establishes.

Earlier local experiments injected numbered companion DLLs into an already running process. That workflow can preserve old hooks/state and is not the supported player update process. For an intentionally isolated developer experiment, the launcher exposes `--load <pid> <absolute-dll-path>`; do not load a fresh base into an existing initialized process or mix companion generations. Prefer a clean restart for reviewable results.

## Diagnostic commands and probes

The base polls `bin/command.txt` relative to its loaded location about every 250 ms and removes a consumed command. Write exactly one supported command with no BOM or newline. Do not overwrite an already pending command. For example, from the repository root using the default installed `bin`:

```powershell
$commandPath = Join-Path $PWD 'bin/command.txt'
if (Test-Path -LiteralPath $commandPath) { throw 'A command is already pending.' }
[IO.File]::WriteAllText($commandPath, 'snapshot', [Text.UTF8Encoding]::new($false))
```

Base command names are `help`, `repeat`, `previous`, `next`, `activate`, `screen`, `subtitles`, `stop`, `mute`, `timers`, `snapshot`, `navdump`, `navgo`, `navnext`, `navprevious`, `navstatus`, `map`, and `maprepeat`. Companion commands include `musicstatus`, `musicdump`, `phonedump`, `clues`, `music`, `volumeup`, `volumedown`, `soundboard`, `phones`, and `musicstop`. Command spelling is exact. State guards remain active: file commands do not override foreground, menu, choice, character-control, reach, or scene requirements. Polling itself is not evidence that an action succeeded.

`python tools/navigation_repro.py` sends `navgo` and checks a bounded new log excerpt for music assistance. Its result is narrower than a complete pickup/playback regression. Earlier standalone Frida probes were removed after unsuccessful attachment tests. They are not a supported developer tool. For new runtime inspection, follow [the reverse-engineering workflow](REVERSE_ENGINEERING.md) and use the native diagnostic command hooks or an available REA runtime provider.

## Reverse engineering and evidence

Use only your own local game files. REA MCP can inspect metadata/native code through the tools actually exposed in the session; tool names and capabilities must be discovered rather than assumed. AssetStudio is optional for targeted local texture inspection. Neither is required for an ordinary source build. Do not commit proprietary assemblies, global metadata, extracted images, screenshots, decompiled dumps, raw scene inventories, or saves.

Grade evidence explicitly: static inspection establishes a code path; compilation establishes a build; a returned native call establishes an attempted action; verified post-state establishes the specific outcome; subsequent dialogue may establish that tutorial step. A blocked foreground test is an attempted test, not a pass. A successful opening route is not proof of later navigation. Consult [accessibility coverage](../ACCESSIBILITY.md) before changing public claims.


## Code style and releases

Project C++ uses `.clang-format`; the initial cleanup used clang-format 23.1.5. Keep runtime behavior changes separate from formatting where possible. Generated headers and vendored third-party code are not hand-formatted. Python dependency downloads and action versions are explicit in the build tooling; changing a pin requires checksum review and a new build.

Before a release, build into an empty isolated output directory, run the standalone tests, package the matched pair and verify the archive contents/checksums. The packaging script rejects artifacts changed after their build manifest was created. Exercise the installer in a disposable directory before touching the real game. Preserve a previous package and a save backup.

Record release validation honestly: native tests and an installer round trip do not replace in-game screen-reader testing. The first polished prerelease is based on tutorial-tested source, while a fresh packaged gameplay run remains a separate check. Attach the player ZIP and its checksum file to the reviewed source tag, mark experimental builds as prereleases, and include the known limitations. Publication requires the maintainer's authorization.

See [checkpoint recovery](CHECKPOINT.md), [architecture](ARCHITECTURE.md) and [the reverse-engineering workflow](REVERSE_ENGINEERING.md) for the detailed handoff.
