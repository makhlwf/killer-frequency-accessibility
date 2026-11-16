# Accessibility coverage

This mod is experimental. **A complete blind playthrough has not been verified.** It does not provide a Windows accessibility object tree or establish WCAG conformance for the native game.

## Observed locally

The following observations apply to one investigated Windows 64-bit IL2CPP build and the local session, not every store or patch:

- NVDA backend selection through Prism and player-confirmed audible speech.
- Main-menu capture and spoken focus, opening-alley walking, normal door interaction, crossing into the studio, and initial dialogue-choice navigation/submission.
- Nearby record title browsing and a held vinyl in at least one attempt. Pickup remains unreliable and can fail repeatedly.
- A simplified five-entry studio destination list and short turntable approach.
- Turntable placement confirmed through exact deck/vinyl identity and ended player-held ownership; playback confirmed with native `IsPlaying`; music stop confirmed with native stopped state.
- Phone Line 1 native activation followed by the producer-line dialogue; later native line-button activations do not establish a complete phone mission.
- Soundboard effect 1 native activation followed by tutorial dialogue; actual music-slider values of 60, 70, 80, 90, and 100 percent; a subsequent broadcast introduction.

Local evidence identifiers include placement `410602156`, playback `410997140`, stop `460385921`, phone activation `460924343` and subsequent dialogue `460943578`, soundboard `461321750`, slider `461507671` through `461530062`, and broadcast snapshot `461579953`. These are local log timestamps, not downloadable evidence artifacts. Proprietary captures and logs are excluded from the public repository.

## Implemented with limited verification

Later dialogue/localization, every answer position, mixed native/controller input, all response-timer transitions, document reopen behavior, scene-wide interactions, grouped clues, longer routes and other floors, every phone/soundboard control, and map matching on all actual items still need broader testing.

Response timers are paused initially while captured answers are available. This intentionally changes gameplay timing and does not establish that every timer is paused. See the [single controls reference](docs/PLAYER_GUIDE.md#controls).

## Known barriers

Record pickup is a flaky normal mouse interaction with the selected sleeve. There is no complete story-objective tracker. Navigation cannot guarantee stairs, closed doors, narrow passages, other floors, or interactability after arrival. Controller cancellation of record browsing needs further testing.

Map and clue coverage is selective. The town map lacks full road connectivity. The maze description covers legend and markers but has **no verified complete path connectivity**, making it insufficient for all route-dependent choices. Individual evidence photos are withheld until their source regions can be identified. Unknown textures are reported as unsupported. Text descriptions can contain spoilers; no source images are distributed.

## Reporting a barrier

Use the [accessibility issue form](https://github.com/makhlwf/killer-frequency-accessibility/issues/new/choose). Give exact steps, scene, mod/game build identity, speech heard, expected feedback, and a short redacted log excerpt. Distinguish a requested action from a confirmed result. Do not post saves or proprietary game assets.
