# Killer Frequency screen reader mod

Open **Play Killer Frequency Accessible.lnk** in the game folder, or run **bin/KillerFrequencyAccessible.exe**. Keep NVDA running. The launcher starts the game and loads the mod; it can also attach to an already running copy. Launching the original game executable alone does not load the mod.

Speech uses Prism 0.18.3, with the user's NVDA controller library as a fallback. Prism selected NVDA successfully on this computer, and the user confirmed audible announcements.

## Controls

- F1: read help.
- F2: repeat the question and answers, focused menu control, or latest content. If no content is available, read active screen text.
- F3 and F4: previous and next answer or control in the focused menu.
- F5: select the reviewed answer, press the focused menu button, request an available interaction with the targeted object, or try a nearby door using the normal mouse hold-and-drag interaction.
- F6: read active screen text, restricted to the focused menu when one is open.
- F7: toggle spoken subtitles. Off initially because dialogue already has recorded speech.
- F8: stop mod speech and automatic movement or door assistance.
- F9: toggle automatic speech. Explicit reading commands remain available.
- F10: toggle pausing dialogue-choice timers. Paused initially while answers are available.
- F11: start walking to the selected destination; press it again to cancel.
- Shift F11 / Control F11: select the next / previous available destination without starting movement.
- Alt F11: announce the selected destination and navigation state. F2 also reports active navigation.
- F12: read a supported map or clue being inspected, or directly faced within three metres.
- Shift F12: cycle descriptions discovered with F12 during this session. Reading a map stops automatic movement.

Manual movement still uses W, A, S, D and the mouse. F3 and F4 do not move the player. Use native left/right keys to adjust sliders and other settings. Function keys are accepted only while the game is foreground, after a fresh key press.

## Automatic walking

In the opening alley, press F11 to walk to the opening entrance. The mod checks the floor and body clearance and plans a route around obstacles. It moves through the game's character controller; it does not teleport the character. A live test followed a 114-point route through the alley to the entrance, and a subsequent test followed a 108-point route.

At the door, F5 tries a short normal mouse hold-and-drag gesture. For the opening entrance only, assistance also attempts to walk through once the door opens and the floor and body-clearance checks pass; it announces this before crossing. F11 can also attempt a route through an already open entrance. Mouse input is released on completion or cancellation, with a separate timed release watchdog. This does not unlock doors or bypass story requirements. A live test used normal door interaction and an eleven-point crossing route, then reached the radio studio and stopped navigation when the game took control. Door assistance may need a second attempt and remains experimental.

W/A/S/D, arrow movement, left-stick input, F11, or F8 cancel walking. Focus loss, menus, dialogue choices, loss of character control, unavailable destinations, blocked movement, and time limits also stop it. The mod does not resume automatically. F8/Escape/W/A/S/D and focus loss also trigger the independent mouse-release watchdog during door assistance.

Later scenes use the game's available movement anchors or a filtered list of doors, maps, notes, clues and equipment. Nearby floors are penalized in the ordering so a basement object does not outrank a usable destination on the current floor merely because it is directly below the character. These are destination choices, not a verified sequence of story objectives. Use Shift F11 to hear the choices. Closed doors, stairs, narrow passages, and different floors may prevent a route; the mod reports failure instead of moving through a wall. Reaching a planned stopping point does not establish that the object is interactable.

When the current seat or interaction disallows movement, F11 can request an available stand/exit action through the game's normal interaction checks, then asks you to press F11 again when standing. If no permitted exit action is available, it reports that the current interaction must be finished first. F5 prioritizes an available interaction with the object actually targeted by the game. These general interaction and seated-exit paths still need broader playtesting.

## Maps and clues

F12 matches an inspected item or the renderer directly hit by the camera's ray to a verified texture description. Descendant scanning is restricted to an identified holdable item. Loaded textures alone do not add clues to the discovery list. Descriptions cover the Gallows Creek town map, the Maize Maze legend and marker positions, four location notes, a handwritten note, and the festival ticket. The evidence-photo atlas is withheld until individual photo regions can be identified.

The town description supplies labels and relative positions, not every road connection. **The maize maze's complete path connectivity has not been transcribed or verified. Its current description is insufficient for choosing every answer that depends on a route through the maze.** The mod describes evidence and does not automatically choose puzzle answers. Unknown textures receive a spoken unsupported-content message.

Read the [map transcription review](map-research/maze-topology-review.json) for the maze coverage boundary and the [navigation investigation](investigation.md) for the implementation evidence.

## Implemented behavior

The mod hooks the game's `Services.WorldUIManager.ShowDialogueChoice` method to receive the localized question, answer strings, answer IDs, callback, and timeout. It announces the question before the answers and includes each answer's position and total. F5 sends the captured ID to the game's callback. Old question state is cleared when the answer UI closes or is replaced.

The answer cursor synchronizes with the answer controls in Unity hierarchy order. Activation requires the complete expected answer-control set. Menu navigation stays within the focused menu and uses Unity's directional navigation when available.

Tutorial and interaction text is read when its corresponding UI is active and visibly faded in. Document text is read when the game opens its text display. Its automatic announcement is cancelled when it closes; a manually requested repeat may continue until F8 is pressed. F6 offers a broader active-text reading fallback; active text is not proof that it is visible on screen or relevant to the current puzzle. Control sprites use their explicit names or names from the text component's sprite asset. Unresolved sprites are announced as “control icon.”

The timer option passes zero elapsed time to `Services.DialogueManager.OnUpdate` while the captured answer UI remains active. This changes normal response timing. It does not pause the entire game or stop recorded audio.

## Validation and current limits

Verified: compilation, choice-ID mapping and reset tests, route-planning obstacle/cliff/limit tests, Prism selecting NVDA, successful speech output, user-confirmed audible speech, runtime initialization, main-menu text capture, menu announcements, opening-alley walking, normal door interaction, crossing to the radio studio, and capture/navigation/submission of the first dialogue choices.

Still requires broader playtesting: localized dialogue in later scenes, selection through every answer position, mixed keyboard/controller selection, timed choices remaining available, all post-selection transitions, document reopening, maps matched to actual items, and navigation/tutorial/interaction behavior throughout the game. These are implemented paths, not a claim of complete playthrough validation.

This is an initial accessibility mod with experimental automatic walking. It does not establish successful navigation throughout every scene, descriptions of every map or visual puzzle, or a Windows accessibility object tree. A complete blind playthrough has not been established.

## Files and recovery

The original game executable and assets are unchanged. To play without the mod, close the game and start `KillerFrequency.exe` directly. To remove it, close the game and remove this folder and the accessible-launch shortcut.

`bin/accessibility.log` records initialization, speech, commands and captured dialogue. It may contain story spoilers. `save-backup` contains the save files copied before the first new-game test; it is not a backup of later progress.

`src` contains the mod, launcher, choice-state tests and route-planning tests. `build.ps1` rebuilds using the installed GCC/G++ and Python tools, and prepares the map-description lookup table. Close the game before rebuilding because Windows locks the loaded DLL. `rea-evidence.json` contains the static REA metadata evidence used for the investigation. Static evidence does not establish runtime behavior.

## Third-party components

- [Prism source and releases](https://github.com/ethindp/prism): MPL-2.0; licenses are included under `vendor/prism/LICENSES`.
- [MinHook source](https://github.com/TsudaKageyu/minhook): its license is included at `vendor/minhook/LICENSE.txt`.
- `bin/nvdaControllerClient64.dll` was copied from this computer's `C:/nvgt/lib` installation for the requested NVDA fallback. The NVDA controller license is included in the Prism license collection.

This package is prepared for this local game installation. Redistribution and compatibility with other game builds have not been verified.


## First music request assistance

F11 faces and announces a nearby record for the reported first music request. F3/F4 browse titles, F5 attempts normal pickup, and F8 leaves selection. Questions and menus retain priority. Browsing stops on focus loss, scripted control changes, and keyboard movement. Controller cancellation needs further testing; use F8 before moving with a controller.

The game creates a separate vinyl object when taking a record from a sleeve. Confirmation reads its Holdable component and player holder. Verified live: title browsing, physical record in hand, and runtime confirmation of a held vinyl. Turntable insertion and playback are not yet verified. Once holding a record, Shift F11 selects a normal walking destination and F11 walks there.

The launcher automatically loads KillerFrequencyMusicFix-v5.dll, tied to this exact base DLL build. The current session received the fix without restarting. build.ps1 regenerates the companion manifest. bin/music-fix.log records diagnostics.


## Simpler destinations and turntable assistance

The current launcher loads KillerFrequencyMusicFix-v10.dll, superseding the earlier music companion. F11 defaults to nearby record selection before pickup, then the turntable while holding vinyl. If close enough, it faces the turntable; otherwise it attempts a collision-checked floor approach. F11 during walking cancels; F8 stops assistance and restores the automatic music destination after manual browsing.

Shift F11 / Control F11 browse a small places list. Verified in the current studio: five entries (Turntable, Studio door, Restroom door, Office door, Notes and clues). Repeated rooms, incidental mixer controls, and paper-towel props are excluded. Notes and clues remain available in their own group: select the group and press F11, then F3/F4 choose an individual clue. F11 attempts walking to that clue; F8 returns to places. Group navigation has passed static review; its complete runtime interaction remains unverified. Name filtering is not a guarantee that every puzzle item is covered.

Live verified: five-item main list, default nearby turntable facing, and a short automatic route ending at the turntable approach. Long routes, every room, turntable insertion/playback, and later story objectives remain unverified. The music sequence is currently the supported automatic task, rather than a complete story-objective tracker.


## Turntable placement

The launcher now loads KillerFrequencyMusicFix-v11.dll. At the turntable, F11 faces the record dock. F5 attempts placement using the native PhysicsHoldableDock.CanDock and Dock actions, rather than falling through to menu selection. The action requires normal player control, foreground game, no active question/menu, a held vinyl, nearby turntable, and facing the dock or active turntable assistance.

Placement success is announced only when the selected turntable's vinyl equals the selected held record and the player no longer holds it. A timeout gives retry guidance. F8 or context changes clear pending feedback. Placement and playback are separate: this change does not assert playback began. Compilation, loading and specialist static reviews passed. Foreground placement and audible confirmation still require live verification; Windows refused foreground activation during the agent's attempted test.


## Phone lines and stopping music

The launcher now loads KillerFrequencyMusicFix-v19.dll. Shift F5 stops the nearest playing studio record; it reports success only after native IsPlaying confirms false. Live stop confirmation was recorded at 460385921.

Shift F6 opens the three physical studio phone buttons, initially Line 1. F3/F4 choose a line, F11 approaches/faces it, F5 presses its native button, F2 repeats selection, and F8 exits. Plain F6 still reads current screen text. Questions and menus take priority; movement, loss of focus, or a new dialogue question exits phone selection. Reopen with Shift F6 when needed.

Mapping uses runtime-verified PhonesLines_Button_One/Two/Three objects and PhoneLineFlashing IDs 1/2/3. A flashing-indicator announcement reflects the native indicator rather than an inferred call connection. F5 checks foreground, player control, menus, questions, reach and cooldown. It reports a pressed button only after exception-aware native Activate returns; it does not assert a call connected.

Verified live: approach to Line 1, native button activation (460924343), and subsequent dialogue snapshot 'You mean the producer line?' (460943578). Keyboard/ARIA specialist reviews found no blockers; tables and links are inapplicable. Other line activations and later call progression remain unverified. The earlier flaky record-pickup path still requires repair; the interrupted pickup rewrite did not land.


## Soundboard and music slider

The launcher now loads KillerFrequencyMusicFix-v22.dll. Shift F7 opens the Sound Blaster soundboard selector: eight numbered effect buttons and three sound groups. F3/F4 select, F11 approaches, F5 presses the physical native button, and F8 exits. Phone and soundboard selection are mutually exclusive. Plain F7 still toggles spoken subtitles.

Shift F3 lowers the music slider by ten percentage points; Shift F4 raises it. These act on SliderAudioLevels with sliderProperyName exactly Record_Slider through Slideable.MoveSliderToNormalizedValue. Values remain within 0-100%, and spoken percentages follow the actual native getter. Pending endpoint requests are described as pending. Menus, active dialogue choices, background focus and unavailable character control block adjustment without moving answer selection. Plain F3/F4 retain their existing selection role.

Verified live: soundboard effect button 1 was activated through its native callback; Peggy then requested playing a record and changing music volume. Native music-slider state changed from 50 to 60%, then user inputs reached 70/80/90/100% with confirmed speech. A subsequent snapshot captured Forrest introducing the broadcast. These observations establish this tutorial progression, not every soundboard effect or later puzzle. Static accessibility reviews found no remaining blocker for these additions.
