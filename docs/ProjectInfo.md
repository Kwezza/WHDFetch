# whdfetch Project Information

Last updated: 2026-03-25

This document is a hybrid reference for project-level orientation and stable runtime contracts.
Use it to understand how whdfetch is structured and which file/marker/index behaviors are
considered authoritative.

## Audience and Scope

Audience:

- New contributors who need a concise architecture and behavior overview.
- Maintainers who need exact runtime contracts and recovery semantics.

In scope:

- Core architecture and runtime data flow.
- Marker/index contracts that drive skip/extract/download behavior.
- Recovery and fallback behavior that affects correctness or performance.

Out of scope:

- Full CLI argument reference (see `docs/CLI_Reference.md`).
- End-user walkthroughs and usage examples (see `README.md` and `docs/Manual/manual.md`).

## Canonical Document Map

- Project quick start and user setup: `README.md`
- Architecture summary and feature map: `PROJECT_OVERVIEW.md`
- Complete command/flag behavior and precedence: `docs/CLI_Reference.md`
- User manual content: `docs/Manual/manual.md`
- Sample INI configuration: `docs/whdfetch.ini.sample`

## Project Snapshot

`whdfetch` is a CLI tool for AmigaOS that downloads Retroplay WHDLoad packs, parses DAT entries,
fetches matching archives, and optionally extracts them into a managed local structure.

Core pipeline:

1. Download and parse pack index/list data.
2. Build candidate archive list from DAT metadata.
3. Apply filters and skip logic.
4. Download archives (or reuse local archive cache when valid).
5. Extract archives when enabled and allowed.
6. Write/update marker and index state.
7. Emit session report and flush runtime caches on shutdown.

## Runtime Contracts

### `ArchiveName.txt` Marker Contract

Purpose:

- Authoritative extraction marker for skip decisions.

Path format:

- `<target>/<pack>/<letter>/<game_folder>/ArchiveName.txt`

File format:

- Line 1: category text (for example `Games` or `Games (Beta & Unofficial)`).
- Line 2: exact archive filename (for example `AbuSimbelProfanation_v1.0_AGA.lha`).

Match rule:

- Exact case-sensitive match against line 2.

### `.archive_index` Cache Contract

Purpose:

- Fast lookup layer for pre-download/extraction checks when archive and folder names differ.
- Performance optimization to avoid repeated folder scans and marker file reads.

Path format:

- `<target>/<pack>/<letter>/.archive_index`

File format:

- One entry per line, two TAB-separated fields:
  1. Exact archive filename
  2. Extracted folder name

Lookup/update rules:

- Lookup is exact case-sensitive on archive filename.
- Existing archive entry updates the folder field.
- New archive entry appends a new mapping.

Lifecycle:

- Loaded on-demand during skip checks.
- Reused in memory for the current run.
- Updated after successful extraction + marker write.
- Flushed during shutdown.

Fallback and safety behavior:

- If missing/corrupt/unreadable, runtime falls back to scan-based behavior.
- Malformed lines are skipped defensively.
- If mapped folder no longer exists, stale entry is removed and corrected on flush.
- `ArchiveName.txt` remains the compatibility authority; `.archive_index` is an optimization.

### `.downloading` Incomplete Download Marker

Purpose:

- Detect partial archives from interrupted downloads and force clean recovery.

Behavior:

1. Create `<archive>.downloading` before download starts.
2. Remove marker after successful download.
3. If interrupted, marker + partial archive remain.
4. On next run, marker triggers cleanup and re-download.

Error-specific handling:

- For non-retryable HTTP errors (400/401/403/404), marker and partial archive are removed immediately.

## Skip and Extraction Semantics

Extraction skip (post-download):

- Uses resolved target path.
- Checks marker compatibility (`ArchiveName.txt` line 2 exact match).
- Skips extraction unless overridden by `FORCEEXTRACT`.

Pre-download skip (before HTTP download):

- Checks local archive cache reuse path first.
- Uses heuristic marker path first.
- Uses `.archive_index` fast lookup for heuristic misses.
- Falls back to scan-based behavior if index is unavailable.
- Skip decision can be overridden by `NODOWNLOADSKIP`.
- `FORCEDOWNLOAD` bypasses both marker-based skip and local archive cache reuse.

## Extraction and Icon Behavior

Extraction behavior:

- `.lha` extraction uses `c:lha`.
- `.lzx` extraction uses `c:unlzx` when present.
- Missing `c:unlzx` skips `.lzx` extraction with warning; processing continues.

Icon behavior:

- Structural folders can use custom icons from `PROGDIR:Icons/` when enabled.
- Default fallback is system drawer icon (`GetDefDiskObject(WBDRAWER)`).
- Existing icons are not overwritten by structural icon placement.
- Optional `PROGDIR:Icons/WHD folder.info` can replace extracted game folder icons.
- When configured, copied icons are unsnapshotted to clear saved Workbench positions.

Control:

- Disable custom icons with CLI `NOICONS` or INI `use_custom_icons=false`.
- Unsnapshot control is INI-only: `unsnapshot_icons=true|false`.

## Session Report Contract

Purpose:

- Persist a run summary that distinguishes network downloads from local cache/recovery activity.

Location:

- `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`

Categories:

- `New`
- `Updated`
- `Local cache reuse (no download)`
- `Extraction skipped`

Update classification:

- Uses strict identity fields from parsed metadata, then version progression.
- Missing/non-numeric version tokens are treated as `New` for classification safety.

## Startup Validation

When extraction is enabled:

- `c:lha` must exist.
- `extract_path` (if set) must exist.
- `extract_path` (if set) must be writable.

Validation failure aborts startup with an error.

## Integration Points

Primary code anchors:

- `extract_is_archive_already_extracted()`
- `extract_process_downloaded_archive()`
- `do_shutdown()`

Responsibilities:

- Skip logic combines marker, index, and fallback behavior.
- Extraction success path updates marker/index state.
- Shutdown persists dirty index state and performs final cleanup.

## Known Limitations

### No pre-download "what's new" preview

The tool currently cannot report new/updated server content before downloading.
Classification is available only after a run through the session report.

Current workaround:

- Use filter flags to limit candidate content.
- Review generated updates report after each run.

Planned direction:

- Dry-run preview mode that compares server DAT metadata with local index state without modifying local files.

## Documentation Maintenance Rule

To prevent documentation drift:

- Keep CLI option details in `docs/CLI_Reference.md`.
- Keep onboarding and usage walkthroughs in `README.md` and manual docs.
- Keep runtime contracts and architectural behavior in this file.
