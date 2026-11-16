# Architecture

## Process and thread model

The launcher loads two matched native x64 DLLs into an owned game process. It does not replace game files. The base resolves IL2CPP exports and method metadata, installs MinHook hooks, and routes incoming UI and game state to speech. The desk-controls companion extends the base through a generated private-symbol manifest.

Gameplay operations run inside existing game update hooks on the main thread. The speech worker sends queued text to Prism or the explicitly supplied NVDA fallback. Independent mouse-release monitors use Windows input and focus APIs only. They keep bounded synthetic input from remaining held when game updates stall.

A command can arrive from a fresh keyboard press or the local command file. Both paths call the same guarded handlers. Questions and menus take precedence over gameplay selectors. Focus loss, game-control restrictions, object lifetime, movement and explicit cancellation constrain automation.

## Source map

| Area | Main files | Boundary |
| --- | --- | --- |
| Launch and injection | `src/launcher.cpp` | Load the complete package; report process/library errors. |
| UI capture and speech | `src/access.cpp`, `src/choice_state.h` | Localized labels remain separate from original answer callback IDs. |
| Navigation | `src/navigation.h`, `src/route_planner.h` | Sample floor/body clearance, plan bounded routes, move the native character controller, stop on unsafe context. |
| Studio controls | `src/music_fix.cpp` | Record selection, native placement/playback, room/clue grouping, phone/soundboard selectors, native music slider. |
| Maps | `src/map_reader.h`, `map-research/descriptions.json` | Describe reviewed, discovered evidence; mark unsupported visual details honestly. |
| Build binding | `tools/music_symbols.py` | Derive offsets and signature data from the exact compiled base DLL. |

## Companion compatibility

The companion uses offsets into our base DLL, not fixed addresses into GameAssembly.dll. PE timestamp, image size, function bytes and an explicit 88-byte destination-layout assertion provide runtime checks. The header is generated in the isolated build directory and included with angle brackets; a stale source-directory header must not take precedence.

The package build records artifact hashes, and packaging rejects a changed or mixed build set. Runtime checks are not a security boundary or proof that arbitrary compilers are compatible. Rebuild the pair together and distribute it together. A changed base is rejected before touching older companion hooks. Restarting with a matched package is the supported update path.

## Interaction evidence

A native call returning without an exception supports an action-attempt or button-pressed announcement. State-changing claims need stronger evidence:

- Placement: the exact selected vinyl is attached to the selected deck and no longer player-held.
- Playback and stop: the native playback getter confirms the result.
- Volume: the native normalized slider getter reaches the requested range; pending changes are announced as pending.
- Tutorial progress: subsequent native dialogue is separate evidence from button dispatch.

Record pickup remains an exception needing repair: the prototype mouse click can fail, and its generic held-vinyl check does not prove the browsed title was picked up. This limitation is preserved rather than silently represented as solved.

## Ownership and cleanup

Retain managed objects across updates using GC handles, then release them when selection ends, an action completes, the scene changes or the game takes control. Prefer native availability checks and interaction methods over writing state fields. Selector handles are independent from navigation destination handles. Synthetic input has an independent release path.

Further changes should deepen these boundaries gradually, with one regression signal at a time. A packaging checkpoint is not a reason to rewrite the working runtime wholesale.
