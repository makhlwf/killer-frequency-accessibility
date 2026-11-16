# Killer Frequency Accessibility

Experimental screen reader and keyboard assistance for the Windows 64-bit IL2CPP version of **Killer Frequency**. This is a public development checkpoint with source, build tools, and a player-package workflow. A complete blind playthrough has **not** been verified.

The mod speaks menus and dialogue choices and provides response-timer pausing, assisted walking, selected clue descriptions, and controls for studio records, phones, soundboard buttons, and music volume. The player confirmed audible NVDA speech through Prism.

## Start here

- [Player guide: installation, controls, recovery, and limits](docs/PLAYER_GUIDE.md)
- [Developer guide: setup, architecture, diagnostics, and verification](docs/DEVELOPER_GUIDE.md)
- [Accessibility coverage and evidence](ACCESSIBILITY.md)
- [Contributing guidelines](CONTRIBUTING.md)
- [Checkpoint changelog](CHANGELOG.md)

Use your own legally obtained game installation. Game binaries, saves, extracted textures, screenshots, and raw reverse-engineering dumps are not included. Compatibility is limited to the investigated local build; consult [supported-build.json](supported-build.json) for fingerprints. Other stores, patches, languages, and operating systems need testing.

## Current checkpoint

Live observations establish spoken menus, opening-alley walking and studio entry, initial dialogue-choice submission, turntable placement and playback/stop, the first phone-line interaction, soundboard effect 1, and music-slider changes from 60 through 100 percent. Sleeve pickup remains unreliable, later navigation is limited, and the maze description lacks verified route connectivity.

**Dialogue-choice timers are paused initially while answers are available.** This changes gameplay timing; see the player guide for the toggle and its limits.

## Licensing

Original code is covered by [LICENSE](LICENSE). Dependencies retain their own licenses; consult [third-party notices](THIRD_PARTY_NOTICES.md) and dependency license files before distributing builds. The game and its assets remain the property of their respective owners.
