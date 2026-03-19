# whdfetch Session Instructions

This document summarizes the features implemented during this chat session.

## Scope Added In This Session

- Integrated archive extraction into the main download flow.
- Added archive-type-aware extraction support for both `.lha` and `.lzx` files.
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
- Added NEW vs UPDATED classification for successfully downloaded archives using strict metadata identity + version progression.
- Added local-cache reuse tracking (explicitly recorded as no-download activity).
- Added explicit runtime log line when local archive cache is reused.
- Fixed `FORCEDOWNLOAD` behavior so it bypasses local-archive short-circuit and truly forces HTTP fetch.
- Added extraction-skip reporting in session updates (including clear UnLZX install guidance when `.lzx` extraction is skipped).
- Added queued download counters with global pre-count and per-download `Download X of Y` status.
- Added queued size tracking from DAT metadata and `MB left` display.
- Added `DISABLECOUNTERS` CLI/INI switch to disable current and future counter-style output.
- Added DAT list metadata persistence in TSV form (`name<TAB>size<TAB>crc`) with backward compatibility for legacy name-only lines.
- Added unified retry handling for network and CRC mismatch failures under one attempt budget.
- Added download-failure reporting section in updates reports (with per-pack and total failure counts).
- Added explicit CRC status output during archive processing (`CRC OK` / `CRC failed`) regardless of `QUIET` mode.
- Added `CRCCHECK` CLI and INI setting (`crccheck` / `crc_check`) to enable CRC verification (default OFF).
- Added startup console status line showing CRC mode (`ON` or `OFF`) to surface feature availability.

## New Runtime Behavior

1. Download completes successfully.
2. If extraction is enabled, archive extraction is attempted.
   - `.lha` archives are extracted with `c:lha`.
   - `.lzx` archives are extracted with `c:unlzx` when available.
   - If `c:unlzx` is missing, `.lzx` archives are skipped with a warning and processing continues.
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

12. UPDATED detection now uses strict identity matching, not title-prefix matching.
13. Archives with missing or non-numeric version tokens are always treated as `New` (never `Updated`).

## Session Report System (What Users See)

Purpose:

- Persist a clear, human-readable record of what changed in a run.
- Distinguish real network download activity from local recovery/re-extraction activity.

Location:

- `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`

Current categories:

- `New` — newly downloaded archives with no strict identity/version update match.
- `Updated` — downloaded archives that match an existing strict identity and have a higher version.
- `Local cache reuse (no download)` — archive handled from existing local `GameFiles` cache.
- `Extraction skipped` — archive extraction intentionally skipped (for example when `c:unlzx` is missing for `.lzx`).

### How UPDATED filenames are detected

Update classification is performed from the in-memory `.archive_index` cache at download time,
before extraction updates the cache entry.

Strict identity fields (all must match):

- `title`
- `special` (cracker/group/special token segment)
- `machineType`
- `videoFormat`
- `language`
- `mediaFormat`
- `sps`
- `numberOfDisks`

Version rule:

- Candidate old archive is considered an update source only when the new filename version is
   strictly greater than the old filename version.
- If multiple matching old candidates exist, the highest older version is selected as the
   `(was ...)` source in the report.

Missing-version hardening:

- If the new archive has no numeric version token, it is treated as `New`.
- If a cached old candidate has no numeric version token, it is ignored for update matching.

Why this change was made:

- Prevent false updates where many different releases share the same primary title prefix
   (for example multiple `Megademo_*` variants by different groups).

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
- Line 2: exact archive filename (for example `AbuSimbelProfanation_v1.0_AGA.lha` or `Mnemonics_v1.0_AGA_Haujobb.lzx`).

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

## Incomplete Download Recovery (`.downloading` marker)

Purpose:

- Detect and recover from interrupted downloads (Ctrl+C, network failures, crashes).
- Without this, a partial `.lha` file left on disk would be treated as a valid archive on the
  next run — extraction would fail, and the file would never be re-downloaded.

How it works:

1. Before a download begins, an empty marker file is created alongside the archive:
   `GameFiles/<pack>/<letter>/<archive>.downloading`
2. If the download completes successfully, the marker is deleted immediately.
3. If the download is interrupted (Ctrl+C, timeout, socket error), the marker and partial
   archive both remain on disk.
4. On the next run, before the normal "file already exists" skip check, the marker is detected.
   The partial archive is deleted, the marker is removed, and the file is re-downloaded cleanly.

Non-retryable server errors (400, 401, 403, 404):

- Both the marker and partial archive are deleted immediately after the error.
- This prevents futile re-download attempts on every subsequent run for files that cannot be
  obtained from the server.

Marker file details:

- Location: same directory as the `.lha` archive, named `<archive_filename>.downloading`
- Example: `GameFiles/Games/A/Academy_v1.2.lha.downloading`
- Content: empty (zero bytes) — only the file's existence matters.
- The marker is never written to `.archive_index` or tracked in session reports.

Edge cases:

- If a user manually creates a `.downloading` file next to an archive, it will trigger
  re-download of that archive on the next run.
- If the partial `.lha` does not exist when recovery runs (e.g. user deleted it manually),
  `DeleteFile` on a non-existent file is harmless — the marker is still cleaned up and the
  download proceeds normally.

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
- `PURGEARCHIVES` permanently deletes downloaded archives (`.lha` and `.lzx`) under `GameFiles` recursively.
- `PURGEARCHIVES` does **not** delete extracted game folders.
- Both commands print a brief warning and require explicit `Y` confirmation.

Icon control:

- `NOICONS` disables custom icon lookup and always uses the system default drawer icon.

Counter and integrity control:

- `DISABLECOUNTERS` disables queued pre-count and counter display output.
- `CRCCHECK` enables CRC verification for downloaded archives when DAT CRC metadata is available.

## Drawer Icons for Extracted Folders

After each directory in the extraction hierarchy is created (base, pack, and letter folders), a
Workbench drawer icon is written automatically so every folder appears correctly in Workbench.

### Custom icon lookup

If the folder `PROGDIR:Icons/` exists, whdfetch attempts to match the folder's leaf name
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
`PROGDIR:Icons/` tells whdfetch to replace every such icon after extraction:

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
the same position. whdfetch now clears saved icon positions after each successful copy from
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
- Set `use_custom_icons=false` in `whdfetch.ini` — persists across all runs.

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
- `disable_counters=true|false`
- `crccheck=true|false` (alias: `crc_check=true|false`)

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
- `disable_counters = false`
- `crccheck = false`

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
whdfetch DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES
```

Force extraction even if marker matches:

```text
whdfetch DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES FORCEEXTRACT
```

Force download even if already extracted marker matches:

```text
whdfetch DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES FORCEDOWNLOAD
```

Disable only pre-download marker skip:

```text
whdfetch DOWNLOADBETAGAMES EXTRACTTO=Games: KEEPARCHIVES NODOWNLOADSKIP
```

Extract existing local archives only:

```text
whdfetch DOWNLOADBETAGAMES EXTRACTONLY EXTRACTTO=Games: KEEPARCHIVES
```

## Notes

- Folder names inside archives are not assumed to match archive filename prefixes.
- Marker matching is authoritative for skip decisions.
- If marker file is missing or unreadable, normal download/extract flow continues.

## Known Limitations

### No pre-download "what's new" preview

whdfetch currently has no way to show the user what new or updated archives are
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
