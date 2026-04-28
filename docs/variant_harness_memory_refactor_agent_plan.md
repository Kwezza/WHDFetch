# WHDFetch Variant Harness Memory Refactor Plan

## Current Problem

The current variant-selection harness under `tools\variant_harness` is functionally useful, but its current in-memory data model is not suitable for classic Amiga targets.

The harness currently keeps a full parsed candidate object for every archive filename. For the Games DAT, which currently contains around 3,973 entries, this scales very badly.

Recent memory analysis showed:

- `VhCandidate` size: approximately 4,908 bytes
- 150 candidate stress sample peak estimate: approximately 777,600 bytes
- Full Games DAT candidate array estimate: approximately 19.5 MB before CSV tables, allocator overhead, temporary arrays, report buffers, and normal WHDFetch runtime memory (current live list of games here Bin\Amiga\temp\Dat files\Games(2026-04-17).txt)

This is too high for WHDFetch, especially as many real users may run it on expanded Amigas with 8 MB or similar memory. The selector is intended to be an optional feature, but it still needs an Amiga-friendly memory model before it is merged into the main WHDFetch program.

The current logic is broadly correct. The storage model is the issue.

## High-Level Goal

Refactor `tools\variant_harness` so it keeps only compact candidate records in memory, stores strings once, parses detailed tokens only when needed, and produces updated memory reports proving the design is viable before WHDFetch integration.

Target outcome:

- Full Games DAT selector peak under 1 MB
- Preferred target: 600 KB or less if reasonably achievable
- Candidate record under 48 bytes if possible
- No persistent full parsed records for every candidate
- Existing selection behaviour preserved unless tests show a deliberate change is needed
- Current progress, decisions, and memory results recorded back into the existing analysis document

Primary working directory:

```text
tools\variant_harness
```

Primary progress document to update:

```text
docs/variant_harness_data_loading_storage_analysis.md
```

If the document is stored elsewhere in the repository, locate the existing file by name and update that copy.

## Important Constraints

- Keep this work inside the harness first. Do not integrate into main WHDFetch yet.
- Keep the code portable C with Amiga/C89 compatibility in mind.
- Avoid C99-only conveniences unless the existing harness already requires them and there is a clear reason.
- Avoid relying on POSIX-only behaviour where an Amiga port would need major rewriting.
- Preserve the existing user-facing harness commands where practical:

```text
--resolve <field> <token>
--parse <filename>
--select <listfile> --profile <name|path>
--report <listfile> --profile <name|path>
```

- Existing tests should continue to pass unless the plan is deliberately revised and documented.
- If a phase reveals that the proposed approach is wrong, update this plan and the progress document rather than forcing the original plan through.

## Current Design Weaknesses To Fix

The current memory use is high because each candidate stores too much data:

- fixed `archive_name[256]`
- fixed `group_key[128]`
- fixed `reject_reason[128]`
- embedded full `VhParsedName`
- `VhParsedName` repeats archive/title/group/version strings
- each parsed field stores fixed token arrays
- each token stores text as `char text[64]`
- every candidate keeps full parsed details even if it is a singleton and never needs scoring

The replacement design should keep only compact selection data resident and treat detailed parse data as temporary working data.

---

# Phase 1: Establish A Baseline And Protect Current Behaviour

## Objective

Before changing storage, capture the current state clearly so later improvements can be measured and behaviour regressions are caught.

## Tasks

1. Inspect the current harness structure under:

```text
tools\variant_harness
```

2. Locate the current memory report generation code and the existing test output files.

3. Run the current test suite.

Expected command may be:

```text
cd tools/variant_harness
make test
```

Use the repository's actual build/test command if different.

4. Confirm current commands still work:

```text
./variant_harness --resolve Memory Slow
./variant_harness --parse SomeGame_v1.2_AGA_1MbChip_Fr.lha
./variant_harness --select tests/<sample>.txt --profile <profile>
./variant_harness --report tests/<sample>.txt --profile <profile>
```

Adjust executable name and paths to match the repository.

5. Add or confirm regression samples for:

```text
tests/grouping_false_positive_cases.txt
tests/language_cases.txt
tests/memory_cases.txt
tests/special_cases.txt
tests/games_small_real.txt
tests/games_stress_large.txt
```

6. If expected-output tests already exist, preserve them. If they do not, add lightweight expected-output checks for at least:

- compact language expansion, for example `FrDeEn`
- memory aliases, for example `Slow` and `SlowMem`
- singleton selection
- duplicate group winner selection
- false-positive grouping cases

7. Update `docs/variant_harness_data_loading_storage_analysis.md` with a new section:

```markdown
## Refactor Baseline

- Date:
- Commit/branch if known:
- Tests run:
- Current full Games candidate count:
- Current peak estimate:
- Known failing or missing tests:
```

## Acceptance Criteria

- Current behaviour is documented.
- Tests run before structural changes.
- Baseline memory figures are recorded.
- Any existing failures are documented before the refactor starts.

---

# Phase 2: Add A String Pool

## Objective

Stop duplicating filename and group-key strings inside every candidate. Add a reusable string-pool module that stores strings once and returns offsets.

## Design

Create new files, names may be adjusted to match project style:

```text
tools\variant_harness\src\vh_string_pool.c
tools\variant_harness\src\vh_string_pool.h
```

Suggested API:

```c
typedef struct VhStringPool VhStringPool;

int vh_string_pool_init(VhStringPool *pool);
void vh_string_pool_free(VhStringPool *pool);
int vh_string_pool_add(VhStringPool *pool, const char *s, unsigned long *out_off);
const char *vh_string_pool_get(const VhStringPool *pool, unsigned long off);
unsigned long vh_string_pool_bytes_used(const VhStringPool *pool);
```

Implementation notes:

- Use one growing byte buffer.
- Store NUL-terminated strings.
- Return offsets rather than pointers, so realloc does not invalidate candidate references.
- Offset 0 may either be a valid string offset or reserved for null. Pick one and document it.
- Use `unsigned long` for offsets to keep it portable and simple.
- Avoid one malloc per string.

## Tasks

1. Implement the string pool.
2. Add unit-style harness tests for:

- adding one string
- adding several strings
- retrieving strings after growth/realloc
- empty string behaviour
- invalid offset handling if applicable

3. Add memory accounting for string-pool bytes.
4. Update Makefile/build files.
5. Update the progress document with:

```markdown
## Phase 2 String Pool

- Files added:
- Offset type:
- Growth strategy:
- Tests added:
- Notes for Amiga/C89 compatibility:
```

## Acceptance Criteria

- String pool tests pass.
- Existing harness tests still pass.
- The pool can be used without storing direct pointers in candidates.

---

# Phase 3: Introduce Compact Candidate Records

## Objective

Replace the resident candidate representation used by selection with a compact structure that does not embed a full parse result.

## Proposed Structure

Create a compact candidate type, for example:

```c
typedef struct VhCandidateLite {
    unsigned long name_off;       /* offset into string pool */
    unsigned long group_hash;     /* hash of normalised group key */
    unsigned long group_off;      /* offset into string pool, or 0 if omitted */
    unsigned short original_index;
    short score;
    unsigned char flags;
    unsigned char reject_reason;
} VhCandidateLite;
```

If `unsigned short original_index` is too small for future use, use `unsigned long` or `unsigned int`, but record the decision. Current Games DAT size is around 3,973 entries, so 16-bit is enough for now, but future safety may justify a wider type.

Possible flags:

```c
#define VH_CAND_SELECTED   0x01
#define VH_CAND_REJECTED   0x02
#define VH_CAND_SINGLETON  0x04
#define VH_CAND_HAS_SPECIAL 0x08
```

Possible reject codes:

```c
typedef enum VhRejectReason {
    VH_REJECT_NONE = 0,
    VH_REJECT_LANGUAGE,
    VH_REJECT_CHIPSET,
    VH_REJECT_MEMORY,
    VH_REJECT_VIDEO,
    VH_REJECT_MEDIA,
    VH_REJECT_SPECIAL,
    VH_REJECT_CLI_FILTER
} VhRejectReason;
```

## Tasks

1. Add `VhCandidateLite` alongside the existing candidate type.
2. Add size reporting for the new struct.
3. Do not remove the old candidate path yet unless the change is trivial and tests are strong.
4. Add compile-time or runtime memory report fields:

```text
candidate_lite_struct_size_bytes
candidate_lite_array_bytes
string_pool_bytes
order_array_bytes
estimated_selector_peak_bytes
```

5. Update the progress document with the actual struct size produced by the compiler.

## Acceptance Criteria

- New compact type exists and is measured.
- No current behaviour has changed yet unless explicitly documented.
- The new struct is preferably under 48 bytes.

---

# Phase 4: Split Group-Key Parsing From Full Token Parsing

## Objective

Candidate ingest should only determine the archive filename and grouping key. It should not resolve CSV tokens or keep full parsed fields.

## New Lightweight Parser

Add a function similar to:

```c
int vh_parse_group_key(const char *archive_name,
                       char *group_key_buf,
                       size_t group_key_buf_size,
                       unsigned long *out_hash);
```

Expected behaviour:

- Remove `.lha` / `.lzx` extension.
- Use the conservative title token before metadata tokens, initially matching current group-key behaviour.
- Lowercase the key.
- Keep only alphanumeric characters, matching current harness behaviour unless tests indicate otherwise.
- Return a stable hash for sorting/grouping.

Hash requirements:

- Deterministic across platforms.
- Simple enough for Amiga.
- Collision-resistant enough for small DATs, but always verify collisions using the stored group string if possible.

Suggested hash: FNV-1a 32-bit or an existing project hash if already used.

## Tasks

1. Implement `vh_parse_group_key`.
2. Add tests comparing old full parser group keys to new lightweight group keys for representative filenames.
3. Add false-positive grouping tests.
4. Store group keys in the string pool and hashes in `VhCandidateLite`.
5. Document any deliberate differences from the old parser.

## Acceptance Criteria

- Lightweight group-key parser matches current grouping for normal cases.
- False-positive grouping tests pass.
- No CSV context is required for lightweight grouping.

---

# Phase 5: Change Candidate Loading To Use Compact Records

## Objective

`vh_group_load_candidates` should stop storing full parsed candidates for the selection path. It should read each line, store the filename in the string pool, derive a group key, and append a compact candidate record.

## Tasks

1. Modify or add a new candidate-list type, for example:

```c
typedef struct VhCandidateLiteList {
    VhCandidateLite *items;
    unsigned long count;
    unsigned long capacity;
    VhStringPool strings;
} VhCandidateLiteList;
```

2. Change candidate ingest to:

```text
read line
trim line
skip blank/comment
store archive name in string pool
create group key using lightweight parser
store group key in string pool
store group hash
append VhCandidateLite
```

3. Avoid full parse during ingest.
4. Consider a two-pass load to avoid repeated realloc:

```text
pass 1: count valid candidate lines
allocate candidate array once
pass 2: load candidates
```

If a two-pass load is not practical yet, keep doubling realloc for now but document it as future work.

5. Keep original input order available through `original_index`.
6. Update selection and report code to retrieve filenames via the string pool.
7. Keep old full-candidate path only if needed for comparison. Prefer removing it once compact path passes tests.

## Acceptance Criteria

- `--select` works using compact candidates.
- Selected output remains in original input order.
- Singleton groups are still selected correctly.
- Duplicate groups are still identified correctly.
- Memory report shows candidate-array memory has dropped sharply.

---

# Phase 6: Parse Only Duplicate Groups For Scoring

## Objective

Full token parsing should be temporary and limited to duplicate groups that require scoring. Singleton groups should not require a full parse unless profile excludes are deliberately applied globally.

## Important Policy Decision

Decide and document how profile excludes apply to singleton groups.

Recommended rule:

```text
Existing WHDFetch CLI filters are hard excludes before grouping.
Profile excludes should reject candidates even in singleton groups when SELECTBEST/PROFILE is active.
Profile include weights only matter when choosing between remaining candidates.
```

However, if testing or project preference chooses another rule, update this plan and document why.

## Tasks

1. Update group selection flow:

```text
sort indexes by group hash, group key, original index
for each group:
    if group count == 1:
        apply required reject checks if policy says so
        select if not rejected
    else:
        full-parse only candidates in this group
        score candidates
        select best non-rejected candidate
        free temporary parse data
```

2. Avoid storing parsed token results in `VhCandidateLite`.
3. If report output needs recognised tokens, reparse only the current group during report generation or produce a more compact report.
4. Add tests proving singleton/profile-exclude behaviour.
5. Add tests proving duplicate scoring still matches previous expected results.

## Acceptance Criteria

- Full parse no longer happens during candidate loading.
- Full parse happens only for duplicate groups and required singleton reject checks.
- Behaviour matches existing expected output or documented revised behaviour.
- Memory report confirms parsed candidate data is no longer resident for all candidates.

---

# Phase 7: Replace Text-Heavy Token Storage With ID-Only Token Lists

## Objective

Shrink the temporary parsed scoring data. Scoring needs token IDs, not copied token text.

## Current Problem

Current token representation:

```c
typedef struct VhFieldToken {
    int id;
    char text[64];
} VhFieldToken;
```

Current token lists reserve multiple fixed text slots per field. This is very large even when most fields have few or no tokens.

## Proposed ID-Only Token List

```c
typedef struct VhIdList {
    unsigned short ids[8];
    unsigned char count;
} VhIdList;
```

Then scoring parse result can be compact:

```c
typedef struct VhParsedScoreName {
    VhIdList chipset;
    VhIdList language;
    VhIdList memory;
    VhIdList video;
    VhIdList media;
    VhIdList special;
    unsigned char has_unknown;
} VhParsedScoreName;
```

If token IDs can exceed 65535, use `unsigned int`. Otherwise prefer `unsigned short`.

## Tasks

1. Add ID-only parse result type.
2. Update scoring to use ID lists.
3. Keep text-heavy parse output only for `--parse` and verbose report/debug modes if needed.
4. For reports, resolve IDs back to token strings from the CSV table when needed.
5. Unknown tokens:
   - Do not store unknown token text in every parsed scoring record.
   - For report mode, collect unknown token text temporarily while reporting that group only.
6. Add sizeof reports for:

```text
VhParsedName old size
VhParsedScoreName new size
VhIdList size
```

## Acceptance Criteria

- Scoring works from ID-only token lists.
- `--parse` remains useful, even if it uses a separate debug parse path.
- Duplicate group scoring produces the same winners as before unless a deliberate bug fix is documented.
- Temporary parse size is much smaller than old `VhParsedName`.

---

# Phase 8: Compact CSV Runtime Storage

## Objective

CSV storage is not the largest issue, but it should be made more Amiga-friendly by reducing small allocations and optionally omitting descriptions in runtime mode.

## Current CSV Behaviour

Each CSV row duplicates token and description strings with separate allocations. This is fine on PC but not ideal for Amiga memory fragmentation.

## Proposed Compact CSV Entry

```c
typedef struct VhCsvEntryLite {
    unsigned short id;
    unsigned long token_off;
    unsigned char flags;
} VhCsvEntryLite;
```

Use a CSV string pool for tokens. Descriptions can be optional.

Possible flags:

```c
#define VH_CSV_DEFAULT 0x01
```

## Tasks

1. Decide whether to refactor CSV storage now or after candidate memory is fixed.
2. If doing now:
   - Store tokens in a string pool.
   - Avoid loading descriptions in normal selection mode.
   - Keep description loading available for tools/debug if useful.
3. Ensure exact match first and case-insensitive fallback still work.
4. Ensure duplicate numeric IDs are still supported as aliases.
5. Add memory report for CSV storage bytes.

## Acceptance Criteria

- CSV lookup behaviour is unchanged.
- Duplicate IDs continue to work.
- CSV memory and allocation count are reduced or documented as deferred.

---

# Phase 9: Improve Memory Reporting

## Objective

Make the harness memory report reflect the new model accurately and use it to decide whether the refactor is sufficient.

## Required Report Fields

Add or update memory report output to include:

```text
candidate_count
average_filename_length
candidate_lite_struct_size_bytes
candidate_lite_array_bytes
string_pool_bytes
csv_storage_estimate_bytes
order_array_bytes
temporary_parse_struct_size_bytes
largest_duplicate_group_size
temporary_parse_peak_estimate_bytes
estimated_selector_peak_bytes
old_model_estimate_bytes_if_available
reduction_vs_old_model_percent
```

## Test Sizes

Run memory reports for:

```text
10 candidates
150 candidates
1000 candidates if a sample exists or can be generated
full Games DAT, around 3973 candidates
```

## Acceptance Targets

- Full Games selector peak under 1 MB.
- Preferred target under 600 KB if practical.
- Candidate struct under 48 bytes if practical.
- No persistent full parsed records.

If these targets are not reached, update the plan with the reason and propose the next reduction.

## Acceptance Criteria

- Memory report is believable and not double-counting embedded structures.
- Full Games memory estimate is generated.
- The progress document contains before/after figures.

---

# Phase 10: Preserve And Improve Report Output

## Objective

Reports are important because bad grouping can silently skip content. Maintain useful report output without storing report-only text on every candidate.

## Tasks

1. Update report code to work from compact candidates and string pool.
2. Print selected/skipped filenames from the string pool.
3. Print scores and reject reasons from compact fields.
4. For recognised special tags and unknown tokens, reparse only the group being reported.
5. Add reject reason text from reject code.
6. Confirm reports still include:

- group key
- selected filename
- selected score
- skipped filenames
- skipped scores
- recognised special tags where available
- unknown tokens where available
- reject reason where applicable

## Acceptance Criteria

- `--report` remains useful for diagnosing grouping and scoring.
- Report mode may use more temporary memory than select mode, but should not require full parsed records for all candidates.
- Report output changes are documented.

---

# Phase 11: Optional Sidecar Parse Cache Design, Do Not Implement Unless Needed

## Objective

Consider a future cache, but do not add it unless memory is already under control and parsing speed becomes a real problem.

## Idea

WHDFetch could later create a compact sidecar file per DAT:

```text
PROGDIR:cache/Games.variant_index
```

It could store:

```text
archive_name_offset
group_hash
group_key_offset
known token IDs
```

This would avoid reparsing unchanged DAT files on slow Amiga systems.

## Important Rule

Do not modify external DAT files. If caching is added later, use WHDFetch-owned sidecar cache files.

## Cache Invalidation Ideas

- DAT filename
- DAT file size
- DAT datestamp
- optional DAT checksum

Profile should not be part of parse-cache invalidation because profile affects scoring, not filename parsing.

## Current Action

Document this as future work only unless test results show parsing time is unacceptable.

---

# Phase 12: Final Harness Validation Before WHDFetch Integration

## Objective

Prove the harness is ready to be ported into WHDFetch.

## Tasks

1. Run all tests.
2. Run memory reports for all standard samples.
3. Run selection/report against real DAT-derived lists.
4. Confirm output order remains original input order.
5. Confirm old hard-filter semantics are preserved or clearly separated for WHDFetch integration.
6. Confirm special tags remain report-visible but not scored by default.
7. Confirm `weight.special=0` default behaviour.
8. Confirm compact language tokens still expand correctly.
9. Confirm memory aliases still resolve correctly.
10. Confirm false-positive grouping tests pass.

## Final Documentation Update

Append a section to `docs/variant_harness_data_loading_storage_analysis.md`:

```markdown
## Compact Memory Refactor Result

### Summary

- Date completed:
- Main approach:
- Candidate struct size before:
- Candidate struct size after:
- Full Games estimate before:
- Full Games estimate after:
- Reduction:

### Behaviour Notes

- Grouping behaviour:
- Singleton/profile-exclude behaviour:
- Report behaviour:
- Special tag handling:

### Test Results

- make test:
- memory report 10 candidates:
- memory report 150 candidates:
- memory report 1000 candidates:
- memory report full Games DAT:

### Deferred Work

- CSV compaction:
- Sidecar parse cache:
- Further C89/Amiga portability work:
- Any known risk before WHDFetch integration:
```

## Acceptance Criteria

- Harness passes tests.
- Full Games memory target is met or a revised target is justified.
- The progress document contains final before/after numbers.
- The code is ready for a later WHDFetch integration phase.

---

# Suggested Implementation Order

Use this order unless testing shows a better path:

```text
1. Baseline tests and documentation
2. String pool
3. CandidateLite struct
4. Lightweight group-key parser
5. Compact candidate loading
6. Duplicate-group-only full parsing
7. ID-only token lists
8. Memory report overhaul
9. Report output repair/improvement
10. Optional CSV compaction
11. Final validation and documentation
```

## Agent Working Notes

- Make small commits or clearly separated changes if the environment supports it.
- After each phase, run tests and update the progress document.
- Do not hide failed tests. Record them and either fix them or explain why the expected result changed.
- Prefer simple, boring C over clever abstractions.
- Keep the harness useful on PC, but constantly check whether each design would be acceptable on Amiga.
- If a proposed structure grows unexpectedly due to compiler padding, reorder fields and measure again.
- If memory targets cannot be reached with the current approach, stop and revise this plan before integrating with WHDFetch.

## Definition Of Done

This refactor is done when:

- The harness no longer stores full parsed candidate objects for all entries.
- Candidate strings are stored once in a pool or referenced safely by offset.
- Full token parsing is temporary and limited to duplicate groups or report needs.
- Scoring works from compact ID lists.
- Full Games DAT estimated selector memory is under the agreed target.
- Tests cover grouping, language, memory, specials, singleton behaviour, and duplicate selection.
- `docs/variant_harness_data_loading_storage_analysis.md` has been updated with clear before/after figures and any deferred work.

