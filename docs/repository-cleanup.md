# Repository cleanup (2026-11-11)

**Index-only cleanup, not disk deletion or a history rewrite.** 566 generated,
runtime and backup files were removed from Git's index using `git rm --cached`.
Every one of those 566 files remained on disk. All 59 runtime/backup file hashes
checked immediately before/after cleanup were identical.

The root `.gitignore` excludes `build/`, `_backup/`, logs, local configuration
INI files, Python caches, IDE state and legacy/local DLL copies. Sources, mod
manifests, documentation, dependency notices and the original supplied WAVs
remain available. No original executable, Data archive, save or offset reference
was removed. The original executable SHA-256 remained
`49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0`.

The staged deletions mean **stop tracking** these paths. Do not use `git clean`,
disk deletes, or a hard reset to finish this operation. Local originals,
settings, logs and historical installation backups are intentionally retained.
The cleanup did not reset settings to defaults or overwrite mod manifests.

No commit, push, force-push, reset or history rewrite was performed. New sources
and other changes are still uncommitted; the user must review what to include.
Ignored files do not automatically disappear from old commits. A force-push of
the unchanged history will **not** purge historical binary blobs or necessarily
reduce the repository's history size. Any history rewrite is a separate task
requiring explicit scope/approval; it was not inferred from this cleanup.

The detailed machine-local audit is retained outside the repository:
`C:\Users\Administrator\AppData\Local\Temp\opencode\ethhh-repository-cleanup-20261111-224307.json`.
