# Working checkpoint and recovery

## Source checkpoint

The annotated tag `checkpoint-v22` preserves the working prototype source before repository polishing. It records the tutorial-tested version, including its historical build scripts. Those scripts depend on the original developer environment; use current main and the developer guide for portable builds.

Inspect the checkpoint in a separate worktree rather than replacing current work:

```powershell
git worktree add ../killer-frequency-checkpoint checkpoint-v22
```

This tag is a source checkpoint, not a saved game or a universal compatible binary package. The public prerelease is built from the polished main branch, with matched stable filenames and current documentation.

## Original local runtime checkpoint

On the original development machine, `checkpoints/2026-11-16-working-v22` contains the exact original base DLL, v22 companion, launcher, speech libraries, source and build scripts. Its `checkpoint.json` lists SHA-256 hashes. This folder is intentionally ignored by Git and is not uploaded or included in releases.

To restore that local runtime, close the game, verify the recorded hashes, preserve the currently installed package separately, then restore the checkpoint's complete `bin` contents to `AccessibilityMod/bin`. Do not combine its v22 companion with a newly built base. Its launcher expects the historical v22 filename. The original accessible shortcut still points to that installed bin directory.

This backup does not restore gameplay progress. Preserve game saves separately before replaying, selecting New Game or testing narrative transitions.
