# Player guide

This experimental checkpoint supports one investigated Windows 64-bit IL2CPP installation. A complete blind playthrough is **not verified**. Read [accessibility coverage](../ACCESSIBILITY.md) before relying on it for a new playthrough.

## Install and launch

1. Close the game and back up your current saves. New Game, dialogue answers, and testing can change progress. The mod does not maintain save backups. A developer's local backup is not supplied and would not protect your saves.
2. Obtain a matched player ZIP from the project's [releases page](https://github.com/makhlwf/killer-frequency-accessibility/releases), if available. A source ZIP is not playable; developers can use the [developer guide](DEVELOPER_GUIDE.md).
3. Compare the downloaded ZIP's SHA-256 with the companion `.zip.sha256` asset, for example with `Get-FileHash -Algorithm SHA256 ./KillerFrequencyAccessibility.zip`. After verification, extract the player ZIP. In PowerShell, run its installer, replacing the example path with your actual game folder:

   ```powershell
   ./AccessibilityMod/scripts/install.ps1 -GameDirectory 'C:/Games/Killer Frequency'
   ```

4. Start NVDA, then run `AccessibilityMod/bin/KillerFrequencyAccessible.exe` inside your game folder. The launcher starts or attaches to the game. Keep only the intended copy running and keep it in the foreground for mod keys.
5. Wait for initialization and press F1 for spoken help. Use Fn or function lock if your keyboard reserves function keys for media controls.

If Windows marks a verified download as blocked, use the ZIP's Properties dialog to unblock it before extraction. Local PowerShell policy may require unblocking the extracted installer too. Follow your device's policy; do not disable antivirus software or grant administrator privileges as a workaround.

The expected layout is `Killer Frequency/AccessibilityMod/bin/KillerFrequencyAccessible.exe`, with the original `KillerFrequency.exe` beside the `AccessibilityMod` folder. Use all files from the same package: base DLL and studio companion must match. Compare game fingerprints before assuming another game patch works. NVDA through Prism is the verified backend; other speech backends need testing.

## Controls

Menus and dialogue take priority over studio selectors. Keys require a fresh press after focus returns. Manual movement uses the game's W/A/S/D and mouse controls.

| Key | Action |
| --- | --- |
| F1 | Read help. |
| F2 | Repeat question/answers, focused control, selector item, navigation state, or latest content. |
| F3 / F4 | Previous / next answer or menu control; browse an active record, clue, phone, or soundboard selector. Does not walk. |
| F5 | Submit answer, activate menu control, or attempt supported interaction: pickup, placement, playback, selected physical button, targeted object, or nearby door. |
| Shift F3 / Shift F4 | Lower / raise studio music volume by ten percentage points, within 0–100%. |
| Shift F5 | Stop nearby studio music, checking the native stopped state. |
| F6 | Read active screen text, restricted to the focused menu when open. |
| Shift F6 | Open three studio phone controls, initially Line 1. |
| F7 | Toggle spoken subtitles; initially off because dialogue is voiced. |
| Shift F7 | Open soundboard controls: eight effects and three sound groups. |
| F8 | Stop speech/assistance, cancel walking and pending interactions, exit selectors, and restore automatic music-task selection. |
| F9 | Toggle automatic speech; manual reading remains available. |
| F10 | Toggle response-timer pausing; initially on while answers are available. |
| F11 | Walk to/face selection, perform the next supported music step, approach selected studio control, or open the clue group. During walking, cancel. |
| Shift F11 / Control F11 | Select next / previous place without walking. |
| Alt F11 | Announce destination and navigation state. |
| F12 | Describe supported inspected map/clue or one directly faced within three metres; stop movement. |
| Shift F12 | Cycle descriptions discovered with F12 during this session. |

Use native left/right keys for ordinary menu sliders. Music shortcuts require foreground gameplay, character control, and no menu or active question. Spoken volume follows native state; pending requests are not confirmations.

**Timer pausing changes gameplay timing.** It supplies zero elapsed time to the dialogue manager while captured answers remain available. It does not pause the whole game or recorded dialogue and is not verified for every timer. F10 restores normal timing, allowing answers to expire.

## Opening and studio tasks

In the opening alley, request walking to the entrance, then attempt door interaction. Door assistance may need another attempt. Only this entrance has automatic follow-through after opening and clearance checks. A live run reached the studio; later routes remain limited.

Questions are spoken before answers with their positions. Review and submit your intended answer. Initial capture and submission have live evidence; later scenes and every timed transition need testing.

For the first music task, automatic assistance faces nearby records. Browse titles and attempt pickup. Pickup uses a brief normal mouse click on the selected sleeve and can fail repeatedly. A held vinyl has been observed, but pickup is not dependable. Facing a sleeve does not mean pickup succeeded.

With vinyl held, automatic assistance selects the turntable and attempts a floor approach or faces it nearby. Interaction requests native dock placement. After confirmed placement, face it again and request playback. Placement and playback have separate state checks: a request announcement is not success. Both were confirmed locally, along with stopping music.

Open the phone or soundboard selector, browse, approach, then activate. Repeat reads selection; cancel exits. Selectors are mutually exclusive. Movement, focus loss, menus, or a new question can close them. A pressed-button announcement confirms native activation returned, not that a call connected or a puzzle succeeded. Phone Line 1 and soundboard effect 1 have subsequent dialogue evidence.

## Places and clues

The studio main list was observed with five entries: Turntable, Studio door, Restroom door, Office door, and Notes and clues. It is contextual, not a complete objective tracker. Open the clue group, browse individual clues, then request walking. The complete grouped-clue workflow still needs live verification.

Routing checks floor/body clearance and uses the game's character controller. It does not teleport or unlock doors. Closed doors, stairs, narrow passages, and floor differences can prevent routes. Arriving near an object does not establish interactability.

Descriptions cover the town map, maze legend/markers, four location notes, a handwritten note, and festival ticket. Town road connections are incomplete. **Maze path connectivity is unverified; the description cannot resolve every route-dependent answer.** Individual evidence photos and unknown textures are unsupported. Descriptions contain potential spoilers. No extracted source images are distributed.

## Recovery and removal

Manual movement, cancel, and the walking action stop automatic walking. Focus loss, menus, questions, blocked progress, and time limits also stop it; it does not resume automatically. Door/pickup assistance has a separate mouse-release watchdog for cancellation, F8, Escape, W/A/S/D, focus loss, and time limits. Cancel before controller movement if a record selector remains active; controller cancellation needs more testing.

If speech is absent, confirm NVDA runs, the game is foreground, and speech DLLs are present. Close the game, replace the complete package with a matched build, and relaunch through the accessible launcher. Check `bin/accessibility.log` and `bin/music-fix.log`. Logs can contain story spoilers/private paths. Companion mismatch can leave basic menu speech working while studio assistance is unavailable.

If the game hangs, close normally where possible and restart; unsaved progress may be lost. Do not overwrite loaded DLLs or repeatedly inject experimental variants into one process.

To play without the mod, close the game and start `KillerFrequency.exe` directly. To uninstall, close the game and remove `AccessibilityMod` and any accessible shortcut you created. The mod does not replace game executables/assets. Removal does not undo progress or restore saves.

Use the [bug report form](https://github.com/makhlwf/killer-frequency-accessibility/issues/new/choose) with package identity, game fingerprint, scene, exact action, expected result, actual speech, and a short log excerpt. Remove private paths; do not publicly upload saves, game files, or extracted assets.
