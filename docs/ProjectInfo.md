# WHDDownloader Session Instructions

This document summarizes the features implemented during this chat session.

## Scope Added In This Session

- Integrated archive extraction into the main download flow.
- Added extraction configuration via CLI and INI.
- Added startup validation for extraction requirements.
- Added `ArchiveName.txt` metadata writing after successful extraction.
- Added skip-if-already-extracted logic before extraction.
- Added pre-download skip logic using existing extraction markers to avoid slow re-downloads.
- Added `.archive_index` per-letter cache files to replace expensive full directory scans.
- Added automatic migration path that can build `.archive_index` from existing `ArchiveName.txt` markers.
- Added stale-entry handling for `.archive_index` when users manually delete extracted folders.
- Added cache lifecycle hooks: runtime update after successful extraction and flush on shutdown.
- Added force/override flags for both download and extraction skip systems.
- Updated help text, sample INI, and README documentation.
- Added session report module to persist end-of-run archive activity.
- Added NEW vs UPDATED classification for successfully downloaded archives.
- Added local-cache reuse tracking (explicitly recorded as no-download activity).
- Added explicit runtime log line when local archive cache is reused.
- Fixed `FORCEDOWNLOAD` behavior so it bypasses local-archive short-circuit and truly forces HTTP fetch.

## New Runtime Behavior

1. Download completes successfully.
2. If extraction is enabled, archive extraction is attempted.
3. If marker-based extraction skip is enabled and marker matches, extraction is skipped.
4. If pre-download marker skip is enabled and marker matches, download is skipped before the direct HTTP download runs.
5. For heuristic misses, pre-download skip now checks `.archive_index` first (fast lookup) instead of scanning every subfolder.
6. If index lookup points to a folder that no longer exists, the stale index entry is removed and normal download/extract continues.
7. After successful extraction and metadata write, `.archive_index` is updated in memory and flushed safely during shutdown.
8. If an archive already exists in `GameFiles/...` and `FORCEDOWNLOAD` is **not** set, the tool can reuse that local archive for extraction/recovery instead of HTTP download.
9. Local archive reuse is now visible in two places:
   - Runtime log (`download: reused local archive cache ... (no HTTP download)`)
   - Session report section `Local cache reuse (no download)`
10. If `FORCEDOWNLOAD` is set, local archive short-circuit is bypassed and network download path is used.
11. Session report file is written at end of run when any reportable activity exists, grouped by pack with totals.

## Session Report System (What Users See)

Purpose:

- Persist a clear, human-readable record of what changed in a run.
- Distinguish real network download activity from local recovery/re-extraction activity.

Location:

- `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`

Current categories:

- `New` — newly downloaded archives not matched to prior title in index.
- `Updated` — downloaded archives matched to an existing title (version/variant bump path).
- `Local cache reuse (no download)` — archive handled from existing local `GameFiles` cache.

Console behavior:

- Prints report path and category totals when report data exists.
- Prints `No new archives this session.` only when no reportable entries were collected.

Why this benefits users:

- Removes ambiguity between "downloaded from server" and "processed from local cache".
- Makes troubleshooting easier when expected downloads do not occur.
- Gives a persistent session audit trail for later review/manual updates.
- Helps users understand whether content changed due to network updates or local marker/index recovery.

## Marker File Contract

Path format:

- `<target>/<pack>/<letter>/<game_folder>/ArchiveName.txt`

File format:

- Line 1: category text (for example `Games` or `Games (Beta & Unofficial)`).
- Line 2: exact archive filename (for example `AbuSimbelProfanation_v1.0_AGA.lha`).

Matching rule:

- Exact case-sensitive match against line 2.

## Archive Index Contract (`.archive_index`)

Purpose:

- Accelerate pre-download skip checks when archive filename heuristics do not match the real extracted folder name.
- Avoid repeated `ExNext()` + `ArchiveName.txt` reads across large letter folders on classic Amiga storage.

Path format:

- `<target>/<pack>/<letter>/.archive_index`

Examples:

- `GameFiles/Games/A/.archive_index`
- `Games:/Games/A/.archive_index` (when `EXTRACTTO` is active)

File format:

- One entry per line.
- Two TAB-separated fields:
   - Column 1: exact archive filename (for example `AcademyAGA_v1.0.lha`)
   - Column 2: real extracted folder name (for example `Academy`)

Lookup rule:

- Exact case-sensitive match on column 1.

Write/update rule:

- If archive already exists in index: update folder field.
- If archive is new: append entry.

Stale entry rule:

- If index maps to a folder that no longer exists, entry is removed and file is corrected on flush.

## CLI Options Added/Active

Extraction control:

- `NOEXTRACT` disables extraction.
- `EXTRACTTO=<path>` extracts to a separate destination.
- `KEEPARCHIVES` keeps downloaded archives after extraction.
- `DELETEARCHIVES` deletes downloaded archives after successful extraction.
- `EXTRACTONLY` extracts already-downloaded archives from DAT lists.

Extraction skip override:

- `FORCEEXTRACT` forces extraction even when `ArchiveName.txt` already matches.

Pre-download skip control:

- `NODOWNLOADSKIP` disables marker-based pre-download skip.
- `FORCEDOWNLOAD` always downloads even if a marker match is found.

Maintenance commands (destructive, with confirmation prompts):

- `PURGETEMP` permanently deletes `PROGDIR:temp` including all files and subfolders.
- `PURGEARCHIVES` permanently deletes downloaded `.lha` archives under `GameFiles` recursively.
- `PURGEARCHIVES` does **not** delete extracted game folders.
- Both commands print a brief warning and require explicit `Y` confirmation.

Icon control:

- `NOICONS` disables custom icon lookup and always uses the system default drawer icon.

## Drawer Icons for Extracted Folders

After each directory in the extraction hierarchy is created (base, pack, and letter folders), a
Workbench drawer icon is written automatically so every folder appears correctly in Workbench.

### Custom icon lookup

If the folder `PROGDIR:Icons/` exists, WHDDownloader attempts to match the folder's leaf name
against a `.info` file inside it before falling back to the system default drawer icon.

Examples:
- `PROGDIR:Icons/A.info` → used for all letter-`A` folders
- `PROGDIR:Icons/Games Beta.info` → used for the Games Beta pack folder
- `PROGDIR:Icons/extract.info` → used for the top-level extract root folder

The match is case-insensitive (AmigaOS filesystem handles this automatically).  
If no matching custom icon is found, `GetDefDiskObject(WBDRAWER)` is used as the fallback.  
If the `Icons` folder does not exist, custom lookup is silently skipped — the feature is
effectively auto-enabled just by dropping the folder in place.

An existing icon is never overwritten (the `.info` file is checked before any icon call is made).

### WHDLoad game folder icons (`WHD folder.info`)

WHDLoad archives typically ship their own MagicWB-style `.info` sidecar next to the game folder
(e.g. `Alien3.info` beside `Alien3/`). Placing a file named exactly `WHD folder.info` inside
`PROGDIR:Icons/` tells WHDDownloader to replace every such icon after extraction:

1. After LHA extraction completes successfully, the resolved game folder name is known
   (e.g. `Alien3`).
2. If `PROGDIR:Icons/WHD folder.info` exists, the existing `Alien3.info` (if any) is replaced:
   - `SetProtection` is called on the existing icon to clear all protection bits.
   - The icon is deleted.
   - The template is loaded with `GetDiskObject` and written via `PutDiskObject` under the game
     folder name.
3. If `SetProtection` or `DeleteFile` fails (e.g. read-only volume), the replacement is skipped
   **gracefully** — the archive extraction result is unaffected.
4. The template file itself is never modified; a fresh copy is loaded from disk for every game.
5. If `PROGDIR:Icons/WHD folder.info` is absent, behaviour is unchanged (existing icons kept).

### Unsnapshot copied custom icons

If copied icons contain saved Workbench snapshot coordinates, Workbench can draw many icons at
the same position. WHDDownloader now clears saved icon positions after each successful copy from
`PROGDIR:Icons/` by calling `strip_icon_position()` on the destination icon.

Scope:
- Applies to structural custom drawer icons copied from `PROGDIR:Icons/<name>.info`.
- Applies to copied `WHD folder.info` replacements for extracted game folders.
- Does not alter icons that were not copied from `PROGDIR:Icons/`.

Failure handling:
- Unsnapshot failures are logged as warnings.
- Extraction continues; this is non-fatal.

Control:
- `unsnapshot_icons=true|false` in INI only (no CLI flag).

### Disabling custom icons

Custom icons (both drawer icons and the WHD folder template) can be disabled in two ways:

- Pass `NOICONS` on the CLI — forces `use_custom_icons = FALSE` for that run.
- Set `use_custom_icons=false` in `WHDDownloader.ini` — persists across all runs.

When disabled, the system default drawer icon is always used for structural folders, and
`WHD folder.info` replacement is never attempted.

## INI Keys Added/Active (`[global]`)

- `extract_archives=true|false`
- `skip_existing_extractions=true|false`
- `skip_download_if_extracted=true|false`
- `force_download=true|false`
- `extract_path=`
- `delete_archives_after_extract=true|false`
- `use_custom_icons=true|false`
- `unsnapshot_icons=true|false`

## Default Settings

- `extract_archives = true`
- `skip_existing_extractions = true`
- `skip_download_if_extracted = true`
- `force_extract = false` (CLI-only runtime flag)
- `force_download = false`
- `delete_archives_after_extract = true`
- `extract_path = (empty / in-place)`
- `use_custom_icons = true`
- `unsnapshot_icons = true`

## Skip Logic Details

Extraction skip (post-download):

- Uses resolved target directory.
- Tries heuristic folder name first.
- Compares `ArchiveName.txt` line 2 with incoming archive filename.
- Skips extraction on exact match unless `FORCEEXTRACT` is set.

Pre-download skip (before direct HTTP download):

- Uses same resolved target directory logic.
- First checks whether the archive already exists in local `GameFiles/...` cache (fast local reuse path).
- Tries heuristic folder marker first.
- Tier 2 now uses `.archive_index` lookup first (fast path).
- If `.archive_index` cannot be loaded, fallback scan still uses child folders + `ArchiveName.txt` markers.
- Skips download on exact marker match unless `FORCEDOWNLOAD` is set or `NODOWNLOADSKIP` is used.
- With current behavior, `FORCEDOWNLOAD` also bypasses local `GameFiles` archive reuse and forces HTTP fetch.

### Why this was changed

- The old Tier 2 fallback scanned all subfolders in a letter directory whenever the heuristic failed.
- Heuristic misses are common (archive filename and folder name often differ), so full scans happened frequently.
- On Amiga HDD/CF media, repeated directory traversal and file opens are expensive and significantly slow full pack checks.
- `.archive_index` turns this into one file load per letter and then in-memory lookups, while keeping safe fallback behavior.

## Archive Index Lifecycle

Load path:

- Called during pre-download skip checks after heuristic mismatch.
- If cache for target letter directory is already loaded, reuse it.
- If no index file exists, migration build can scan existing `ArchiveName.txt` markers and create index data.

Update path:

- Called after successful extraction and successful `ArchiveName.txt` write.
- Updates in-memory cache immediately so subsequent checks in same run benefit without extra disk I/O.

Flush path:

- Called on shutdown.
- Persists dirty cache to `.archive_index`.
- Designed to keep extraction flow non-fatal: failure to write index logs a warning but does not invalidate extraction result.

## Reliability and Recovery Notes

- `.archive_index` is an optimization layer; `ArchiveName.txt` remains the authoritative compatibility marker.
- If index file is missing/corrupt/unreadable, runtime falls back safely (scan path or normal download/extract behavior).
- Malformed index lines are skipped defensively.
- Manual folder deletion is handled by stale-entry removal when folder verification fails.
- Feature is transparent with and without `EXTRACTTO` because target directory resolution is shared.

## Developer Notes (for future manual writing)

Code integration points:

- `extract_is_archive_already_extracted()`:
   - Tier 1 heuristic unchanged.
   - Tier 2 now index-first with folder existence verification and stale cleanup.
- `extract_process_downloaded_archive()`:
   - Calls index update after metadata write succeeds.
- `do_shutdown()`:
   - Flushes extract index cache before INI cleanup/log shutdown.

Design intent:

- Keep correctness equivalent to prior logic.
- Improve speed on large libraries.
- Ensure failures in index handling never block core download/extract operations.

## Startup Validation Added

If extraction is enabled:

- `c:lha` must exist.
- `extract_path` (if set) must exist.
- `extract_path` (if set) must be writable.

If validation fails, startup exits with an error.

## Example Commands

Default behavior with extraction and marker-based skip systems:

```text
WHDDownloader DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES
```

Force extraction even if marker matches:

```text
WHDDownloader DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES FORCEEXTRACT
```

Force download even if already extracted marker matches:

```text
WHDDownloader DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES FORCEDOWNLOAD
```

Disable only pre-download marker skip:

```text
WHDDownloader DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES NODOWNLOADSKIP
```

Extract existing local archives only:

```text
WHDDownloader DOWNLOADBETAGAMES EXTRACTONLY EXTRACTTO=Games: KEEPARCHIVES
```

## Notes

- Folder names inside archives are not assumed to match archive filename prefixes.
- Marker matching is authoritative for skip decisions.
- If marker file is missing or unreadable, normal download/extract flow continues.

## Known Limitations

### No pre-download "what's new" preview

WHDDownloader currently has no way to show the user what new or updated archives are
available on the server *before* downloading and applying them. The session report
(`PROGDIR:updates/updates_*.txt`) classifies each downloaded archive as NEW or UPDATED,
but this information is only available *after* the download run completes.

This means the user cannot:
- Preview what would be downloaded without actually downloading it
- Selectively skip individual new archives before they are fetched

**Workaround:** Use the existing skip filter flags (`SKIPAGA`, `SKIPCD`, `SKIPNTSC`,
`SKIPNONENGLISH`) to exclude broad categories of content before running. Review the
session report after each run to see exactly what was added or updated.

**Future release:** A dry-run preview mode is planned that will compare the latest
server DAT files against the local archive index and produce a report of what would be
downloaded — without actually fetching any archives or modifying local state.
