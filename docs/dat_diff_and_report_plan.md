# Plan: DAT Diff + "What's New" Report

## TL;DR
When Retroplay releases an updated pack, **diff the old and new .txt DAT files** to identify only the added/removed entries. Download/extract only the delta instead of checking all ~4000 entries against disk. Generate a human-readable report of what was added. Notify the user when filters eliminate new entries to avoid confusion.

## Prerequisite: Archive Index (COMPLETED)

The `.archive_index` feature has been implemented (see `docs/archive_index_handoff.md`).
This replaced the expensive Tier 2 directory scan in `extract_is_archive_already_extracted()`
with a per-letter cached index lookup. Key points relevant to this plan:

- **Tier 2 is now index-first**: `extract_index_load()` / `extract_index_lookup()` provide
  O(1) per-entry lookups from an in-memory cache, with one file read per letter directory.
- **`extract_index_update()`** is called after every successful extraction, keeping the
  index current without extra I/O.
- **`extract_index_flush()`** is already wired into `do_shutdown()` in `main.c`.
- **`extract_find_archive_by_scanning_metadata()`** is retained as a fallback only when
  index loading fails.
- The index also provides archive→folder name resolution that the report module can use
  (via `extract_index_lookup()`) without hitting disk.

## Problem

Currently `download_roms_from_file()` reads ALL entries (~4000 for Games) and calls
`execute_wget_download_command()` for each. Each call performs:
1. A filesystem existence check for the `.lha` archive file.
2. An `extract_is_archive_already_extracted()` call (now fast thanks to the archive index).

While the archive index eliminated the expensive Tier 2 directory scan, the sheer volume
of ~4000 iterations still involves ~4000 `does_file_or_folder_exist()` checks and ~4000
index lookups per run — plus console output overhead — even when nothing is new. On classic
Amiga hardware with a mechanical HDD, this is still noticeably slow.

## Key Discovery
Old .txt DAT files **already survive across runs** (only ZIPs and XMLs are cleaned up). So when `Games(2026-03-07).txt` is created, `Games(2025-09-20).txt` still exists — the diff is free.

## Design Overview

### Flow change (update scenario):
```
OLD: Read ALL 4000 lines → check each against disk → download missing
NEW: Diff old.txt vs new.txt → get ~20 added lines → download only those
```

### Flow change (first run / no old .txt):
```
Same as current: read all lines, download all missing (no diff possible)
```

### Filter transparency:
When a new DAT has N new entries but filters remove some:
```
Games update: 15 new archives found, 3 filtered (AGA), 12 downloaded.
```

---

## Steps

### Phase 1: DAT Diff Module (`src/dat_diff.h` / `src/dat_diff.c`)

1. **Define diff result struct**
   - `dat_diff_entry`: filename string, status (ADDED / REMOVED)
   - `dat_diff_result`: array of entries, counts (added, removed, total)
   - API: `dat_diff_compare()`, `dat_diff_cleanup()`

2. **Implement `dat_diff_compare(old_path, new_path)`**
   - Both files are sorted alphabetically (they already are — XML `<rom>` tags come from sorted DAT)
   - Use merge-join algorithm: two file pointers walking in parallel, O(n+m) single pass
   - On mismatch: classify as ADDED (in new, not old) or REMOVED (in old, not new)
   - Return `dat_diff_result` with the delta
   - Memory: only need 2 line buffers (256 bytes each) — no need to load entire files

3. **Implement `dat_diff_find_previous_dat(filter_prefix)`** — *parallel with step 2*
   - Scan `DIR_DAT_FILES` for .txt files matching filter
   - Find the one with the OLDEST date that is NOT the current file
   - Return its filename (or NULL if no previous exists = first run)
   - Fixes the existing `get_first_matching_fileName()` bug (filesystem-order dependent)

### Phase 2: Integrate Diff into Download Flow

4. **Add diff-aware download path in main.c** — *depends on 1-3*
   - After `process_and_archive_WHDLoadDat_Files()` creates new .txt
   - Before `download_roms_if_file_exists()` runs
   - Call `dat_diff_find_previous_dat()` to locate old .txt
   - If found: call `dat_diff_compare()` → get delta list
   - Pass delta list to a new `download_roms_from_diff()` function
   - If NOT found (first run): fall back to existing `download_roms_from_file()` (full scan)

5. **Implement `download_roms_from_diff()`** — *depends on 4*
   - Similar to `download_roms_from_file()` but iterates the diff ADDED entries only
   - Calls `execute_wget_download_command()` for each (reuses existing download logic)
   - Each call still benefits from the archive index for fast skip checks
   - Much shorter loop: typically 10-50 entries vs 4000

6. **Apply filters to diff and report transparency** — *depends on 4*
   - The new .txt already has filters applied (during XML→txt conversion)
   - But the OLD .txt may have been created with DIFFERENT filters
   - Solution: diff operates on the filtered .txt files as-is
   - Track: "N entries in new DAT not in old DAT" = raw additions
   - Of those, some may already exist on disk (re-downloaded pack with same content)
   - Console output: "Games update: 15 new entries, 12 to download, 3 already on disk"

7. **Handle filter-change scenario** — *parallel with step 6*
   - If user changes filters between runs (e.g., removes SKIPAGA), the diff would show entries that aren't truly "new" from Retroplay's side but are new to the user's filtered set
   - This is actually correct behavior: user changed filters, so more files should appear
   - No special handling needed — the diff naturally captures this

### Phase 3: Report Generation

8. **New module: `src/report.h` / `src/report.c`** — *parallel with Phase 2*
   - `report_entry` struct: archive filename, pack name, parsed `game_metadata`, status (NEW/UPDATED)
   - `report_list` struct: dynamic array, count, capacity
   - API: `report_init()`, `report_add_entry()`, `report_write()`, `report_cleanup()`
   - Uses `amiga_malloc`/`amiga_free`

9. **Record entries during download** — *depends on 5 and 8*
   - In `download_roms_from_diff()` (and `download_roms_from_file()` for first-run): after successful download, call `report_add_entry()` with filename and pack name
   - Parse metadata via existing `extract_game_info_from_filename()` / `game_metadata` struct
   - For folder name resolution in the report, `extract_index_lookup()` can provide the
     actual extracted folder name without disk I/O (populated during extraction)

10. **Write report file** — *depends on 8-9*
    - `report_write()` creates `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`
    - Groups entries by pack, sorts alphabetically
    - Format per entry: `Title vVersion (Machine, Video, Language, Media) [Special]`
    - Per-pack header with counts
    - Only created if there are entries to report

11. **Console notification** — *depends on 10*
    - After existing stats summary in main():
      - If new files: "Report saved to: updates/updates_2026-03-14_14-30-22.txt"
      - If nothing new: "No new or updated archives this run."
      - If new entries were all filtered: "Games pack updated with 5 new entries, but all were excluded by your filters (AGA: 3, NTSC: 2)."

### Phase 4: Cleanup & Wiring

12. **Delete old .txt after successful diff+download** — *depends on 4-5*
    - After the diff-based download completes successfully, delete the old .txt
    - Keeps `temp/Dat files/` clean with only 1 .txt per pack
    - The old .txt has served its purpose (diffing)

13. **Create `PROGDIR:updates/` directory** — add to directory creation block in main()

14. **Update Makefile** — add `src/dat_diff.c`, `src/report.c` to SRCS/OBJS

15. **Wire lifecycle** in main.c:
    - `report_init()` early in main()
    - Diff logic between DAT processing and download phases
    - `report_write()` after download loops
    - `report_cleanup()` in `do_shutdown()` (before `extract_index_flush()` which is
      already present)

---

## Relevant files
- `src/main.c` — `download_roms_from_file()` (add diff-aware alternative), `process_and_archive_WHDLoadDat_Files()` (diff entry point), end-of-run summary, `do_shutdown()` (already has `extract_index_flush()`)
- `src/main.h` — `whdload_pack_def` struct (reference for pack context)
- `src/gamefile_parser.h` — `game_metadata` struct, `extract_game_info_from_filename()` for report metadata
- `src/extract/extract.h` — archive index public API (`extract_index_lookup()` etc.) — available for report folder-name resolution
- `src/extract/extract.c` — archive index implementation (completed, see `docs/archive_index_handoff.md`)
- **NEW** `src/dat_diff.h` / `src/dat_diff.c` — diff module
- **NEW** `src/report.h` / `src/report.c` — report module
- `Makefile` — add new source/object files

---

## Test Data
- **Old DAT**: `Bin/Amiga/temp/old dats/Games(2025-04-13).txt` — 3,852 lines
- **New DAT**: `Bin/Amiga/temp/Dat files/Games(2026-03-07).txt` — 3,971 lines
- **Expected delta**: ~119 ADDED entries (new games, version bumps, language/platform variants, remastered editions)
- **Both files confirmed alphabetically sorted** — merge-join algorithm validated

## Verification
1. `make CONSOLE=1` — clean compile
2. **First-run test** (no old .txt): full scan, same as current behavior, report generated
3. **Update test** (old + new .txt): only ~119 delta entries processed vs 3,971 — verify correct diff output
4. **No-change test** (copy new as old): zero delta, "No new archives" message
5. **Filter test**: run with SKIPAGA, check console mentions filtered entries (e.g., AGA remastered editions)
6. **Filter-change test**: run with SKIPAGA, then without — previously-filtered entries now download
7. **Version bump test**: verify entries like Academy_v1.2_2943 appear as ADDED (old v1.1 is separate entry, not "replaced")
8. Verify old .txt deleted after successful diff-based download
9. `make MEMTRACK=1` — no memory leaks from diff or report modules

---

## Decisions
- **Diff algorithm**: Merge-join on sorted files, O(n+m), constant memory (2 line buffers). Files are already alphabetically sorted.
- **First-run fallback**: No old .txt → use existing full-scan logic unchanged. Archive index makes even the full-scan path fast for skip checks.
- **Filter transparency**: Console explicitly reports when filters eliminate new entries.
- **Old .txt cleanup**: Deleted after successful diff, keeping directory clean.
- **Report location**: `PROGDIR:updates/` with timestamped filenames, preserved across runs.
- **"Updated" entries**: For now all are NEW. UPDATED status reserved for future version-upgrade feature.
- **Archive index integration**: The diff does NOT interact with `.archive_index` directly. The index is consumed indirectly through `extract_is_archive_already_extracted()` and `extract_index_lookup()` which are called from the download/report paths.

## Further Considerations
1. **DAT file sort verification**: The plan assumes .txt files are sorted. If Retroplay ever changes XML ordering, the merge-join would produce wrong results. Mitigation: add a quick is-sorted check at the start of diff; if unsorted, fall back to loading into memory and sorting. Recommend: verify with current files first, add the check only if needed.
2. **Removed entries**: The diff detects archives REMOVED from a pack (in old, not in new). Currently not acted on. Future option: warn user about removed entries, or auto-clean extracted folders. Recommend: log removed entries in the report but don't delete anything. Note: removed entries will leave stale `.archive_index` entries — these are harmless (never queried) and self-correct if the same archive is ever re-added.
3. **Index cache and letter boundaries in diff mode**: `download_roms_from_diff()` will process entries that may span multiple letters. The archive index cache handles letter transitions automatically (flush + reload on directory change). No special handling needed in the diff download loop — it calls the same `execute_wget_download_command()` which triggers the index through `extract_is_archive_already_extracted()`.
