# WHDDownloader Session Instructions

This document summarizes the features implemented during this chat session.

## Scope Added In This Session

- Integrated archive extraction into the main download flow.
- Added extraction configuration via CLI and INI.
- Added startup validation for extraction requirements.
- Added `ArchiveName.txt` metadata writing after successful extraction.
- Added skip-if-already-extracted logic before extraction.
- Added pre-download skip logic using existing extraction markers to avoid slow re-downloads.
- Added force/override flags for both download and extraction skip systems.
- Updated help text, sample INI, and README documentation.

## New Runtime Behavior

1. Download completes successfully.
2. If extraction is enabled, archive extraction is attempted.
3. If marker-based extraction skip is enabled and marker matches, extraction is skipped.
4. If pre-download marker skip is enabled and marker matches, download is skipped before `wget` runs.

## Marker File Contract

Path format:

- `<target>/<pack>/<letter>/<game_folder>/ArchiveName.txt`

File format:

- Line 1: category text (for example `Games` or `Games (Beta & Unofficial)`).
- Line 2: exact archive filename (for example `AbuSimbelProfanation_v1.0_AGA.lha`).

Matching rule:

- Exact case-sensitive match against line 2.

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

Pre-download skip (before `wget`):

- Uses same resolved target directory logic.
- Tries heuristic folder marker first.
- Falls back to scanning child folders and reading `ArchiveName.txt` markers.
- Skips download on exact marker match unless `FORCEDOWNLOAD` is set or `NODOWNLOADSKIP` is used.

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
