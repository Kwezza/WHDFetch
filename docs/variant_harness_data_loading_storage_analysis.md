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

## Phase 6 Duplicate-Only Weighted Scoring Policy

- Objective status:
  - Selection flow now treats singleton groups differently from duplicate groups.
  - Singleton groups: parse candidate and apply reject checks only.
  - Duplicate groups: parse candidates and apply full weighted scoring for best-selection.
- Files updated:
  - tools/variant_harness/src/vh_score.h
  - tools/variant_harness/src/vh_score.c
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
- API/policy updates:
  - vh_score_candidate now accepts apply_include_weights flag.
  - singleton run path calls vh_score_candidate(..., 0, ...) to enforce reject-only handling.
  - duplicate run path calls vh_score_candidate(..., 1, ...) for full score comparison.

Observed behavior change captured by baseline:
- singleton excluded cases now report score=0 (previously included weighted score contribution was shown even though candidate was rejected)
- reject reason and selected/skipped behavior remain unchanged

Notes:
- This policy prevents include weights from influencing singleton outcomes while preserving hard reject filters.
- Duplicate-group winner selection behavior remains fully weighted and unchanged from prior intent.

## Phase 7 ID-Only Temporary Scoring Parse

- Objective status:
  - Added a compact score-parse path that stores token IDs only (no per-token text copies).
  - Selection scoring now uses the ID-only parse path.
  - Existing detailed parse path is retained for --parse output and report token text sections.
- Files updated:
  - tools/variant_harness/src/vh_parse.h
  - tools/variant_harness/src/vh_parse.c
  - tools/variant_harness/src/vh_score.h
  - tools/variant_harness/src/vh_score.c
  - tools/variant_harness/src/vh_csv.h
  - tools/variant_harness/src/vh_csv.c
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/expected/language_cases.report
- Implementation notes:
  - Added VhTokenIdList (unsigned short ids[8] + count).
  - Added VhParsedScoreName with id lists for chipset/language/memory/video/media/special plus has_unknown flag.
  - Added vh_parse_filename_score() for temporary score parsing.
  - Updated vh_score_candidate() to score against VhParsedScoreName.
  - Reject messages still show token text by resolving ID back to canonical CSV token via vh_csv_lookup_id().

Measured size metrics (from current report output):
- id_list_struct_size_bytes: 18
- parsed_name_old_struct_size_bytes: 4380
- parsed_score_struct_size_bytes: 110

Behavior notes:
- Winner selection and reject outcomes remain aligned with prior baselines.
- Memory report now includes the additional struct-size fields listed above.

## Phase 8 Compact CSV Runtime Storage

- Objective status:
  - CSV runtime storage now uses one pooled string buffer per CSV file with offset-based entries.
  - Per-row malloc/free for token and description strings has been removed.
  - CSV lookup behavior remains unchanged (exact match first, case-insensitive fallback, duplicate-id alias canonicalization retained).
- Files updated:
  - tools/variant_harness/src/vh_csv.h
  - tools/variant_harness/src/vh_csv.c
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/Makefile
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/expected/language_cases.report
- Implementation notes:
  - VhCsvEntry now stores token_off/description_off plus flags (VH_CSV_ENTRY_DEFAULT).
  - VhCsvFile now owns a VhStringPool for all CSV strings.
  - Added vh_csv_bytes_used() and surfaced csv_storage_estimate_bytes in report memory output.
  - Makefile parse-group-key test link line now includes src/vh_string_pool.c (required because vh_csv depends on pooled storage).

Observed report updates:
- csv_storage_estimate_bytes field now present (sample value in current test environment: 4733)
- peak_memory_estimate_bytes now includes CSV storage estimate

Validation:
- make clean ; make test passed
- milestone script output:
  - Phase 1 baseline comparisons passed
  - Phase 2 baseline comparisons passed
  - Phase 3 memory estimate output present
  - Phase 3 stress candidate-count assertion passed

## Phase 11 Sidecar Parse Cache Design (Deferred)

- Objective status:
  - Defined as design-only in this phase.
  - No sidecar cache implementation has been added to the harness.
  - Current behavior remains parse-on-demand from list entries.

Decision:
- Defer implementation until profiling shows parse time is a practical bottleneck on target Amiga systems.
- Keep this as explicit future work; do not modify DAT content files.

Proposed sidecar location and ownership:
- Location concept: WHDFetch-owned cache path, for example `PROGDIR:cache/Games.variant_index`.
- Ownership: cache files are internal artifacts created and managed by WHDFetch, never written into DAT source files.

Proposed cache payload (compact):
- archive_name_off
- group_hash
- group_key_off
- token-id lists required for score path

Suggested file format direction:
- Small binary header with version/magic.
- Compact tables using offsets and fixed-width ids.
- Optional string block for archive/group key interned text.

Invalidation strategy (required):
- DAT filename
- DAT file size
- DAT datestamp
- Optional DAT checksum if available/cheap

Important rule:
- Profile is not part of parse-cache invalidation, since profile affects scoring policy, not filename parse results.

Safety/fallback behavior for future implementation:
- On cache read failure or mismatch, fall back to current parse-on-demand path.
- On successful full parse run, refresh cache atomically.
- Keep cache versioned for forward/backward format changes.

Exit criteria for moving this out of deferred state:
- Measurable wall-time bottleneck demonstrated on real target workloads.
- Cache correctness proven against current output baselines.
- No memory-regression vs current compact model.

## Compact Memory Refactor Result

### Summary

- Date completed: 2026-04-29
- Main approach:
  - compact candidate ingest with string-pool-backed filenames/group keys
  - duplicate-group-only weighted scoring with id-only temporary parse structures
  - pooled CSV runtime storage
  - report-mode group-local reparse for special/unknown diagnostics
- Candidate struct size before: 4908 bytes
- Candidate struct size after: 160 bytes
- Full Games estimate before: 19859932 bytes
- Full Games estimate after: 1213147 bytes (`peak_memory_estimate_bytes`, current conservative report-path estimate)
- Full Games selector-path estimate: 307638 bytes (`estimated_selector_peak_bytes`)
- Reduction (before vs current conservative estimate): 93.89%

### Behaviour Notes

- Grouping behaviour:
  - lightweight group key + hash sorting with string verification retained
  - false-positive grouping baseline checks pass in milestone suite
- Singleton/profile-exclude behaviour:
  - singleton groups apply reject checks without include-weight scoring
  - rejected entries keep score 0 in singleton cases
- Report behaviour:
  - report prints selected/skipped from compact candidate storage and string pool
  - rejected entries include reject category and reason text
  - special/unknown token sections are built by reparsing only current group
- Special tag handling:
  - visible in report output (group token aggregation)
  - not scored by default profile (`weight.special=0`)

### Test Results

- make test:
  - pass (`make clean ; make test`)
  - milestone script confirms Phase 1/2 baseline comparisons and Phase 3 assertions
- memory report 10 candidates:
  - candidate_count: 10
  - estimated_selector_peak_bytes: 700
  - peak_memory_estimate_bytes: 7717
- memory report 150 candidates:
  - candidate_count: 150
  - estimated_selector_peak_bytes: 11943
  - peak_memory_estimate_bytes: 50596
- memory report 1000 candidates:
  - candidate_count: 1000
  - estimated_selector_peak_bytes: 77600
  - peak_memory_estimate_bytes: 307537
- memory report full Games DAT:
  - candidate_count: 3973
  - estimated_selector_peak_bytes: 307638
  - peak_memory_estimate_bytes: 1213147
- additional Phase 12 checks:
  - output order remains original input order on full Games select output (pass)
  - compact language token expansion confirmed (`FrDeEn` -> `Fr, De, En`)
  - memory alias resolution confirmed (`Slow`, `SlowMem`)
  - selection/report runs completed for real DAT-derived lists and full Games list

### Deferred Work

- CSV compaction:
  - primary compaction completed in Phase 8 (pooled strings + offset entries)
  - optional further reduction: skip description storage in strict runtime mode if needed
- Sidecar parse cache:
  - deferred by design (Phase 11)
  - implement only when profiling demonstrates parse-time bottleneck on target hardware

## Phase 9 Memory Reporting Expansion

- Objective status:
  - Memory report now includes the remaining Phase 9 required fields for temporary parse pressure and old-vs-new model comparison.
  - Reports were generated for the required sizes: 10, 150, 1000, and full Games list.
- Files updated:
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/expected/language_cases.report

Added/updated report fields (Phase 9 required set):
- candidate_count
- average_filename_length
- candidate_lite_struct_size_bytes
- candidate_lite_array_bytes
- string_pool_bytes
- csv_storage_estimate_bytes
- order_array_bytes
- temporary_parse_struct_size_bytes
- largest_duplicate_group_size
- temporary_parse_peak_estimate_bytes
- estimated_selector_peak_bytes
- old_model_estimate_bytes_if_available
- reduction_vs_old_model_percent

Measurement runs (default profile):
- 10 candidates (generated subset):
  - candidate_count: 10
  - candidate_lite_array_bytes: 200
  - order_array_bytes: 40
  - string_pool_bytes: 460
  - csv_storage_estimate_bytes: 4733
  - temporary_parse_struct_size_bytes: 110
  - largest_duplicate_group_size: 5
  - temporary_parse_peak_estimate_bytes: 550
  - estimated_selector_peak_bytes: 700
  - peak_memory_estimate_bytes: 7677
  - old_model_estimate_bytes_if_available: 54737
  - reduction_vs_old_model_percent: 85.97
- 150 candidates (generated subset):
  - candidate_count: 150
  - candidate_lite_array_bytes: 3000
  - order_array_bytes: 600
  - string_pool_bytes: 8343
  - csv_storage_estimate_bytes: 4733
  - temporary_parse_struct_size_bytes: 110
  - largest_duplicate_group_size: 5
  - temporary_parse_peak_estimate_bytes: 550
  - estimated_selector_peak_bytes: 11943
  - peak_memory_estimate_bytes: 49996
  - old_model_estimate_bytes_if_available: 754453
  - reduction_vs_old_model_percent: 93.37
- 1000 candidates (generated subset):
  - candidate_count: 1000
  - candidate_lite_array_bytes: 20000
  - order_array_bytes: 4000
  - string_pool_bytes: 53600
  - csv_storage_estimate_bytes: 4733
  - temporary_parse_struct_size_bytes: 110
  - largest_duplicate_group_size: 10
  - temporary_parse_peak_estimate_bytes: 1100
  - estimated_selector_peak_bytes: 77600
  - peak_memory_estimate_bytes: 303537
  - old_model_estimate_bytes_if_available: 5001937
  - reduction_vs_old_model_percent: 93.93
- Full Games list (Bin/Amiga/temp/Dat files/Games(2026-04-17).txt):
  - candidate_count: 3973
  - candidate_lite_array_bytes: 79460
  - order_array_bytes: 15892
  - string_pool_bytes: 212286
  - csv_storage_estimate_bytes: 4733
  - temporary_parse_struct_size_bytes: 110
  - largest_duplicate_group_size: 12
  - temporary_parse_peak_estimate_bytes: 1320
  - estimated_selector_peak_bytes: 307638
  - peak_memory_estimate_bytes: 1197255
  - old_model_estimate_bytes_if_available: 19864665
  - reduction_vs_old_model_percent: 93.97

Acceptance target check:
- Full Games selector peak under 1 MB: not met (estimated_selector_peak_bytes is 307638, but current peak_memory_estimate_bytes is 1197255 due to report-path accounting including full candidate struct and analytical token estimate).
- Preferred target under 600 KB: not met for peak_memory_estimate_bytes, but met for estimated_selector_peak_bytes.
- Candidate struct under 48 bytes: not met (VhCandidate = 156 bytes).
- No persistent full parsed records: met.

Notes:
- `estimated_selector_peak_bytes` reflects compact selection-path resident data.
- `peak_memory_estimate_bytes` remains a conservative report-path estimate and includes analytical token-storage pressure plus CSV/storage terms.

## Phase 10 Report Output Preservation And Improvements

- Objective status:
  - Report output remains compact-candidate based with filenames read from the string pool.
  - Skipped rejected entries now include reject-category text derived from reject code.
  - Recognised special tags and unknown tokens are now gathered by reparsing only the current reported group.
- Files updated:
  - tools/variant_harness/src/vh_score.h
  - tools/variant_harness/src/vh_score.c
  - tools/variant_harness/src/vh_group.h
  - tools/variant_harness/src/vh_group.c
  - tools/variant_harness/tests/expected/singleton_profile_exclude.report
  - tools/variant_harness/tests/expected/language_cases.report

Implementation notes:
- Added structured reject code flow:
  - VhScoreResult now carries VhRejectCode.
  - VhCandidate stores reject_code alongside reject_reason text.
  - Report formatting prints `rejected (<code>: <detail>)` when detail exists.
- Group-local token reporting:
  - During report, each group run is reparsed and aggregated into temporary string pools for special and unknown token display.
  - No persistent report-only token text is stored on candidates.

Observed behavior updates:
- Singleton exclude report now shows reject category prefixes, for example:
  - `rejected (special: special excluded by profile token 'Censored')`
  - `rejected (video: video excluded by profile token 'NTSC')`
- Group token display can now include tokens present in the reported group even when no selected candidate carries them.

Memory-impact note:
- VhCandidate size increased from 156 to 160 bytes due to reject_code field.
- This slightly increases report memory totals while preserving compact ingest and non-persistent parse design.

Validation:
- make clean ; make test passed
- milestone script output:
  - Phase 1 baseline comparisons passed
  - Phase 2 baseline comparisons passed
  - Phase 3 memory estimate output present
  - Phase 3 stress candidate-count assertion passed

## Phase 13 Real Full-DAT Allocation Measurement

- Date: 2026-04-29
- Commit/branch: e2ff256 on branch dev
- Full Games DAT path used:
  - Bin/Amiga/temp/Dat files/Games(2026-04-17).txt

Build and test status (tools/variant_harness):
- make clean
- make
- make test
- Status: pass

Commands run for measurement (tools/variant_harness cwd):
- .\variant_harness --select "..\..\Bin\Amiga\temp\Dat files\Games(2026-04-17).txt" --profile default --memtrace
- .\variant_harness --report "..\..\Bin\Amiga\temp\Dat files\Games(2026-04-17).txt" --profile default --memtrace

Measured --select memory trace (real tracked allocations):
- candidate_count: 3973
- selected_count: 2893
- group_count: 2897
- duplicate_group_count: 670
- largest_duplicate_group_size: 12
- actual_peak_heap_bytes: 942612
- actual_current_heap_bytes: 926720
- actual_current_heap_bytes_after_cleanup: 0
- allocation_count: 16
- free_count: 16
- realloc_count: 36
- largest_single_allocation_bytes: 655360
- failed_allocation_count: 0
- all_tracked_allocations_freed: yes

Measured --report memory trace (real tracked allocations):
- candidate_count: 3973
- selected_count: 2893
- group_count: 2897
- duplicate_group_count: 670
- largest_duplicate_group_size: 12
- actual_peak_heap_bytes: 943380
- actual_current_heap_bytes: 926720
- actual_current_heap_bytes_after_cleanup: 0
- allocation_count: 5812
- free_count: 5812
- realloc_count: 5831
- largest_single_allocation_bytes: 655360
- failed_allocation_count: 0
- all_tracked_allocations_freed: yes

Measured vs previous estimate comparison:
- selector-path estimate (existing):
  - estimated_selector_peak_bytes: 307638
- selector-path measured peak (real tracked):
  - actual_peak_heap_bytes (--select): 942612
- delta: +634974 bytes (measured peak is ~3.06x the compact selector estimate)
- conservative/report-path estimate (existing):
  - peak_memory_estimate_bytes: 1213147
- report-path measured peak (real tracked):
  - actual_peak_heap_bytes (--report): 943380
- delta: -269767 bytes (measured peak is lower than conservative estimate)

Interpretation:
- Compact selector path classification:
  - under 1 MB, above 512 KB (942612 bytes)
- Report path classification:
  - under 1 MB in this PC harness measurement (943380 bytes)
- Release suitability decision:
  - keep report path as deferred/non-shipping diagnostics for now; it remains output-heavy and high-churn (5812 allocations, 5831 reallocs) and is not required for normal selection flow
- Leak evidence:
  - none observed in tracked harness allocations (actual_current_heap_bytes_after_cleanup = 0, free_count matches allocation_count)

Coverage and caveats:
- Tracked allocation coverage in harness code:
  - candidate arrays
  - order arrays (selection/report/stats)
  - string pools (candidate names/group keys, CSV pools, report temporary token pools)
  - CSV entry arrays
- Profile heap allocations:
  - none currently used (profile runtime storage is fixed-size struct storage)
- Allocations not covered by this tracker:
  - C runtime and standard library internal allocations (stdio FILE buffering, CRT bookkeeping, process/runtime startup memory)
  - stack memory usage (not heap-tracked)
- Platform caveat:
  - measurements were captured on PC malloc/realloc behavior in the harness; Amiga AllocVec allocator behavior and fragmentation characteristics can differ

Terminology clarity:
- estimated_selector_peak_bytes:
  - compact selector-path analytical estimate (candidate_lite + order + string pool model)
- peak_memory_estimate_bytes:
  - conservative/report/debug-style analytical estimate, intentionally broader than normal compact selector resident terms

## Phase 14 Largest Allocation Investigation

- Date: 2026-04-29
- Commit/branch: e2ff256 on branch dev
- Scope: investigation-only diagnostics in tools/variant_harness (no main WHDFetch integration changes)

Build and test status (tools/variant_harness):
- make clean
- make
- make test
- Status: pass

Commands run (tools/variant_harness cwd):
- .\variant_harness --select "..\..\Bin\Amiga\temp\Dat files\Games(2026-04-17).txt" --profile default --memtrace
- .\variant_harness --report "..\..\Bin\Amiga\temp\Dat files\Games(2026-04-17).txt" --profile default --memtrace

Memtrace implementation findings:
- largest_single_allocation_bytes is updated by both:
  - vh_malloc_tag()/vh_malloc() payload size
  - vh_realloc_tag()/vh_realloc() new target payload size
- Tracker header metadata is not included in largest_single_allocation_bytes.
  - vh_memtrack stores header fields (magic/size/tag index) before payload.
  - reported largest size is payload-only (`largest_single_allocation_is_payload_only: yes`).

Memtrace summary for full Games --select:
- candidate_count: 3973
- selected_count: 2893
- actual_peak_heap_bytes: 942612
- largest_single_allocation_bytes: 655360
- largest_single_allocation_tag: candidate_array
- largest_single_allocation_operation: realloc
- all_tracked_allocations_freed: yes
- notable per-tag peaks:
  - candidate_array: peak_bytes=655360
  - candidate_string_pool: peak_bytes=262144
  - order_array: peak_bytes=15892
  - report_order_array: peak_bytes=15892

Memtrace summary for full Games --report:
- candidate_count: 3973
- selected_count: 2893
- actual_peak_heap_bytes: 943380
- largest_single_allocation_bytes: 655360
- largest_single_allocation_tag: candidate_array
- largest_single_allocation_operation: realloc
- all_tracked_allocations_freed: yes
- notable per-tag peaks:
  - candidate_array: peak_bytes=655360
  - candidate_string_pool: peak_bytes=262144
  - report_order_array: peak_bytes=15892
  - report_temp_pool: peak_bytes=768

Root cause of 655360-byte allocation:
- Owner/source:
  - candidate_array (VhCandidateList.items) in vh_group candidate ingest growth path.
- Why it reaches this size:
  - candidate array grows by doubling capacity via realloc.
  - At full Games size, growth reaches capacity 4096 elements.
  - sizeof(VhCandidate) is 160 bytes.
  - 4096 * 160 = 655360 bytes.
- Allocation type/lifetime:
  - This is a realloc growth step to the final resident candidate-array capacity for the run.
  - It remains allocated through selection/report processing, then is freed at cleanup.

Useful bytes vs capacity at full Games size:
- Useful bytes stored (actual candidates):
  - 3973 * 160 = 635680 bytes.
- Allocated capacity bytes:
  - 4096 * 160 = 655360 bytes.
- Capacity slack due to doubling policy:
  - 655360 - 635680 = 19680 bytes (123 spare slots, about 3.0% slack).

Interpretation:
- The largest allocation is expected and structural, not an accidental leak or runaway temp buffer.
- It is mainly the normal final doubling capacity step for the candidate array.
- The slack is modest at this dataset size (about 3%), so most of the block is useful payload.

Count-first/exact-allocation impact (analysis only):
- A count-first exact allocation strategy would likely remove this slack and avoid over-capacity growth steps.
- At this dataset size, direct memory savings for candidate_array would be about 19680 bytes.
- This is an optimization opportunity, not required for correctness.

Amiga integration risk assessment:
- PC harness result confirms the dominant single block is one contiguous 640 KiB-class candidate array payload.
- On Amiga AllocVec, contiguous allocation availability and fragmentation can be a bigger constraint than total free memory.
- Risk level for first integration:
  - acceptable with caveat for systems that have sufficient contiguous Fast RAM.
  - potentially fragile on highly fragmented or very low-memory targets.
- Recommendation for current milestone:
  - keep as-is for first WHDFetch integration (investigation objective met; no obvious bug).
  - later improvement option: pre-count and exact-size candidate array (or bounded chunking) if Amiga runtime tests show fragmentation failures.

Caveat:
- These measurements and allocation behavior were captured in the PC harness runtime allocator.
- Final Amiga behavior should be validated with real AllocVec telemetry on representative target hardware/configurations.

## Phase 15 Amiga Variant Test Utility

- Date: 2026-04-29
- Objective status:
  - Added a standalone Amiga CLI test utility for the existing compact selector path.
  - No integration into main WHDFetch runtime.
  - No selector behavior changes were introduced intentionally.
  - Binary cache/blob work remains explicitly deferred.

Files added:
- tools/variant_harness/src/amiga_varianttest.c

Files updated:
- tools/variant_harness/Makefile
- docs/variant_harness_data_loading_storage_analysis.md

Build target added:
- tools/variant_harness Makefile target: `varianttest-amiga`
- intended output binary: `Bin/Amiga/varianttest`
- object output folder: `build/amiga/varianttest/`

CLI usage:
- `varianttest [DATFILE] [PROFILE]`
- help flags: `HELP`, `?`, `-h`, `--help`

Examples:
- `varianttest`
- `varianttest "Bin/Amiga/temp/Dat files/Games(2026-04-17).txt"`
- `varianttest "Bin/Amiga/temp/Dat files/Games(2026-04-17).txt" default`

Default path resolution implemented:
- DAT candidates (in order):
  - `temp/Dat files/Games(2026-04-17).txt`
  - `Bin/Amiga/temp/Dat files/Games(2026-04-17).txt`
  - `PROGDIR:temp/Dat files/Games(2026-04-17).txt`
- profile resolution:
  - if PROFILE looks like a path, load directly
  - otherwise resolves `<name>.profile` from:
    - `data/Profiles/`
    - `tools/variant_harness/data/Profiles/`
    - `PROGDIR:data/Profiles/`
    - `PROGDIR:tools/variant_harness/data/Profiles/`
- defs resolution:
  - `data/Defs`
  - `tools/variant_harness/data/Defs`
  - `PROGDIR:data/Defs`
  - `PROGDIR:tools/variant_harness/data/Defs`

Output files written by utility:
- summary:
  - primary: `PROGDIR:varianttest_result.txt`
  - fallback: `varianttest_result.txt`
- selected archive list (one selected archive filename per line in original input order):
  - primary: `PROGDIR:varianttest_selected.txt`
  - fallback: `varianttest_selected.txt`

Terminal output behavior:
- Prints the same summary fields that are written to summary file.
- Prints the first 10 selected archive names for quick validation.
- Does not dump full selection list to console.

Summary fields currently emitted:
- test label/version
- DAT file path used
- profile path used
- defs path used
- candidate_count
- selected_count
- group_count
- duplicate_group_count
- largest_duplicate_group_size
- elapsed_ms (or timing unavailable line)
- actual_peak_heap_bytes
- actual_current_heap_bytes
- actual_current_heap_bytes_after_cleanup
- largest_single_allocation_bytes
- allocation_count/free_count/realloc_count
- failed_allocation_count
- all_tracked_allocations_freed
- result success/failure
- error message on failure

PC regression status:
- Before changes: `make clean ; make ; make test` passed.
- After changes: `make clean ; make ; make test` passed.
- Existing harness behavior and milestone test outputs remain passing.

Amiga vbcc build status in this environment:
- `make varianttest-amiga` target and compile commands are present and execute.
- Build progressed through multiple object files (including `amiga_varianttest.o`, `vh_csv.o`, `vh_memtrack.o`, `vh_parse.o`, `vh_profile.o`, `vh_score.o`, `vh_group.o`, `vh_string_pool.o`).
- Final link did not succeed in this local setup due unresolved vbcc runtime symbols (`___builtin_*`, `__ieee*`) when linking harness objects.
- Result in this environment: no final `Bin/Amiga/varianttest` binary produced yet.

Known limitations:
- This utility is intentionally concise and standalone; no GUI/report-expansion/cache features were added.
- Runtime symbol resolution for this harness subset under local vbcc setup needs follow-up to complete final link consistently.

How to run on Amiga once linked binary is available:
- Copy `varianttest` to `Bin/Amiga` (or run from your chosen tools folder).
- Ensure DAT file and profile/Defs paths are available per resolution rules above.
- Run:
  - `varianttest`
  - or `varianttest "PROGDIR:temp/Dat files/Games(2026-04-17).txt" default`

Requested runback data for hardware/emulation validation:
- CPU config used (68000/68020/68030/68040/68060, FPU yes/no)
- total and free Fast RAM / Chip RAM at start
- elapsed_ms
- candidate_count/selected_count/group_count
- duplicate_group_count/largest_duplicate_group_size
- actual_peak_heap_bytes
- largest_single_allocation_bytes
- all_tracked_allocations_freed
- any failure line from summary output

Deferred work note:
- Binary blob/cache mechanisms remain deferred by decision; this phase is only a standalone viability test utility for real/emulated Amiga runs.

## Historical Appendix (Pre-Refactor Baseline Snapshot)

The sections below are retained for historical comparison with pre-refactor behavior and structure.

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
