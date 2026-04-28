# Variant Harness Data Loading, Storage, and Manipulation Analysis

Date: 2026-04-28
Scope: tools/variant_harness current implementation
Purpose: establish a concrete memory-model baseline before redesigning storage for low-memory Amiga targets.

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
