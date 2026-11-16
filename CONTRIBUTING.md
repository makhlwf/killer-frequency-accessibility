# Contributing

This is an experimental checkpoint. Start with the [developer guide](docs/DEVELOPER_GUIDE.md) and [coverage limits](ACCESSIBILITY.md).

## Useful contributions

Small reproducible fixes, accurate compatibility reports, player-observed barriers, reviewed clue transcriptions, and narrow regression tests are welcome. Record one failing scenario before changing code. Preserve ordinary game progression, foreground/state guards, managed-object lifetimes, and independent mouse-release cancellation.

Build the base and companion together and run relevant checks. Explain what was statically reviewed, compiled, attempted live, and confirmed through native state or subsequent dialogue. Do not infer full accessibility from one successful tutorial interaction. Keep control documentation in the player guide rather than adding another competing key list.

## What to include

Describe the concrete problem, resulting behavior, relevant scene/build, validation performed, and remaining limits. For reverse-engineered selectors, explain the local metadata/hierarchy evidence and how identity is checked at runtime. Use the pull request template.

Do not contribute game binaries, extracted assets, global metadata, decompiled dumps, screenshots, raw inventories, saves, credentials, or unredacted private paths. Keep local investigation evidence outside version control. Dependency changes must retain licenses and use the repository's setup/package workflow.

## Testing with progress

Back up current saves before New Game or progression testing. Tests must not silently reset player progress. Request announcements alone do not count as success. Verify cancellation and failure feedback as well as the intended action. Flag spoilers in issue titles/body where practical and include only the smallest useful log excerpt.
