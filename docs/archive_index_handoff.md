# Archive Index Change Handoff

## Purpose

This document is a technical handoff summary for the `.archive_index` feature work.
It is intended for another AI/code agent to continue or audit this change without re-mapping the codebase.

## Status

- Batch 1: complete (index core engine + public API).
- Batch 2: complete (pre-download skip path switched to index-first Tier 2).
- Batch 3: complete (post-extraction index updates + shutdown flush).
- Build verification: `make MEMTRACK=1` succeeded after each batch.

## Why This Feature Was Added

Pre-download skip logic previously used:

1. Tier 1 heuristic (`archive filename -> guessed folder`) and metadata match check.
2. Tier 2 full directory scan (`ExNext()` through all letter subfolders, read each `ArchiveName.txt`).

On Amiga media this Tier 2 scan is expensive and ran frequently because heuristic misses are common.

The new `.archive_index` optimization preserves correctness while reducing repeated folder scan I/O:

- One per-letter index file at `<target>/<pack>/<letter>/.archive_index`.
- In-memory cache for the active target directory.
- Fast lookup by exact archive filename.

## Functional Changes

### 1) New Archive Index API (extract module)

Added in `src/extract/extract.h`:

- `BOOL extract_index_load(const char *target_directory);`
- `BOOL extract_index_lookup(const char *target_directory, const char *archive_filename, char *out_folder_name, size_t out_folder_name_size);`
- `BOOL extract_index_update(const char *target_directory, const char *archive_filename, const char *folder_name);`
- `void extract_index_flush(void);`

### 2) New Index Engine and Cache

Implemented in `src/extract/extract.c`:

- Per-directory cache struct `archive_index_cache`.
- Parse/write helpers for `.archive_index`.
- Migration builder that can seed cache from existing `ArchiveName.txt` markers by scanning subfolders.
- Stale entry removal helper when mapped folder no longer exists.

Key implementation details:

- Index file format: `archive_filename<TAB>folder_name` per line.
- Uses `amiga_malloc`/`amiga_free` for dynamic allocations.
- Hidden-file protection call is guarded with `#ifdef FIBF_HIDDEN` for NDK portability.

### 3) Pre-download Skip Logic (Tier 2) Updated

Changed in `extract_is_archive_already_extracted()` in `src/extract/extract.c`:

- Tier 1 heuristic behavior is unchanged.
- Tier 2 is now index-first:
  - load index/cache for target directory,
  - lookup archive filename,
  - verify mapped folder exists via `Lock`/`UnLock`.
- If mapped folder is missing:
  - remove stale cache entry,
  - return FALSE (download proceeds).
- Fallback scan (`extract_find_archive_by_scanning_metadata`) remains only when index loading fails.

### 4) Post-extraction Index Update Hook

Changed in `extract_process_downloaded_archive()` in `src/extract/extract.c`:

- After successful `extract_write_archive_metadata(...)`, now calls:
  - `extract_index_update(target_directory, archive_filename, game_folder_name)`
- Index update failure is non-fatal and logged as warning.

### 5) Shutdown Flush Hook

Changed in `do_shutdown()` in `src/main.c`:

- Added `extract_index_flush()` before INI cleanup/memory report/log shutdown.
- Ensures dirty cache is persisted on normal exits.

## Files Changed

- `src/extract/extract.h`
- `src/extract/extract.c`
- `src/main.c`
- `docs/ProjectInfo.md` (documentation expanded to include archive index behavior and rationale)

## Behavior and Safety Notes

- `.archive_index` is an optimization layer; `ArchiveName.txt` remains the compatibility marker contract.
- If index data is unavailable/unusable, logic still degrades safely via fallback behavior.
- Stale entries are auto-corrected when index points to removed folders.
- Update/flush failures do not block download/extraction success paths.

## Build / Verification Performed

Command used:

```text
make MEMTRACK=1
```

Observed result:

- Compile and link successful, output `Bin/Amiga/WHDDownloader`.

## Notable Portability Detail

- `FIBF_HIDDEN` may not be defined by all Amiga NDK header sets used with VBCC.
- Code now conditionally applies hidden bit only when available:

```c
#ifdef FIBF_HIDDEN
SetProtection(index_path, FIBF_HIDDEN);
#endif
```

## Suggested Follow-up Checks (for next agent)

1. Runtime sanity on real target flow:
   - first run (no index),
   - second run (index reuse),
   - stale-folder deletion scenario.
2. Confirm `.archive_index` appears in both in-place and `EXTRACTTO` modes.
3. Optionally add a tiny debug log summary for index hit/miss counts per run.
4. If needed, add docs/manual language describing `.archive_index` as internal machine-managed metadata.

## Handoff Summary

Archive skip logic has been migrated from scan-heavy Tier 2 behavior to index-first behavior with cache, stale-entry recovery, and shutdown persistence. Core functionality compiles and is integrated in the download/extract pipeline. The feature is ready for runtime validation and any final documentation polishing.
