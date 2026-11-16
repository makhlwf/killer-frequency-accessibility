# Working on Killer Frequency Accessibility

Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing runtime behavior. For builds and setup, read [the developer guide](docs/DEVELOPER_GUIDE.md). For IL2CPP analysis, main-thread calls, input handling, live injection, or release evidence, read [the reverse-engineering workflow](docs/REVERSE_ENGINEERING.md) and [architecture](docs/ARCHITECTURE.md).

## Preserve the player session

Use an isolated output directory while the game is running. Keep a source commit and matched binary checkpoint before experimental runtime changes. Default development work must preserve the existing running game and its save progress. Test with a disposable or backed-up save when story advancement is necessary.

## Runtime changes

Call Unity/IL2CPP-managed operations from the existing main-thread hooks. Keep strong GC handles for retained managed objects and release them on cancellation or context loss. Preserve foreground, menu, dialogue-choice and game-control guards. Automated walking keeps collision checks and manual cancellation. Synthetic held inputs require independent release monitoring.

Treat `src/music_symbols.h` as generated and excluded from Git. Compile the desk-controls companion using the header generated from that exact base DLL. Publish both DLLs together. Shared C++ structure layouts must match; the current destination layout is explicitly asserted in source.

Prefer normal native game methods and availability checks. Report an attempted action separately from a verified state transition. Preserve current questions and their announcements. Keep prototype limitations visible; do not call the game fully accessible without a verified playthrough.

## Evidence and publication

Use the connected REA tools' actual schemas and the developer's own game files for new investigations. Record the game build identity, static observation, runtime observation, pass/fail signal, and remaining uncertainty. A successful build or injection is not a gameplay test. Keep game binaries, metadata dumps, extracted art/audio, saves, crash dumps, logs, credentials and local checkpoints out of Git and release packages.

Run the documented standalone tests, build into an isolated output folder, and validate the packaged matched binary set before publishing. Review player-facing control and announcement changes with keyboard and screen-reader specialists. Explicit user authorization governs external publication; do not infer it from a request to review code.
