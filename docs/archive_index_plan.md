# Plan: Archive Index for Fast Extraction Lookups

## TL;DR

Create a per-letter index file (`.archive_index`) in each extraction target directory
(e.g., `Games/A/.archive_index`) that maps archive filenames to their actual extracted
folder names. This replaces the expensive directory-scanning Tier 2 fallback with a
single file read and in-memory lookup.

---

## Problem

When `extract_is_archive_already_extracted()` checks whether an archive has already been
extracted, it follows a two-tier approach:

1. **Tier 1 (heuristic):** Derive folder name from archive filename by chopping at the
   first `_` (e.g., `Academy_v1.2_2943.lha` -> `Academy`). Then read
   `Academy/ArchiveName.txt` and compare line 2 to the archive filename. Fast — one file
   open — but only works when the heuristic matches the real folder name.

2. **Tier 2 (full scan):** If Tier 1 fails, lock the entire letter directory, iterate
   every subdirectory with `ExNext()`, and for each one open + read its
   `ArchiveName.txt`. This is 100% accurate but extremely expensive on classic Amiga
   hardware: ~100 subdirectories in `Games/A` = ~100 `Lock()`/`Open()`/`Read()`/`Close()`
   cycles on a mechanical HDD.

The heuristic fails roughly half the time (e.g., `AcademyAGA_v1.0.lha` yields
`AcademyAGA` but the real folder is `Academy`), so Tier 2 runs for **thousands** of
entries on a full-pack scan.

---

## Solution: `.archive_index`

A lightweight text file in each letter directory that stores every
archive-to-folder mapping:

```
AbuSimbelProfanation_v1.0_AGA.lha	AbuSimbelProfanation
Academy_v1.2_2943.lha	Academy
AcademyAGA_v1.0.lha	Academy
Alien3_v1.1_0347.lha	Alien3
```

**Format**: one entry per line, two TAB-separated columns:

| Column | Content |
|--------|---------|
| 1 | Exact archive filename (matches `ArchiveName.txt` line 2) |
| 2 | Actual extracted folder name (leaf name only, no path) |

The file is machine-managed — users should never need to edit it.

---

## File Location

```
<target>/<pack>/<letter>/.archive_index
```

Examples:
- `GameFiles/Games/A/.archive_index` (in-place extraction)
- `Games:/Games/A/.archive_index` (EXTRACTTO mode)

Uses `extract_build_target_directory()` to resolve the correct base, so EXTRACTTO is
handled transparently — same as `ArchiveName.txt`.

---

## Design Details

### Write Path (after extraction)

Insert point: `extract_process_downloaded_archive()`, immediately after the existing
`extract_write_archive_metadata()` call succeeds.

At this point, both values are known with 100% accuracy:
- `archive_filename` — the input parameter (e.g., `AcademyAGA_v1.0.lha`)
- `game_folder_name` — resolved by `extract_get_top_level_directory_from_lha()` from
  inside the .lha (e.g., `Academy`)

Operation: **update-or-append**.

1. Read the existing `.archive_index` into memory (if it exists).
2. Scan for a line starting with the exact `archive_filename\t`.
3. If found: replace just the folder name portion (handles version updates where the
   archive name stays the same but the folder changes — unlikely but defensive).
4. If not found: append a new line.
5. Write the file back.

Memory cost: the index for a single letter is ~100 entries at ~60 bytes each = ~6 KB.
Well within heap budget.

### Read Path (pre-download skip check)

Replace the Tier 2 fallback in `extract_is_archive_already_extracted()`.

Current flow:
```
Tier 1: heuristic folder name → read ArchiveName.txt → match?
  YES → return TRUE (skip download)
  NO  → Tier 2: scan ALL subdirectories → read each ArchiveName.txt → match?
          YES → return TRUE
          NO  → return FALSE (proceed to download)
```

New flow:
```
Tier 1: heuristic folder name → read ArchiveName.txt → match?
  YES → return TRUE (skip download)
  NO  → Tier 2: load .archive_index → find archive_filename → got folder_name?
          YES → verify folder exists (Lock/UnLock) → exists?
                  YES → return TRUE (skip download)
                  NO  → stale entry, remove from index → return FALSE
          NO  → return FALSE (proceed to download)
```

### Index Caching Across Entries

DAT file entries arrive alphabetically: all `A*` entries, then all `B*` entries, etc.
This means for a typical Games pack:
- ~100 entries for letter A all query `Games/A/.archive_index`
- Then ~80 entries for letter B all query `Games/B/.archive_index`

**Optimisation**: cache the loaded index in a static or session-scoped structure:

```c
typedef struct {
    char   letter_directory[EXTRACT_MAX_PATH]; /* key: which dir is cached */
    char **archive_names;                      /* column 1 values */
    char **folder_names;                       /* column 2 values */
    int    entry_count;
    BOOL   dirty;                              /* needs write-back? */
} archive_index_cache;
```

- On first query for a letter: load the `.archive_index`, populate the cache.
- On subsequent queries for the same letter: in-memory lookup only (zero I/O).
- On letter change: if dirty, write back the cache, then free and reload.
- On shutdown: flush any remaining dirty cache.

This means the **entire letter A** costs exactly **one file read** regardless of how
many entries are in the DAT, instead of potentially thousands of `ExNext()` + file reads.

### Migration (First Run / No Index Exists)

When `extract_is_archive_already_extracted()` reaches Tier 2 and no `.archive_index`
exists for the current letter directory:

1. Fall back to the existing `extract_find_archive_by_scanning_metadata()` — full scan.
2. As each match is found during the scan, collect the archive→folder mapping.
3. After the full scan completes, write all collected mappings to `.archive_index`.
4. Populate the in-memory cache so subsequent queries for this letter hit memory.

This means:
- **Existing installs**: the expensive scan happens exactly **once per letter**, then
  the index takes over for all future runs.
- **Fresh installs**: no index needed yet because no folders exist to scan. The index
  will be built naturally as extraction happens.

Alternatively, a dedicated one-time migration pass at startup could build all indexes
up front. But lazy per-letter migration is simpler, requires no special startup phase,
and spreads the cost across the first run.

### Hidden File (AmigaOS)

After writing `.archive_index`, call:
```c
SetProtection(index_path, FIBF_HIDDEN);
```
This sets the `h` protection bit. Workbench will not display the file. The `.` prefix
is just a filename convention — the protection bit is what actually hides it on AmigaOS.

---

## API Surface

All new functions live in `src/extract/extract.c` (static where possible) with only
the necessary interface exposed in `extract.h`.

### New Public Functions

```c
/* Load or rebuild the archive index for a letter directory.
   Returns TRUE if the index was loaded/built; FALSE on error.
   Caches the result — subsequent calls for the same directory are free. */
BOOL extract_index_load(const char *target_directory);

/* Look up an archive filename in the currently cached index.
   If found, copies the folder name to out_folder_name and returns TRUE.
   Returns FALSE if not found or no index is loaded for this directory. */
BOOL extract_index_lookup(const char *target_directory,
                          const char *archive_filename,
                          char *out_folder_name,
                          size_t out_folder_name_size);

/* Add or update an entry in the index (marks cache dirty).
   The cache will be flushed on letter change or shutdown. */
BOOL extract_index_update(const char *target_directory,
                          const char *archive_filename,
                          const char *folder_name);

/* Flush any dirty cache to disk and free memory.
   Called on shutdown and letter boundary transitions. */
void extract_index_flush(void);
```

### New Static Functions (internal to extract.c)

```c
/* Parse .archive_index file into the cache struct. */
static BOOL index_parse_file(const char *index_path, archive_index_cache *cache);

/* Write cache contents to .archive_index file. */
static BOOL index_write_file(const char *index_path, const archive_index_cache *cache);

/* Build index by scanning all subdirectories (migration path). */
static BOOL index_build_from_scan(const char *target_directory, archive_index_cache *cache);

/* Remove a stale entry from the cache by archive name. */
static void index_remove_entry(archive_index_cache *cache, int entry_index);
```

---

## Integration Points

### 1. `extract_is_archive_already_extracted()` — Replace Tier 2

Current code (lines ~799-803 of extract.c):
```c
return extract_find_archive_by_scanning_metadata(target_directory,
                                                 archive_filename,
                                                 out_match_folder_path,
                                                 out_match_folder_path_size);
```

Replace with:
```c
/* Tier 2: index-based lookup */
if (extract_index_load(target_directory))
{
    char indexed_folder[EXTRACT_MAX_NAME] = {0};
    if (extract_index_lookup(target_directory, archive_filename,
                             indexed_folder, sizeof(indexed_folder)))
    {
        /* Verify the folder still exists on disk */
        char verify_path[EXTRACT_MAX_PATH] = {0};
        snprintf(verify_path, sizeof(verify_path), "%s/%s",
                 target_directory, indexed_folder);
        sanitize_amiga_file_path(verify_path);

        BPTR lock = Lock(verify_path, ACCESS_READ);
        if (lock != 0)
        {
            UnLock(lock);
            if (out_match_folder_path != NULL && out_match_folder_path_size > 0)
            {
                strncpy(out_match_folder_path, verify_path,
                        out_match_folder_path_size - 1);
                out_match_folder_path[out_match_folder_path_size - 1] = '\0';
            }
            return TRUE;
        }
        /* Folder gone — stale entry. Remove it, fall through to FALSE. */
        /* (Stale removal handled inside extract_index_lookup or caller) */
    }
}
return FALSE;
```

### 2. `extract_process_downloaded_archive()` — Write Index After Extraction

After the existing `extract_write_archive_metadata()` call (line ~896):
```c
extract_index_update(target_directory, archive_filename, game_folder_name);
```

No extra I/O — just marks the in-memory cache dirty. The actual write happens when
the letter changes or at shutdown.

### 3. Shutdown / Letter Boundary — Flush

Add `extract_index_flush()` to `do_shutdown()` in `main.c`.

The DAT-driven download loop naturally processes entries alphabetically. When the first
letter of the current entry differs from the cached letter, call
`extract_index_flush()` before loading the new letter's index.

### 4. `extract_find_archive_by_scanning_metadata()` — Keep but Repurpose

This function is NOT deleted. It becomes the migration path: called only when
`.archive_index` doesn't exist yet, and its results are used to **build** the index.
After the DAT diff feature lands, it may also serve as a fallback if the index file
is corrupted.

---

## Memory Budget

| Item | Size |
|------|------|
| `archive_index_cache` struct | ~1,060 bytes (paths + pointers + metadata) |
| Pointer arrays (100 entries) | 800 bytes (2 x 100 x 4-byte pointers) |
| String storage (100 entries at ~60 bytes avg) | ~6,000 bytes |
| **Total per letter** | **~8 KB** |

Only one letter is cached at a time, so the total overhead is always ~8 KB regardless
of how many letters or packs exist. This is well within the 68000 stack/heap budget.

---

## Performance Comparison

### Per-entry cost in `extract_is_archive_already_extracted()`

| Scenario | Current (Tier 2 scan) | With Index |
|----------|----------------------|------------|
| Heuristic hits | 1 file read | 1 file read (unchanged) |
| Heuristic misses, found | ~N ExNext + ~N file reads | 0 I/O (memory lookup) |
| Heuristic misses, not found | ~N ExNext + ~N file reads | 0 I/O (memory lookup, not found) |
| First letter query, no index | - | 1 file read (load index) |
| First letter query, no index file | - | ~N ExNext + ~N file reads (migration, once) |

Where N = number of game folders in that letter directory (~100 for Games/A).

### Full Games pack scan (~4000 entries)

| Metric | Current | With Index |
|--------|---------|------------|
| Tier 2 invocations | ~2000 (50% heuristic miss rate) | ~2000 (same miss rate) |
| Total disk operations (Tier 2) | ~2000 * ~80 avg = ~160,000 | 26 file reads (one per letter) |
| Time estimate (mechanical HDD, 10ms/seek) | ~26 minutes of seeking | < 1 second |

### With DAT diff (~119 entries)

| Metric | Current would be | With Index |
|--------|-----------------|------------|
| Tier 2 invocations | ~60 | ~60 |
| Total disk operations | ~60 * ~80 = ~4,800 | ~26 file reads (worst case, all letters) |
| Time estimate | ~48 seconds | < 1 second |

---

## Edge Cases

### Version Updates

When a pack updates `Academy_v1.1_0347.lha` to `Academy_v1.2_2943.lha`:
- The old archive name exists in the index mapping to folder `Academy`.
- The new archive name does NOT exist in the index.
- Tier 1 heuristic: `Academy` from `Academy_v1.2_2943.lha` → reads
  `Academy/ArchiveName.txt` → finds `Academy_v1.1_0347.lha` → **mismatch** → falls
  through.
- Index lookup: `Academy_v1.2_2943.lha` → not found → returns FALSE → download +
  extract proceeds.
- After extraction: `extract_index_update()` adds the new mapping. The old mapping
  (`Academy_v1.1_0347.lha → Academy`) becomes stale but harmless — it will never be
  looked up again because the old filename no longer exists in the DAT.

Future cleanup: when the DAT diff detects REMOVED entries, their index entries could
be removed. But this is not required for correctness.

### User Manually Deletes a Game Folder

- Index says `Academy_v1.2_2943.lha → Academy`.
- Tier 1 or Tier 2 lookup finds the mapping.
- Verification `Lock()` on `Games/A/Academy` fails — folder is gone.
- Returns FALSE → download + extract proceeds normally.
- After extraction: index entry is updated (same mapping re-established).

### User Manually Adds a Game Folder

- Not reflected in the index, but the user wouldn't be looking it up via DAT entry.
- No impact — WHDDownloader only queries archives it knows about from the DAT file.

### Corrupted or Truncated Index File

- `index_parse_file()` should handle partial reads gracefully: skip malformed lines
  (missing TAB, empty columns) and log a warning.
- Even if no valid entries parse, the system still works: Tier 1 heuristic covers ~50%,
  and entries not in the index will just download (safe default).
- The index self-heals: each extraction writes `extract_index_update()`, so after one
  full run the index is repopulated.

### Multiple Packs Sharing EXTRACTTO

If Games and Games Beta both extract to `Games:`, they use different pack subdirectories
(`Games:/Games/A/` vs `Games:/Games Beta/A/`), so each has its own `.archive_index`.
No collision.

### Index File Grows Stale Over Time

Old entries from archives that no longer exist in any DAT stay in the index. This is
harmless — they are never queried. The file grows slightly larger over many pack
updates but at ~60 bytes per entry, even 200 stale entries is only 12 KB. Not worth
the complexity of periodic pruning.

---

## Implementation Steps

### Step 1: Define the Index Data Structures

Add to `extract.c`:
```c
#define ARCHIVE_INDEX_FILENAME ".archive_index"
#define ARCHIVE_INDEX_MAX_ENTRIES 512

typedef struct {
    char letter_directory[EXTRACT_MAX_PATH];
    char *archive_names[ARCHIVE_INDEX_MAX_ENTRIES];
    char *folder_names[ARCHIVE_INDEX_MAX_ENTRIES];
    int  entry_count;
    BOOL dirty;
    BOOL loaded;
} archive_index_cache;

static archive_index_cache g_index_cache = {0};
```

Using fixed-size pointer arrays avoids the need for dynamic resizing. 512 entries
exceeds the maximum game count for any single letter directory. Each pointer targets
an `amiga_malloc()`-allocated string.

### Step 2: Implement `index_parse_file()`

- Open `.archive_index` with `fopen()`.
- Read line by line with `fgets()`.
- For each line: find the TAB separator, split into archive name and folder name.
- `amiga_strdup()` both strings into the cache arrays.
- Skip blank lines and lines without a TAB (malformed).
- Return TRUE if at least zero entries parsed (empty file is valid).

### Step 3: Implement `index_write_file()`

- Open `.archive_index` with `fopen(path, "w")`.
- Write each cache entry as `archive_name\tfolder_name\n`.
- Call `SetProtection()` with `FIBF_HIDDEN` after closing.
- Clear the `dirty` flag.

### Step 4: Implement `index_build_from_scan()`

- Reuse the core loop from `extract_find_archive_by_scanning_metadata()`:
  `Lock()` directory, `Examine()`, `ExNext()` loop.
- For each subdirectory: read `ArchiveName.txt` line 2 (archive filename).
  The folder name is `fib->fib_FileName`.
- Store each pair in the cache.
- After scan: write the index file.
- This is the one-time migration path.

### Step 5: Implement Public API

- `extract_index_load()`: Check if `g_index_cache.letter_directory` matches. If so,
  return TRUE (already cached). Otherwise: flush old cache if dirty, then try
  `index_parse_file()`. If no file exists, call `index_build_from_scan()`.
- `extract_index_lookup()`: Linear scan of `g_index_cache.archive_names[]` for a
  `strcmp()` match. Return the corresponding `folder_names[]` entry. O(N) where
  N ≈ 100 — trivial on any CPU.
- `extract_index_update()`: Scan for existing entry by archive name. If found, update
  the folder name (free old string, strdup new). If not found, append. Set `dirty = TRUE`.
- `extract_index_flush()`: If dirty, call `index_write_file()`. Free all `amiga_malloc()`
  strings. Reset cache state.

### Step 6: Wire Into `extract_is_archive_already_extracted()`

Replace the `extract_find_archive_by_scanning_metadata()` call with the index lookup
as shown in Integration Point 1 above.

### Step 7: Wire Into `extract_process_downloaded_archive()`

Add `extract_index_update()` call after `extract_write_archive_metadata()` succeeds.

### Step 8: Wire Into `main.c` Shutdown

Add `extract_index_flush()` to `do_shutdown()`.

### Step 9: Update `extract.h`

Add the public function declarations.

### Step 10: Test

1. **Clean build**: `make CONSOLE=1 MEMTRACK=1` — no compile errors.
2. **Fresh install test**: No `.archive_index` files exist. First run extracts games,
   indexes are built naturally via `extract_index_update()` during extraction.
3. **Second run test**: `.archive_index` files exist. Tier 2 uses index lookups
   instead of directory scans. Verify skip messages appear and no unnecessary
   downloads occur.
4. **Migration test**: Delete `.archive_index` but leave extracted folders. Run
   WHDDownloader — `index_build_from_scan()` should fire once per letter, rebuild
   the index, and subsequent lookups use the index.
5. **Stale entry test**: Delete a game folder manually. Run WHDDownloader — the
   verification `Lock()` should fail, entry should be treated as not-found, and
   download + extract should proceed.
6. **Version update test**: Simulate a version bump (e.g., `Academy_v1.1` to
   `Academy_v1.2`). The new archive name is not in the index, so download proceeds.
   After extraction, the new mapping is added.
7. **Memory leak test**: `make MEMTRACK=1` — verify all `amiga_strdup()` allocations
   are freed by `extract_index_flush()`.
8. **EXTRACTTO test**: Run with `EXTRACTTO=Games:` — verify `.archive_index` is
   created in the EXTRACTTO target, not the download directory.

---

## Relationship to DAT Diff Feature

This feature should be implemented **before** the DAT diff:

1. The index is a standalone optimisation that benefits the current full-scan mode
   immediately. No dependency on the diff module.
2. The DAT diff will reduce entries to ~119 per update, but those entries still call
   `extract_is_archive_already_extracted()`. The index makes each of those calls
   instant instead of triggering a scan.
3. The index API (`extract_index_lookup`, `extract_index_update`) is also useful for
   the future "What's New" report — it tells you the actual folder name for any
   archive without hitting disk.

After both features are in place, the lookup cost per DAT entry drops from potentially
hundreds of disk operations to zero (memory hit from cached index).

---

## Files Changed

| File | Change |
|------|--------|
| `src/extract/extract.c` | Add index data structures, static helpers, public API functions. Modify `extract_is_archive_already_extracted()` and `extract_process_downloaded_archive()`. |
| `src/extract/extract.h` | Add public declarations for `extract_index_load()`, `extract_index_lookup()`, `extract_index_update()`, `extract_index_flush()`. |
| `src/main.c` | Add `extract_index_flush()` to `do_shutdown()`. |

No Makefile changes needed — all new code is in existing source files.
