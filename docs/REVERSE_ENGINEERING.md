# Reverse-engineering and runtime workflow

This is the evidence workflow used to build the mod. It requires a developer's own installed copy of the game. The public repository supplies the mod source, not proprietary binaries, metadata, textures, audio, saves or extracted inventories.

## Prepare a controlled session

1. Read the [developer guide](DEVELOPER_GUIDE.md) and [verified coverage](../ACCESSIBILITY.md).
2. Record your game fingerprints and compare them with `supported-build.json`. Note the scene, current prompt, held item and player state.
3. Back up the relevant save and preserve a source commit plus the matched mod binaries. Keep that local checkpoint outside Git.
4. Build to an isolated output directory. A running game locks loaded DLLs, and replacing only one member of a pair is unsupported.

## Inspect static facts with REA MCP

Use the tools actually exposed by your connected REA server; setup and schemas depend on the installed version. The established static tools include `inspect_il2cpp_metadata` and `inspect_analysis_view`. Discover their schemas before invocation rather than copying guessed arguments.

The relevant local inputs are `GameAssembly.dll` and `KillerFrequency_Data/il2cpp_data/Metadata/global-metadata.dat`. Metadata inspection provides type, method, field and signature information. Method-address observations can lead to bounded native disassembly when implementation details matter. A method name or compatible metadata table is not proof of runtime availability.

Keep evidence bundles local. Retained evidence IDs are session-specific: import your local bundle or rerun the producer if a reference is unavailable after restart. Do not retry an unchanged access-denied/runtime-provider failure indefinitely. Use a supported alternative and state what was not observed.

AssetStudio is optional for targeted visual clues. Use it to inspect a specific texture from your local installation, then write a reviewed text description. Confirm labels, orientation, connections and uncertainty. Do not infer complete maze paths from a legend, or distribute the extracted assets.

## Trace the physical control

Identify the current native object, its owning component and its availability checks before invoking an action. For example:

- Phone buttons were identified from actual studio hierarchy paths containing `PhonesLines_Button_One`, `Two` and `Three`; notification components separately expose line IDs and flashing state.
- Soundboard effects and group selectors were identified from their physical `Functional/Button_1` through `Button_8` and `Group1` through `Group3` paths.
- The music fader was identified by `SliderAudioLevels.sliderProperyName == Record_Slider`.
- A record sleeve is not the held vinyl. Placement and pickup identity must follow the actual spawned/held object, not just a displayed title.

The native command-file pump is the supported local diagnostic seam. `snapshot`, `navdump`, `musicdump` and `phonedump` can expose current state without inventing a test provider. See the command-file example in the [developer guide](DEVELOPER_GUIDE.md). Keep dumps and logs private and redact snippets before reporting them.

## Build one regression signal

Start with the reported symptom and capture a bounded failing run. State an observable pass condition such as exact held-object identity, actual slider percentage, native playback state, or a subsequent tutorial prompt. A log saying 'requested' is not that pass condition.

Inspect only enough metadata and runtime state to distinguish hypotheses. Make the smallest source change, build the matched package, run relevant tests, then repeat the same scenario with foreground gameplay. Check cancellation, focus loss, active questions and adjacent normal behavior. Preserve the question's callback identity and never answer a puzzle merely to advance a test unless the player specifically authorized that outcome.

The earlier development session used numbered hot-loaded companion DLLs to preserve progress. That is an experimental technique, not the player upgrade method. It can leave old hooks and watchdog threads resident. Prefer a clean restart with a disposable or backed-up save; do not assume unloading or reinjecting an initialized mod is safe.

## Record a handoff

A useful change report includes the exact build, entry state, reproduction command or keys, native method/check used, verified post-state, spoken feedback, cancellation result and remaining uncertainty. Link the source change to that evidence. Keep static observations, builds, attempts, verified state changes and playthrough coverage separate.

Update public coverage only for the specific verified behavior. Preserve unsupported scenes, unreliable pickup, incomplete map connectivity and later-game uncertainty in the report.
