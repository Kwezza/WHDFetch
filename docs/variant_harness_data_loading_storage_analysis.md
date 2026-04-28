# Variant Harness Data Loading, Storage, and Manipulation Analysis

Date: 2026-04-28
Scope: tools/variant_harness current implementation
Purpose: establish a concrete memory-model baseline before redesigning storage for low-memory Amiga targets.

## Refactor Baseline

- Date: 2026-04-28
- Commit/branch if known: cdf7195 on branch dev
- Tests run:
  - tools/variant_harness: make clean ; make ; make test
  - command checks: --resolve, --parse, --select, --report
  - full Games report: --report ../../Bin/Amiga/temp/Dat files/Games(2026-04-17).txt --profile default
- Current full Games candidate count: 3973
- Current peak estimate: 19859932 bytes (~18.94 MiB)
- Known failing or missing tests:
  - No baseline test failures observed.
  - Required Phase 1 sample names now exist at tests root: grouping_false_positive_cases.txt, language_cases.txt, memory_cases.txt, special_cases.txt, games_small_real.txt, games_stress_large.txt.

## Phase 2 String Pool

- Files added:
  - tools/variant_harness/src/vh_string_pool.h
  - tools/variant_harness/src/vh_string_pool.c
  - tools/variant_harness/tests/test_vh_string_pool.c
- Files updated:
  - tools/variant_harness/src/vh_group.h
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/Makefile
  - tools/variant_harness/tests/expected/language_cases.report
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
- Offset type:
  - unsigned long offsets returned by vh_string_pool_add()
  - offset 0 is reserved as null/invalid
- Growth strategy:
  - single contiguous byte buffer
  - initial capacity 256 bytes
  - capacity doubles with realloc until required size fits
  - strings are stored NUL-terminated and deduplicated by exact match
- Tests added:
  - single add/get round-trip
  - multiple adds and dedup offset reuse
  - retrieval stability across realloc growth
  - empty-string handling
  - invalid offset handling
- Memory accounting updates:
  - report now includes string_pool_bytes
  - peak_memory_estimate_bytes now includes string pool bytes
- Notes for Amiga/C89 compatibility:
  - implementation uses only malloc/realloc/free and unsigned long offsets
  - no pointer persistence from pool (offsets are stable across realloc)
  - no C11/C++ features; plain C compatible with current harness style

Measured Phase 2 result snapshot (full Games list: Bin/Amiga/temp/Dat files/Games(2026-04-17).txt):
- candidate_count: 3973
- candidate_struct_size_bytes: 4532
- string_pool_bytes: 212286
- peak_memory_estimate_bytes: 18578370 (~17.72 MiB)
- reduction vs baseline peak (19859932): ~6.45%

## Phase 3 CandidateLite Baseline

- Objective status:
  - Added compact candidate type alongside current full-candidate path.
  - Existing selection behavior remains on full candidates for now.
- Files updated:
  - tools/variant_harness/src/vh_group.h
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/tests/expected/language_cases.report
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/run_milestone4_tests.ps1
- New compact type:
  - VhCandidateLite
  - fields: name_off, group_hash, group_off, original_index, score, flags, reject_reason
  - measured size: 20 bytes
- New report fields added:
  - candidate_lite_struct_size_bytes
  - candidate_lite_array_bytes
  - candidate_full_array_bytes
  - order_array_bytes
  - estimated_selector_peak_bytes
  - string_pool_bytes (retained from Phase 2)

Measured Phase 3 result snapshot (full Games list: Bin/Amiga/temp/Dat files/Games(2026-04-17).txt):
- candidate_count: 3973
- candidate_lite_struct_size_bytes: 20
- candidate_lite_array_bytes: 79460
- order_array_bytes: 15892
- string_pool_bytes: 212286
- estimated_selector_peak_bytes: 307638 (~300.43 KiB)
- peak_memory_estimate_bytes (current full path): 18578370 (~17.72 MiB)

Notes:
- Phase 3 currently introduces and measures the compact type; it does not switch candidate ingest/selection to compact records yet.
- Historical baseline sections below remain useful for before/after comparisons and include pre-refactor figures where noted.

## Phase 4 Group-Key Lightweight Parse Split

- Objective status:
  - Added lightweight group-key parser: vh_parse_group_key().
  - Candidate ingest now derives group key/hash with lightweight parser, while full parse is retained for current scoring/report behavior.
  - Group sort/group-run detection now compares hash first and verifies with group-key string compare.
- Files updated:
  - tools/variant_harness/src/vh_parse.h
  - tools/variant_harness/src/vh_parse.c
  - tools/variant_harness/src/vh_group.h
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/tests/test_vh_parse_group_key.c
  - tools/variant_harness/Makefile
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/expected/language_cases.report
- Lightweight parser behavior:
  - strips .lha/.lzx extension
  - takes conservative title token (before first underscore)
  - normalizes to lowercase alphanumeric group key
  - computes deterministic 32-bit FNV-1a hash (stored as unsigned long)
- Validation added:
  - new unit test binary vh_parse_group_key_test
  - verifies known key outputs
  - verifies lightweight group keys match full parser group keys on representative filename set

Observed metric impact after adding group_hash to VhCandidate:
- candidate_struct_size_bytes: 4536 (was 4532)
- language_cases candidate_full_array_bytes: 22680 (was 22660)
- singleton_profile_exclude candidate_full_array_bytes: 9072 (was 9064)

Notes:
- No intentional selection-logic behavior change; grouping order in reports may differ when hash-order differs from lexical-only order, while selected winners remain unchanged.

## Phase 5 Compact Candidate Ingest

- Objective status:
  - Candidate ingest now stores compact selection records only; full parsed filename payload is no longer resident in each candidate.
  - Selection and report paths now parse on demand from archive names in the string pool.
- Files updated:
  - tools/variant_harness/src/vh_group.h
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/src/vh_score.h
  - tools/variant_harness/src/vh_score.c
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/expected/language_cases.report
- Structural changes:
  - removed resident VhParsedName field from VhCandidate
  - VhCandidateList now keeps parse context pointer for reparse-on-demand
  - scoring API now accepts VhParsedName directly (vh_score_candidate)
- Behavior notes:
  - selected outputs and reject reasons remain aligned with prior behavior in baseline tests
  - report token sections are produced by reparsing selected entries for the active group

Measured Phase 5 result snapshot (full Games list: Bin/Amiga/temp/Dat files/Games(2026-04-17).txt):
- candidate_count: 3973
- candidate_struct_size_bytes: 156
- candidate_lite_struct_size_bytes: 20
- candidate_lite_array_bytes: 79460
- candidate_full_array_bytes: 619788
- order_array_bytes: 15892
- string_pool_bytes: 212286
- estimated_selector_peak_bytes: 307638 (~300.43 KiB)
- peak_memory_estimate_bytes: 1192522 (~1.14 MiB)

Impact vs previous full-path estimate (Phase 4 peak: 18578370):
- reduced to 1192522 bytes
- reduction: ~93.58%

Notes:
- This completes the primary Phase 5 objective: compact resident candidate storage with selection/report operating from reparsed details.
- Remaining gap to long-term target is Phase 6 policy/flow optimization (parse only duplicate groups and policy-scoped singleton parse).

## 1. End-to-End Runtime Flow

1. main.c run_selection loads CSV parse context (vh_parse_context_load).
2. main.c run_selection loads profile rules (vh_profile_load).
3. vh_group_load_candidates reads list file line-by-line and parses each filename.
4. Parsed candidates are appended to one in-memory dynamic array (VhCandidateList.items).
5. vh_group_select_best allocates an order array, sorts/groups, scores, and marks winners.
6. Optional vh_group_print_report allocates another order array for grouped report output.
7. Cleanup frees candidate array, profile state, and CSV parse context.

## 2. How Data Is Loaded

### 2.1 CSV Definitions

Loaded in vh_parse_context_load from data/Defs:
- Language.csv
- Chipset.csv
- Video.csv
- Media.csv
- Memory.csv
- Special.csv

Per CSV row (vh_csv_load):
- id parsed to int
- token duplicated with malloc
- description duplicated with malloc
- is_default flag parsed

Storage container:
- VhCsvFile.entries dynamic array
- growth strategy: realloc doubling (32, 64, 128, ...)

### 2.2 Profile

Loaded in vh_profile_load from profile file.

Important memory behavior:
- VhProfile is fixed-size stack/struct storage.
- include and exclude lists are fixed-size int arrays (no heap per token list).
- unresolved include and exclude strings are fixed arrays inside the profile struct.

### 2.3 Candidate List File

Loaded in vh_group_load_candidates.

Per input line:
1. Trim line and skip blank/comment.
2. Parse filename into a full VhParsedName.
3. Copy archive_name and group_key to top-level candidate fields.
4. Append full VhCandidate to a single dynamic array using realloc growth.

This means the harness keeps all parsed candidates in memory simultaneously.

## 3. In-Memory Layout (Current)

## 3.1 Core Structs

From current headers and report output:

- VhFieldToken
  - int id
  - char text[64]
  - size observed: 68 bytes

- VhTokenList
  - VhFieldToken items[8]
  - int count
  - size observed: 548 bytes

- VhParsedName
  - archive_name[256]
  - title[128]
  - group_key[128]
  - version[32]
  - token lists for: chipset, language, memory, video, media, special, unknown
  - size observed: 4380 bytes

- VhCandidate
  - archive_name[256]
  - group_key[128]
  - original_index, selected, rejected, score
  - reject_reason[128]
  - embedded VhParsedName
  - size observed in harness output: 4908 bytes

## 3.2 Dynamic Allocations by Phase

- CSV load:
  - dynamic entries array per CSV
  - malloc for every token and description string

- Candidate ingest:
  - one large dynamic VhCandidate array
  - growth by realloc doubling

- Selection pass:
  - temporary int order array of count entries

- Report pass:
  - temporary int order array of count entries

No compact representation exists between parse and score. The full parsed object is retained for every candidate.

## 4. Manipulation Model

## 4.1 Parse Stage

Parser tokenizes each filename by underscore and fills VhParsedName fields:
- detects version token
- resolves tokens against CSVs
- expands compact language tokens
- stores unmatched tokens into unknown list
- builds conservative group_key from title

All token text is copied into fixed token slots in VhTokenList arrays.

## 4.2 Group and Select Stage

Selection logic:
1. Build order array of indexes.
2. Sort indexes by group_key then original_index.
3. For each group run, score each candidate.
4. Mark single winner (or none if all rejected).

The candidate array remains resident for the full operation.

## 4.3 Report Stage

Report iterates grouped candidates and prints selected/skipped info.
Memory estimate output is computed from the in-memory candidate array.

## 5. Memory Baseline for Large Games Sample

Input used:
- Bin/Amiga/temp/Dat files/Games(2026-04-17).txt
- non-empty candidate lines counted: 3973

Observed/derived baseline:
- candidate_struct_size_bytes: 4908
- candidate array bytes: 19,499,484
- one order array bytes: 15,892
- base working set (candidate array + one order array): 19,515,376 bytes
  - about 18.61 MiB

Additional memory beyond base:
- CSV definition heap allocations (entries + token strings + description strings)
- allocator overhead and temporary stack buffers
- report token-estimate term currently printed by harness

This explains why practical runtime estimates trend toward or above 20 MiB for large pack lists.

## 6. Important Caveat About Current Estimate Output

The current memory estimate in vh_group_print_report prints:
- parsed_token_storage_estimate_bytes = parsed_token_count * sizeof(VhFieldToken)

This is useful as an analytical token-load indicator, but token storage is already embedded in each VhCandidate via fixed token arrays. It should be interpreted as a pressure indicator rather than an additional separately allocated heap block.

## 7. Why This Is Expensive on Amiga-Class Memory

Current design choices that drive memory use:
- full-candidate retention for entire file
- large fixed-size embedded strings in every candidate
- fixed token capacity allocated per field even when mostly empty
- no compact id-only representation after parse
- no streaming or chunked group reduction

For a 4 MiB target, the current all-in-memory model is not viable for large Games lists.

## 8. Source References

- tools/variant_harness/src/main.c
- tools/variant_harness/src/vh_group.h
- tools/variant_harness/src/vh_group.c
- tools/variant_harness/src/vh_parse.h
- tools/variant_harness/src/vh_parse.c
- tools/variant_harness/src/vh_csv.h
- tools/variant_harness/src/vh_csv.c
- tools/variant_harness/src/vh_profile.h
- tools/variant_harness/src/vh_profile.c
- tools/variant_harness/src/vh_score.c
