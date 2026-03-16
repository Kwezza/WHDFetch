# Plan: Session Report (Minimal "What's New" Output)

## Goal

After a run completes, write a human-readable text file listing every archive that was
**newly downloaded** (and optionally extracted) during this session, clearly distinguishing
between **genuinely new** titles and **updates** (version bumps of games the user already
has). Print a one-liner to the console pointing at the file. No diff engine, no
interactive prompts, no new algorithm.

### Why distinguish new from updated?

Without this distinction the report is misleading. Consider: a DAT update adds 5 new
archive names. The user runs WHDDownloader, the report says "5 new downloads." But 4 of
them are version bumps — `Academy_v1.1.lha` replaced by `Academy_v1.2.lha`, etc. After
extraction, the old game folders are overwritten. No new games appear in the user's
front-end (iGame, TinyLauncher, etc.). The user goes looking for 5 new titles and finds
none.

By classifying each download as NEW or UPDATED, the report tells the truth:
"1 new game, 4 updated." The user knows to look for 1 unfamiliar title and expects the
other 4 to simply be better versions of games they already have.

## Rationale

The user already has the information scrolling past during the run — the report just
persists it. This replaces the full DAT-diff plan (`dat_diff_and_report_plan.md`) with
something that delivers the main user-facing value (a persistent record of changes) at
a fraction of the complexity.

The existing `count_new_files_downloaded` / `count_existing_files_skipped` counters
already prove the data is available at the right moment — we just need to record the
filenames alongside the counts.

---

## Design

### New module: `src/report/report.c` + `src/report/report.h`

Keeps the report logic out of `main.c`. Small enough to be a single file pair.

### Data structure

```c
#define REPORT_MAX_FILENAME 256

typedef struct report_entry {
    char archive_name[REPORT_MAX_FILENAME];       /* e.g. "Academy_v1.2.lha"      */
    char old_archive_name[REPORT_MAX_FILENAME];   /* e.g. "Academy_v1.1.lha"      */
    const char *pack_name;                         /* points into whdload_pack_def  */
    BOOL is_update;                                /* TRUE if title matched an      */
                                                   /* existing index entry          */
} report_entry;

typedef struct session_report {
    report_entry *entries;
    int count;
    int capacity;
} session_report;
```

- `pack_name` is a pointer to the pack's `full_text_name_of_pack` string (static
  lifetime, no need to copy).
- `old_archive_name` is populated only when `is_update == TRUE`. Empty string otherwise.
- `is_update` is determined by checking the archive index for any existing entry whose
  title prefix matches the new archive's parsed title (see **Update Detection** below).
- Initial capacity: 64 entries. Grows by doubling. Typical session adds 0–50 files.
- All allocations via `amiga_malloc` / `amiga_free`.

### Public API

```c
/* Call once at startup (before download loops) */
void report_init(void);

/* Call after each successful new download.
 * old_archive_name = NULL if genuinely new, or the old filename if update. */
void report_add(const char *archive_name,
                const char *pack_name,
                const char *old_archive_name);

/* Call at end of run — writes file if count > 0, prints console line */
void report_write(void);

/* Call in do_shutdown() — frees memory */
void report_cleanup(void);
```

### Report file format

Location: `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`

```
WHDDownloader Session Report
============================
Date: 2026-03-16 14:30:22

Games - 1 new, 4 updated

  New:
    BrandNewGame_v1.0.lha

  Updated:
    Academy_v1.2.lha  (was Academy_v1.1.lha)
    Cadaver_v1.1.lha  (was Cadaver_v1.0.lha)
    Elite_v1.3_NTSC.lha  (was Elite_v1.2_NTSC.lha)
    Lemmings_v1.4.lha  (was Lemmings_v1.3.lha)

Demos Beta - 2 new

  New:
    CoolDemo_v1.0_AGA.lha
    NiceIntro_v0.9.lha

Total: 3 new, 4 updated.
```

Grouped by pack, split into New and Updated subsections. Each updated entry shows the
old archive name in parentheses so the user can see exactly what was replaced.

Uses raw archive filenames — clear and unambiguous. Pretty-printed titles via
`game_metadata` can be layered on later if users request it.

### Console output

After the existing per-pack stats block in `main()`:

```
Report saved: updates/updates_2026-03-16_14-30-22.txt (3 new, 4 updated)
```

If nothing was downloaded:

```
No new archives this session.
```

No report file is created when there's nothing to report.

---

## Integration Points

### 1. `report_init()` — early in `main()`

Call after `initialize_log_system()` and before any download loops begin.

### 2. Update detection + `report_add()` — in `execute_archive_download_command()`

At the exact point where `count_new_files_downloaded` is incremented (around line 2013
of `main.c`), detect whether this is a new title or an update, then record it:

```c
files_downloaded = files_downloaded + 1;
WHDLoadPackDefs->count_new_files_downloaded += 1;

/* Detect new vs update: check if index has an entry with the same title */
{
    game_metadata meta = {0};
    char old_archive[256] = {0};
    const char *old_name = NULL;

    extract_game_info_from_filename(downloadWHDFile, &meta);

    if (extract_index_find_by_title(meta.title, old_archive, sizeof(old_archive)))
    {
        old_name = old_archive;
    }

    report_add(downloadWHDFile, WHDLoadPackDefs->full_text_name_of_pack, old_name);
}
```

This works because at this point in the flow:

1. The archive index for the current letter **is already loaded** —
   `extract_is_archive_already_extracted()` was called earlier in the same function,
   which triggers `extract_index_load()` internally.
2. The **old** version is still in the index — extraction of the new archive hasn't
   happened yet (it comes after download completes).

So the index contains exactly what the user had before this session — perfect for
classifying the download.

### 3. `report_write()` — end-of-run summary in `main()`

After the existing stats `for` loop (around line 789), before `do_shutdown()`:

```c
/* existing stats loop ... */

report_write();   /* writes file + console line */
```

### 4. `report_cleanup()` — in `do_shutdown()`

Add before `extract_index_flush()`:

```c
report_cleanup();
extract_index_flush();
```

### 5. Directory creation

Add `PROGDIR:updates` to the directory creation block that already creates
`PROGDIR:logs`, `PROGDIR:temp`, etc. Only created if `report_write()` has entries.
Alternatively, `report_write()` can create it on demand.

---

## New function in `extract.c` / `extract.h`

### `extract_index_find_by_title()`

A single new function (~25 lines) added to the existing archive index module:

```c
BOOL extract_index_find_by_title(const char *title,
                                  char *out_old_archive,
                                  size_t out_size);
```

**Purpose:** Walk the already-loaded index cache and return the first entry whose archive
name starts with `title` followed by `_` or `.` (the separators that always follow a
title in WHDLoad filenames). If found, copies the old archive name to `out_old_archive`
and returns `TRUE`.

**Why this works:** WHDLoad archive filenames follow a strict `Title_vX.Y_flags.lha`
convention. The title is always the leading segment before the first `_v` or `_` or `.`.
Matching on `title` + separator cleanly avoids false positives like `AcademyDeluxe`
matching `Academy` — because `AcademyDeluxe_` does not start with `Academy_` or
`Academy.`.

**Performance:** The index for a single letter directory has 50–200 entries. A linear
scan with a `strncmp` + separator check is trivial — well under 1ms even on a 68000.

**No new module needed.** This is a natural extension of the existing archive index API
in `extract.c` — it queries the same `g_archive_index_cache` that's already loaded.

---

## Makefile Changes

```makefile
SRCS += src/report/report.c
OBJS += build/amiga/report/report.o
```

Add `build/amiga/report` to the `directories` target.

---

## Implementation Notes

### Sorting entries within a pack

The entries arrive in DAT-file order (already alphabetical). If we want to guarantee
sorted output even across multiple download passes, `report_write()` can do a simple
insertion sort on the entries array before writing — it's at most ~50 entries, so
performance is irrelevant.

Alternatively, since `download_roms_from_file()` reads the .txt file top to bottom and
the .txt is already sorted, entries naturally arrive in order. Only if multiple packs
interleave (they don't — packs are processed sequentially) would reordering matter.
**Entries will already be in order. No sort needed.**

### Extraction status in the report

The report records downloads, not extractions. If a downloaded archive fails extraction,
it still appears in the report (it was downloaded). This matches user expectation: "what
did the tool fetch for me this time?"

If we later want to distinguish "downloaded + extracted" from "downloaded but extraction
failed", add a `BOOL extracted` field to `report_entry` and set it after
`extract_process_downloaded_archive()` returns `EXTRACT_RESULT_OK`. Not needed for v1.

### Extract-only mode (`EXTRACTONLY`)

In extract-only mode, nothing is downloaded, so `report_add()` is never called and no
report is generated. This is correct — extract-only is re-processing existing files,
not fetching new ones.

If we later want to report re-extractions too, add a separate `report_add_extraction()`
call in the extract-only path. Not needed for v1.

### Memory budget

64-entry initial capacity = `64 * sizeof(report_entry)` = `64 * 516` ≈ 32 KB
(each entry now has two 256-byte filename fields + pointer + BOOL).
Doubling to 128 = 64 KB. Well within budget. Worst case (first run of Games pack,
~4000 entries) ≈ 2 MB — acceptable since it's a one-time event and frees at shutdown.

The `game_metadata` struct used for title parsing is a stack local (~490 bytes) created
once per download in the detection block — no heap allocation needed.

---

---

## Edge Cases in Update Detection

The title-based matching approach handles the common cases cleanly but has known
ambiguities in a few scenarios. These are documented here with a rationale for why they
are accepted in v1 rather than solved.

### 1. AGA/OCS/CD32 variants — different platform, same title

**Scenario:** `Academy_v1.2_AGA.lha` is downloaded. The index already contains
`Academy_v1.1.lha` (the OCS version). Title parsing yields "Academy" for both.
The function matches, so the AGA version is classified as UPDATE.

**Reality:** These are arguably different products — they extract to different folders
(`Academy` vs `AcademyAGA`) and the user might consider the AGA version a new addition,
not a replacement.

**Why accepted:** In practice the user already *has* an "Academy" and the new one is a
variant of the same game, not a brand new title to discover. Calling it an update is
closer to correct than calling it new — the user won't be searching their game list for
an unfamiliar name. The report still shows both filenames (`was Academy_v1.1.lha`), so
the user can see it's a platform variant.

**Fix if needed later:** Compare `title` + `machineType` + `videoFormat` as a composite
identity key instead of title alone. This would make AGA-of-OCS count as NEW. Requires
parsing metadata for both old and new filenames, roughly doubling the detection logic.
Deferred because the current behaviour is reasonable and the edge case is infrequent.

### 2. Unversioned archives

**Scenario:** `Transhuman.lha` is in the index (no version string). A new DAT adds
`Transhuman_v1.1.lha`. Title parsing gives "Transhuman" for both. The separator check
matches `Transhuman.` in the old name. Classified as UPDATE.

**Reality:** This is correct — same game, now with a version number. The extraction will
overwrite the same folder.

**No fix needed.** This case works as intended.

### 3. Title substrings — `Academy` vs `AcademyDeluxe`

**Scenario:** `AcademyDeluxe_v1.0.lha` is downloaded. The index contains
`Academy_v1.1.lha`. Could "Academy" falsely match?

**Reality:** No. `extract_game_info_from_filename()` parses the new file's title as
"AcademyDeluxe", not "Academy". The function then searches the index for entries
starting with `AcademyDeluxe_` or `AcademyDeluxe.` — which won't match `Academy_v1.1.lha`.
The separator check (`_` or `.` must immediately follow the title) prevents substring
false positives.

**No fix needed.** This case is handled correctly by design.

### 4. First run — empty index

**Scenario:** The user runs WHDDownloader for the first time. The archive index for each
letter is empty (no `.archive_index` files exist yet).

**Reality:** `extract_index_find_by_title()` finds nothing for every download.
Everything is classified as NEW. The report shows all entries under "New:" with no
"Updated:" section.

**No fix needed.** This is the correct and expected behaviour.

### 5. Multiple old entries with the same title

**Scenario:** The index contains both `Academy_v1.0.lha` and `Academy_v1.1.lha` (the
user ran an older version of the tool that didn't clean up stale entries, or both were
legitimately present for some reason). `Academy_v1.2.lha` is downloaded.

**Reality:** `extract_index_find_by_title()` does a linear scan and returns the **first**
match. The "was" field in the report shows whichever entry comes first in the index.
This is slightly imprecise — ideally it would show the most recent version.

**Why accepted:** This scenario is rare (the index is typically pruned on flush, and
version bumps delete the old `.lha`). The report's purpose is to tell the user "this is
an update, not a new game" — which old version it replaced is secondary information.
Showing any old version name achieves that goal.

**Fix if needed later:** Sort matches by version string and return the highest. Adds
version-comparison logic for minimal gain. Deferred.

---

## What This Deliberately Doesn't Do

| Feature | Why not |
|---------|---------|
| DAT diff engine | Complexity for a perf gain the archive index already mostly provides |
| Interactive "install these?" prompt | Changes tool from batch to interactive; filters already handle exclusion |
| "Never install" skip list | Persistent state to maintain; rabbit hole of versioning edge cases |
| Removed-entries detection | Requires diffing; stale index entries are harmless |
| Pretty-printed titles via `game_metadata` | Can add later; raw filenames are unambiguous and simpler |
| Filter transparency ("3 skipped due to AGA") | Filters happen during DAT→txt conversion, before downloads; would need a separate counting pass |
| Composite identity key (title+machine+video) | Marginal improvement over title-only; doubles detection logic for an infrequent edge case |

Any of these can be layered on later if there's demand. The report module's `report_add()`
pattern makes it easy to extend without touching `main.c` again.

---

## Test Plan

1. `make CONSOLE=1` — clean compile, no warnings
2. **Normal run with new files**: verify report file created in `PROGDIR:updates/`,
   entries grouped by pack and split into New/Updated subsections, console line printed
3. **Run with nothing new**: verify no report file created, "No new archives" message
4. **Multi-pack run**: verify entries grouped correctly under each pack header
5. **Cancelled run (Ctrl-C)**: verify `report_cleanup()` frees memory via `do_shutdown()`
   (no report file written for partial runs, or optionally write what was completed)
6. `make MEMTRACK=1` — verify no leaks from report module
7. **First-run (everything is new)**: verify report handles large entry count (~4000),
   all classified as NEW (no index entries exist yet)
8. **Version bump test**: manually place an older archive entry in `.archive_index`
   (e.g. `Academy_v1.1.lha`), then download `Academy_v1.2.lha` — verify it appears
   under Updated with `(was Academy_v1.1.lha)`
9. **Substring safety test**: ensure `AcademyDeluxe_v1.0.lha` is not falsely matched
   when `Academy_v1.1.lha` exists in the index
10. **AGA variant test**: download an AGA archive when OCS is in the index — verify
    it's classified as UPDATE (documented known behaviour, see edge case #1)

---

## Estimated Size

- `report.h`: ~35 lines (struct defs + API)
- `report.c`: ~140 lines (init, add with new/update flag, write with split sections, cleanup)
- `extract.c` addition: ~25 lines (`extract_index_find_by_title`)
- `extract.h` addition: ~3 lines (function prototype)
- `main.c` changes: ~15 lines (4 lifecycle calls + update detection block in download)
- `Makefile`: ~3 lines

Total: ~220 lines of new code.

---

## Implementation Progress (2026-03-16)

This section tracks what has been implemented in code so future contributors can
quickly map the plan to current behaviour. The original plan above is intentionally
left unchanged.

### Completed

1. **New report module created**
   - Added `src/report/report.h` and `src/report/report.c`.
   - Implemented:
     - `report_init()`
     - `report_add()`
     - `report_write()`
     - `report_cleanup()`

2. **Archive index title lookup added**
   - Added `extract_index_find_by_title()` to:
     - `src/extract/extract.h`
     - `src/extract/extract.c`
   - Uses title prefix + separator (`_` or `.`) matching against loaded cache entries.

3. **Main flow integration completed**
   - `report_init()` is called during startup after log initialization.
   - `report_write()` is called after per-pack stats summary.
   - `report_cleanup()` is called in `do_shutdown()`.
   - On successful HTTP archive download, code classifies entry as NEW/UPDATED and calls
     `report_add()`.

4. **Build integration completed**
   - `Makefile` now includes `src/report/report.c` and
     `build/amiga/report/report.o`, plus report build directory creation/rule.

5. **Follow-up behaviour patch (post-plan clarification)**
   - `FORCEDOWNLOAD` now bypasses the local `GameFiles/...` short-circuit.
   - Added local-cache reporting path:
     - New API: `report_add_local_cache_reuse()`
     - Report output includes **"Local cache reuse (no download)"** section and totals.
   - Added explicit runtime log line when local archive cache is reused instead of HTTP
     download.

### Behaviour Notes (Current)

- If archive file exists in `GameFiles/...` **and** `FORCEDOWNLOAD` is not set,
  downloader may reuse local archive and optionally re-extract it; this is now reported
  as local-cache reuse.
- If `FORCEDOWNLOAD` is set, this local-file short-circuit is bypassed and downloader
  proceeds to HTTP fetch path.
- Session report currently tracks:
  - NEW downloads
  - UPDATED downloads
  - Local-cache reuse (no HTTP download)

### Validation Status

- `make clean ; make` passes after implementation and follow-up patch.
- Existing historical warnings in `main.c` prototype signatures remain unrelated to this
  feature set.
