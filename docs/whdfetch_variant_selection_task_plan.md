# WHDFetch Variant Selection Backport Task Plan

## Purpose

This plan breaks the WHDFetch weighted variant-selection feature into small, agent-friendly chunks.

The aim is to backport the useful filename parsing and profile-scoring knowledge from the shelved GUI version without dragging the full GUI/catalogue system into WHDFetch.

The first milestone is deliberately limited:

- Select the best archive from apparent duplicate title groups.
- Use language, chipset, video, memory, and media fields first.
- Parse special tags, but keep them report-only in the first milestone.
- Preserve existing WHDFetch behaviour unless the new feature is explicitly enabled.
- Develop and test the core text-processing engine on PC with GCC before integrating into the Amiga build.

The second milestone can then enable `special` tags as low-weight tie-breakers.

---

## Guiding Principles

1. **WHDFetch must walk before it runs**
   - Do not try to implement the whole GUI metadata system in one pass.
   - Do not add pretty-name/catalogue logic to the first milestone.
   - Do not attempt fuzzy title matching yet.

2. **Selection is opt-in**
   - Existing WHDFetch CLI and INI behaviour must remain unchanged unless explicitly enabled.
   - Suggested CLI switch: `SELECTBEST`.
   - Suggested optional profile selector: `PROFILE=<name>`.

3. **Existing filters remain hard filters**
   - Existing options such as `SKIPAGA`, `SKIPCD`, `SKIPNTSC`, and `SKIPNONENGLISH` should remove candidates before weighted selection.
   - The profile scorer only ranks what remains.

4. **Keep the PC test harness close to Amiga C**
   - Use portable C.
   - Avoid C99 features if WHDFetch itself is strict C89.
   - Avoid POSIX-only assumptions in core code.
   - Keep file/path handling abstract enough to adapt to Amiga paths later.

5. **Group conservatively**
   - Bad grouping is more dangerous than weak scoring.
   - A false duplicate group could cause WHDFetch to skip a genuinely separate title.
   - Start with a simple group key based on the filename title before recognised metadata tokens.

6. **Parse special tags before scoring them**
   - In milestone 1, recognise and report `special` tags.
   - Do not let special tags affect selection until milestone 2.

---

## Recommended Branches and Work Areas

Suggested source branch:

```text
feature/variant-selection-backport
```

Suggested temporary extraction/staging folder outside WHDFetch:

```text
variant_backport_staging/
```

Suggested PC harness folder, either inside WHDFetch or standalone during development:

```text
tools/variant_harness/
```

Suggested WHDFetch source folder after integration:

```text
src/variant/
```

Suggested WHDFetch runtime data folder:

```text
Data/
  Defs/
  Profiles/
  pack_types.ini
```

---

# Milestone 0: Create a Backport Staging Pack from the GUI Version

## Goal

Create a clean folder containing only the files likely to be useful for WHDFetch variant selection.

This is not integration yet. It is an extraction and classification task.

## Task 0.1: Create staging folder

Create a new folder beside the GUI project or in a temporary workspace:

```text
variant_backport_staging/
  assets_raw/
  src_raw/
  notes/
```

## Task 0.2: Copy raw asset files from the GUI project

Copy the GUI version's definition files into:

```text
variant_backport_staging/assets_raw/defs/
```

Include, if present:

```text
Chipset.csv
Language.csv
Memory.csv
Video.csv
Media.csv
variant_tags.csv
special copy.csv
```

Also copy these for reference only:

```text
compilations.csv
contributors.csv
cover_disks.csv
crack_groups.csv
demo_groups.csv
Disks.csv
filename.csv
magazines.csv
name_overrides.csv
official_amiga_bundles.csv
software_houses.csv
special_overrides.csv
```

Copy configuration/profile files into:

```text
variant_backport_staging/assets_raw/prefs/
variant_backport_staging/assets_raw/profiles/
```

Include:

```text
pack_types.ini
prefs.ini
*.profile
```

## Task 0.3: Copy raw source files from the GUI project

Copy these files, if present, into:

```text
variant_backport_staging/src_raw/
```

Filename parsing pipeline:

```text
src/tlv_filename/filename_processor.c
src/tlv_filename/field_registry.c
src/tlv_filename/csv_cache.c
src/tlv_filename/tlv_builder.c
```

Filter/profile/scoring pipeline:

```text
src/filter/filter_runtime.c
src/filter/profile_loader.c
src/filter/filter_profile.c
src/filter/filter_pipeline.c
src/filter/variant_iterator.c
src/filter/variant_index.c
```

Any related headers should also be copied.

## Task 0.4: Rename awkward asset names in staging only

In the staging folder, rename:

```text
special copy.csv
```

to:

```text
Special.csv
```

Do not change the GUI project yet. This rename is for WHDFetch packaging only.

## Task 0.5: Create staging inventory document

Create:

```text
variant_backport_staging/notes/backport_inventory.md
```

Document each file under these headings:

```text
Use In Milestone 1
Use In Milestone 2
Reference Only
Ignore For WHDFetch
Needs Review
```

Initial suggested classification:

### Use In Milestone 1

```text
Chipset.csv
Language.csv
Memory.csv
Video.csv
Media.csv
variant_tags.csv
pack_types.ini
basic profile files
CSV lookup code
field registry code
filename token parser code
profile loader code
scoring code
```

### Use In Milestone 2

```text
Special.csv
special tag parsing rules
special tag scoring rules
```

### Reference Only

```text
compilations.csv
contributors.csv
cover_disks.csv
crack_groups.csv
demo_groups.csv
Disks.csv
filename.csv
magazines.csv
name_overrides.csv
official_amiga_bundles.csv
software_houses.csv
special_overrides.csv
```

## Task 0.6: Record known gotchas

Add a `Known Gotchas` section to `backport_inventory.md`.

Include at least:

```text
- Memory tokens are not simple numeric sizes.
- Multiple spellings can map to the same concept.
- CSV IDs may intentionally be shared by aliases.
- Special tags are a mixed bucket and must not be fully scored in milestone 1.
- Existing GUI key mismatch: ActiveProfile versus active_profile. WHDFetch should standardise on active_profile.
- Language tokens may be compact multi-language forms such as EnDe or FrDeEn.
```

## Acceptance Criteria

- `variant_backport_staging` contains the copied raw assets and source files.
- `backport_inventory.md` exists and classifies the files.
- `Special.csv` exists in staging.
- No WHDFetch source code has been changed yet.

---

# Milestone 1: Build a PC-Side Variant Selection Harness

## Goal

Create a small PC/GCC test program that can parse WHDLoad archive filenames, group likely variants, score duplicate groups, and produce a selection report.

This should be done before touching WHDFetch's main download/extract flow.

Suggested folder:

```text
tools/variant_harness/
```

Suggested structure:

```text
tools/variant_harness/
  Makefile
  README.md
  data/
    Defs/
    Profiles/
    pack_types.ini
  src/
    main.c
    vh_csv.c
    vh_csv.h
    vh_fields.c
    vh_fields.h
    vh_parse.c
    vh_parse.h
    vh_profile.c
    vh_profile.h
    vh_score.c
    vh_score.h
    vh_group.c
    vh_group.h
    vh_report.c
    vh_report.h
  tests/
    filenames_basic.txt
    filenames_memory.txt
    filenames_language.txt
    filenames_grouping.txt
```

Use `vh_` prefixes in the harness to avoid collisions. Later, the code can be renamed or moved into `src/variant/`.

---

## Task 1.1: Create harness scaffold

Create:

```text
tools/variant_harness/Makefile
tools/variant_harness/README.md
tools/variant_harness/src/main.c
```

The first version should build and run:

```text
make
./variant_harness --help
```

Initial help output should list planned commands:

```text
--parse <filename>
--resolve <field> <token>
--select <listfile> --profile <profile>
--report <listfile> --profile <profile>
```

## Task 1.2: Copy selected data files into harness

Copy staged files into:

```text
tools/variant_harness/data/Defs/
tools/variant_harness/data/Profiles/
tools/variant_harness/data/pack_types.ini
```

Milestone 1 runtime data:

```text
Chipset.csv
Language.csv
Memory.csv
Video.csv
Media.csv
variant_tags.csv
Special.csv
```

`Special.csv` should be loaded/parsible but should not affect scoring yet.

## Task 1.3: Implement CSV loader

Create:

```text
vh_csv.c
vh_csv.h
```

Required behaviour:

- Load a CSV file with at least these logical columns:
  - numeric ID
  - token/name
  - description
  - optional marker such as `default`
- Allow duplicate numeric IDs for aliases.
- Lookup order:
  1. Exact token match
  2. Case-insensitive token match
- Preserve original token text for reports.
- Store canonical numeric ID for scoring.

Suggested API:

```c
int vh_csv_load(VhCsvFile *csv, const char *path);
void vh_csv_free(VhCsvFile *csv);
int vh_csv_lookup_token(VhCsvFile *csv, const char *token, VhCsvResult *out);
int vh_csv_get_default(VhCsvFile *csv, VhCsvResult *out);
```

## Task 1.4: Add CSV resolver command

Add:

```text
--resolve <field> <token>
```

Examples:

```text
./variant_harness --resolve Memory 1MbChip
./variant_harness --resolve Memory Slow
./variant_harness --resolve Language Fr
./variant_harness --resolve Chipset AGA
```

Expected output format:

```text
field: Memory
token: 1MbChip
matched: yes
id: 8
canonical: 1MbChip
description: May require 1MB chip RAM
```

The exact description depends on the CSV contents.

## Acceptance Criteria

- Harness builds with GCC.
- `--resolve` works for `Memory`, `Language`, `Chipset`, `Video`, and `Media`.
- Aliases sharing the same ID are supported.
- Missing tokens are reported cleanly.

---

# Milestone 2: Implement Filename Parsing in the Harness

## Goal

Parse WHDLoad archive filenames into fields needed by the selector.

Milestone 1 scoring fields:

```text
chipset
language
memory
video
media
```

Report-only field:

```text
special
```

Do not implement pretty names, name overrides, software house lookup, or catalogue enrichment.

---

## Task 2.1: Define parsed filename structure

Create:

```text
vh_parse.c
vh_parse.h
```

Suggested structure:

```c
#define VH_MAX_TOKENS_PER_FIELD 8
#define VH_MAX_TOKEN_TEXT 64
#define VH_MAX_TITLE_TEXT 128

typedef struct VhFieldToken {
    int id;
    char text[VH_MAX_TOKEN_TEXT];
} VhFieldToken;

typedef struct VhTokenList {
    VhFieldToken items[VH_MAX_TOKENS_PER_FIELD];
    int count;
} VhTokenList;

typedef struct VhParsedName {
    char archive_name[256];
    char title[VH_MAX_TITLE_TEXT];
    char group_key[VH_MAX_TITLE_TEXT];
    char version[32];

    VhTokenList chipset;
    VhTokenList language;
    VhTokenList memory;
    VhTokenList video;
    VhTokenList media;
    VhTokenList special;

    VhTokenList unknown;
} VhParsedName;
```

Adjust sizes later for Amiga memory constraints.

## Task 2.2: Implement basic archive-name sanitising

Required behaviour:

- Remove `.lha` or `.lzx` extension, case-insensitive.
- Do not modify the original `archive_name` field.
- Split remaining filename by underscore.
- First token is initially treated as title base.

Examples:

```text
AlienBreed_v1.3_AGA_Fr.lha
```

Initial tokens:

```text
AlienBreed
v1.3
AGA
Fr
```

## Task 2.3: Implement version token detection

Detect version tokens before CSV lookup.

Accept patterns such as:

```text
v1.0
v1.2
1.0
1.2
```

Store in `parsed.version`.

Do not use version for scoring yet.

## Task 2.4: Implement CSV-backed token recognition

For each non-title, non-version token, try resolving against fields in this order:

```text
Language
Chipset
Video
Media
Memory
Special
```

This order can be adjusted later if the GUI parser requires it.

Required behaviour:

- Add matched token to the relevant token list.
- Allow multiple language tokens.
- Allow multiple special tokens.
- Unknown tokens should be preserved in `unknown` for reports.

## Task 2.5: Implement compact multi-language token handling

Language tokens may be compact sequences of two-character language codes.

Examples:

```text
EnDe
FrDeEn
EnFrEs
```

Required behaviour:

- Try exact CSV lookup first.
- If exact lookup fails, split token into two-character chunks.
- Resolve each chunk against `Language.csv`.
- If all chunks resolve, add each language to the language token list.
- If any chunk fails, leave the original token for normal CSV/unknown processing.

Example:

```text
FrDeEn
```

Should become:

```text
Fr
De
En
```

If the CSV contains `FrDeEn` as its own canonical entry, exact CSV lookup wins unless you decide otherwise later.

## Task 2.6: Generate group key

For the first version, the group key should be conservative:

```text
group_key = lower-case normalised title token
```

Normalisation rules:

- Convert ASCII letters to lower case.
- Remove spaces.
- Optionally remove simple punctuation.
- Do not fuzzy-match.
- Do not apply name overrides.

Examples:

```text
AlienBreed_v1.3_AGA_Fr.lha       -> alienbreed
AlienBreed_v1.3_OCS_En.lha       -> alienbreed
AlienBreed2_v1.0_AGA_En.lha      -> alienbreed2
AlienBreedDemo_v1.0.lha          -> alienbreeddemo
```

## Task 2.7: Add parse command

Add:

```text
--parse <filename>
```

Example:

```text
./variant_harness --parse AlienBreed_v1.3_AGA_1MbChip_FrDeEn_PAL.lha
```

Example output:

```text
archive: AlienBreed_v1.3_AGA_1MbChip_FrDeEn_PAL.lha
title: AlienBreed
group_key: alienbreed
version: v1.3
chipset: AGA
memory: 1MbChip
language: Fr, De, En
video: PAL
media:
special:
unknown:
```

## Acceptance Criteria

- `--parse` recognises chipset, language, memory, video, media, and special tokens.
- Compact multi-language tokens are handled.
- Unknown tokens are preserved for reports.
- `group_key` is stable and conservative.

---

# Milestone 3: Implement Lightweight Grouping and Selection in the Harness

## Goal

Read a file containing archive names, group apparent variants, score duplicate groups, and choose one candidate per group.

---

## Task 3.1: Create candidate structure

Create:

```text
vh_group.c
vh_group.h
```

Suggested structures:

```c
typedef struct VhCandidate {
    char archive_name[256];
    char group_key[128];
    int original_index;
    int selected;
    int rejected;
    long score;
} VhCandidate;
```

For the harness, fixed-size fields are acceptable. Later, WHDFetch can optimise memory.

## Task 3.2: Load candidate list from text file

Input file format:

```text
One archive filename per line
Blank lines ignored
Lines beginning with # ignored
```

Command:

```text
--select <listfile> --profile <profile>
```

The loader should:

- Read all archive names.
- Parse each enough to derive `group_key`.
- Store original input order.

## Task 3.3: Sort candidates by group key

Implement sorting by:

```text
group_key ascending
original_index ascending
```

Then scan sorted candidates in runs:

```text
if run length == 1:
    select candidate
else:
    score duplicate group and select winner
```

Do not process selected items in sorted order. After selection, output selected candidates in original input order.

## Task 3.4: Create simple profile format

Create or adapt profile files under:

```text
tools/variant_harness/data/Profiles/
```

Suggested profile format:

```ini
[Filter.language]
include=Fr,En
exclude=

[Filter.chipset]
include=AGA,ECS,OCS
exclude=

[Filter.video]
include=PAL,NTSC
exclude=

[Filter.memory]
include=8MB,4MB,2MB,1MB,512KB,LowMem
exclude=

[Filter.media]
include=
exclude=

[Filter.special]
include=
exclude=

[Scoring]
weight.language=100
weight.chipset=80
weight.video=30
weight.memory=20
weight.media=10
weight.special=0
```

In milestone 1, `weight.special` must remain `0`.

## Task 3.5: Implement profile loader

Create:

```text
vh_profile.c
vh_profile.h
```

Required behaviour:

- Load INI-like sections.
- Load include/exclude lists for fields.
- Preserve include order as priority order.
- Resolve profile tokens through CSV where possible.
- If a profile token cannot be resolved, store it as unresolved and warn.

Suggested API:

```c
int vh_profile_load(VhProfile *profile, const char *path, VhFieldRegistry *fields);
void vh_profile_free(VhProfile *profile);
```

## Task 3.6: Implement scoring

Create:

```text
vh_score.c
vh_score.h
```

Scoring rules:

1. If field weight is 0, skip field.
2. If any candidate token matches the profile exclude list, reject candidate.
3. Find the best include-rank match among candidate tokens.
4. Lower include rank is better.
5. Field contribution:

```text
field_score = include_count - rank
contribution = field_score * weight
```

6. Sum contributions across fields.
7. Highest score wins.
8. Tie-break by lower original input index.

Special rule for milestone 1:

- `special` tokens may be parsed and reported.
- `weight.special` should default to 0.
- No special tag should influence selection unless explicitly testing milestone 2.

## Task 3.7: Add selected/skipped output

For `--select`, output only selected archive names by default:

```text
AlienBreed_v1.3_AGA_Fr.lha
AnotherGame_v1.0_OCS_En.lha
```

For `--report`, output grouped detail:

```text
Group: alienbreed
Selected:
  AlienBreed_v1.3_AGA_Fr.lha  score=12300
Skipped:
  AlienBreed_v1.3_AGA_En.lha  score=2300
  AlienBreed_v1.3_OCS_En.lha  score=1500
Recognised special tags:
  none
Unknown tokens:
  none
```

## Acceptance Criteria

- Harness can read a filename list and select one candidate per group.
- Groups with one candidate are preserved.
- Duplicate groups are scored.
- Output selection order follows original input order.
- `special` tags are visible in reports but do not affect milestone 1 scoring.

---

# Milestone 4: Create Test Data and Parity Checks

## Goal

Build confidence before integrating into WHDFetch.

---

## Task 4.1: Create core test files

Create:

```text
tools/variant_harness/tests/filenames_basic.txt
tools/variant_harness/tests/filenames_memory.txt
tools/variant_harness/tests/filenames_language.txt
tools/variant_harness/tests/filenames_special_report_only.txt
tools/variant_harness/tests/filenames_bad_grouping_watchlist.txt
```

## Task 4.2: Basic grouping tests

Example `filenames_basic.txt`:

```text
AlienBreed_v1.3_AGA_En.lha
AlienBreed_v1.3_OCS_En.lha
AlienBreed2_v1.0_AGA_En.lha
```

Expected behaviour:

- `AlienBreed` variants are grouped.
- `AlienBreed2` is separate.

## Task 4.3: Memory token tests

Include tokens such as:

```text
512k
512KB
1MB
1Mb
1MBChip
1MbChip
1.5MB
2MB
8MB
12MB
15MB
LowMem
SlowMem
Slow
Fast
Chip
```

Expected behaviour:

- Tokens resolve through `Memory.csv`.
- Aliases resolve to the intended IDs.
- No manual numeric memory assumptions are made.

## Task 4.4: Multi-language tests

Include:

```text
GameA_v1.0_Fr.lha
GameA_v1.0_En.lha
GameA_v1.0_FrDeEn.lha
GameB_v1.0_EnDe.lha
```

Expected behaviour:

- `FrDeEn` resolves into multiple language tokens if not an exact CSV token.
- A French profile prefers French-capable variants over English-only variants.

## Task 4.5: Special report-only tests

Include filenames with tags such as:

```text
Enhanced
UltimateEdition
Censored
Uncensored
NoMusic
NoSpeech
BETA
PreRelease
68020
68060
```

Expected behaviour in milestone 1:

- Tags are recognised if present in `Special.csv`.
- Tags appear in reports.
- Tags do not change the selected winner while `weight.special=0`.

## Task 4.6: Bad grouping watchlist

Create intentionally risky examples:

```text
Game_v1.0.lha
GameDemo_v1.0.lha
Game2_v1.0.lha
GameDataDisk_v1.0.lha
GameEditor_v1.0.lha
```

Expected behaviour:

- Conservative group key should not merge these unless their title token is identical.
- This file is for manual inspection and future regression checks.

## Task 4.7: Add `make test`

Extend the harness Makefile:

```text
make test
```

It should run representative commands and write outputs under:

```text
tools/variant_harness/test_output/
```

Do not require a complex test framework at first. Plain command execution and output files are enough.

## Acceptance Criteria

- Test files exist.
- `make test` runs without crashing.
- Output files show expected selected/skipped groups.
- Special tags are report-only in milestone 1.

---

# Milestone 5: Prepare WHDFetch Runtime Data Package

## Goal

Move the selected data files into WHDFetch in a clean runtime layout.

---

## Task 5.1: Create WHDFetch data folder

Inside WHDFetch, create:

```text
Data/
  Defs/
  Profiles/
```

Copy from the harness/staging area:

```text
Data/Defs/Chipset.csv
Data/Defs/Language.csv
Data/Defs/Memory.csv
Data/Defs/Video.csv
Data/Defs/Media.csv
Data/Defs/variant_tags.csv
Data/Defs/Special.csv
Data/pack_types.ini
```

## Task 5.2: Add starter profiles

Create:

```text
Data/Profiles/default.profile
Data/Profiles/english_aga_pal.profile
Data/Profiles/english_ocs_pal.profile
Data/Profiles/french_aga_pal.profile
```

Initial rule:

- `default.profile` should be safe and boring.
- `special` weight should be 0 in all milestone 1 profiles.

## Task 5.3: Add packaging notes

Update whichever project packaging/readme file controls release contents.

Ensure the new runtime data folder is copied with the executable.

Suggested note:

```text
Variant selection profiles and token definition files are stored in PROGDIR:Data/.
They are only used when SELECTBEST or variant_selection.enabled=true is active.
```

## Acceptance Criteria

- WHDFetch project contains `Data/Defs` and `Data/Profiles`.
- Release/package process includes the data files.
- Special tags are present but not scored by default.

---

# Milestone 6: Port Harness Code into WHDFetch as `src/variant/`

## Goal

Move the tested core code into WHDFetch without integrating it into the downloader yet.

---

## Task 6.1: Create source folder

Create:

```text
src/variant/
```

Suggested files:

```text
variant_csv.c
variant_csv.h
variant_fields.c
variant_fields.h
variant_parse.c
variant_parse.h
variant_profile.c
variant_profile.h
variant_score.c
variant_score.h
variant_group.c
variant_group.h
variant_report.c
variant_report.h
variant_select.c
variant_select.h
```

## Task 6.2: Rename harness symbols

Rename `vh_` prefixes to `variant_` or project-preferred names.

Examples:

```text
vh_csv_load       -> variant_csv_load
vh_parse_filename -> variant_parse_filename
vh_score_candidate -> variant_score_candidate
```

## Task 6.3: Make code WHDFetch/Amiga friendly

Review and adjust:

- Avoid `strdup` unless WHDFetch already provides a replacement.
- Avoid `snprintf` if not available in the Amiga compiler/runtime.
- Avoid variable declarations after statements if building as C89.
- Avoid `//` comments if strict C89.
- Replace PC path assumptions with WHDFetch path helpers if available.
- Keep all file handles closed on failure paths.
- Check allocation failures.

## Task 6.4: Add compile-only integration

Add the new files to the WHDFetch build system, but do not call them from the main flow yet.

The program should compile and behave exactly as before.

## Task 6.5: Optional developer-only test command

If useful, add a temporary hidden/developer command:

```text
TESTVARIANTPARSE=<filename>
```

or compile-gated test code such as:

```c
#ifdef WHDFETCH_VARIANT_TEST
#endif
```

This should not appear in the public manual unless kept intentionally.

## Acceptance Criteria

- WHDFetch builds with the new source files.
- Existing WHDFetch commands behave unchanged.
- No variant selection runs unless explicitly invoked by a test path.

---

# Milestone 7: Add WHDFetch CLI and INI Settings

## Goal

Add opt-in controls while keeping default behaviour unchanged.

---

## Task 7.1: Add configuration fields

Add runtime config fields similar to:

```c
BOOL variant_selection_enabled;
char variant_profile_name[64];
BOOL variant_report_enabled;
```

Default values:

```text
variant_selection_enabled = FALSE
variant_profile_name = "default"
variant_report_enabled = FALSE
```

## Task 7.2: Add INI keys

Suggested INI section:

```ini
[variant_selection]
enabled=false
active_profile=default
report_skipped_variants=false
```

Use lowercase `active_profile` consistently. Do not carry across the GUI mismatch between `ActiveProfile` and `active_profile`.

## Task 7.3: Add CLI arguments

Suggested CLI:

```text
SELECTBEST
PROFILE=<name>
VARIANTREPORT
```

Rules:

```text
SELECTBEST
```

Enables variant selection for the current run.

```text
PROFILE=<name>
```

Sets the active profile and implies `SELECTBEST`.

```text
VARIANTREPORT
```

Enables selected/skipped variant reporting.

## Task 7.4: Update help text lightly

Add concise help entries only. Avoid a large manual expansion at this stage.

Suggested wording:

```text
SELECTBEST       Select best matching variant per title using profile rules
PROFILE=<name>   Use profile from PROGDIR:Data/Profiles and enable SELECTBEST
VARIANTREPORT    Write a report of selected and skipped variants
```

## Acceptance Criteria

- INI values load correctly.
- CLI overrides INI values.
- `PROFILE=<name>` implies selection.
- Without these options, WHDFetch behaviour remains unchanged.

---

# Milestone 8: Integrate Variant Selection Before Download/Extract Processing

## Goal

Apply selection to the DAT-derived archive list before the existing download/extract pipeline processes entries.

This is the main integration point.

---

## Task 8.1: Locate current DAT iteration point

Find the code path where WHDFetch currently loops over DAT entries and decides whether to download/process each archive.

Document:

```text
- Function name
- Source file
- Structure used for DAT entries
- Where current filters are applied
- Where download/extract processing begins
```

Add notes to:

```text
docs/dev/variant_selection_integration_notes.md
```

## Task 8.2: Add selected-entry list abstraction

Create a way to build a list of entries to process.

Existing behaviour:

```text
DAT entry -> filter -> process immediately
```

New behaviour when selection is enabled:

```text
DAT entries -> existing hard filters -> candidate list -> selection -> process selected entries
```

When selection is disabled, keep the current streaming path if possible.

## Task 8.3: Apply existing hard filters before grouping

Before adding candidates to the variant selector, apply existing filters:

```text
SKIPAGA
SKIPCD
SKIPNTSC
SKIPNONENGLISH
```

These should remain hard excludes.

## Task 8.4: Build candidate list from filtered DAT entries

For each remaining DAT entry, store at least:

```text
archive name
pack id
letter path/destination info needed by existing processing
original DAT order
```

Avoid duplicating large structures if possible.

## Task 8.5: Run selector

Call variant-selection engine:

```text
variant_select_candidates(candidates, profile, options)
```

Expected output:

- Each candidate marked selected/skipped/rejected.
- Score recorded for report/debugging.
- Single-candidate groups selected automatically.
- Duplicate groups scored.

## Task 8.6: Process selected candidates through existing pipeline

Iterate candidates in original DAT order.

For each selected candidate:

- Call the same download/extract logic currently used.
- Do not duplicate download/extract code.
- Preserve existing `FORCEDOWNLOAD`, `FORCEEXTRACT`, `NODOWNLOADSKIP`, `.archive_index`, and `ArchiveName.txt` behaviour.

## Task 8.7: Handle all-skipped edge cases

If all candidates are rejected by profile or filters:

- Print a clear message.
- Do not treat it as a fatal error unless existing filter behaviour would do so.

Example:

```text
No archives matched the active variant-selection profile.
```

## Acceptance Criteria

- With selection disabled, WHDFetch works as before.
- With `SELECTBEST`, duplicate groups are reduced to one selected archive.
- Existing download/extract skip logic still works.
- Output order remains stable.

---

# Milestone 9: Add Variant Selection Report

## Goal

Make the feature auditable so bad grouping or surprising choices can be spotted easily.

---

## Task 9.1: Decide report location

Suggested location:

```text
PROGDIR:updates/variant_selection_YYYY-MM-DD_HH-MM-SS.txt
```

Alternative:

```text
Include a Variant Selection section in the existing session report.
```

A separate report is probably cleaner for the first version.

## Task 9.2: Report summary totals

Include:

```text
Profile used
Selected total
Skipped alternative total
Rejected by profile total
Single-candidate groups total
Duplicate groups total
```

## Task 9.3: Report duplicate groups only by default

To keep the report readable, include full detail only for groups with two or more candidates.

Example:

```text
Group: alienbreed
Selected:
  AlienBreed_v1.3_AGA_Fr.lha  score=12300
Skipped alternatives:
  AlienBreed_v1.3_AGA_En.lha  score=2300
  AlienBreed_v1.3_OCS_En.lha  score=1500
Special tags seen:
  Enhanced
Unknown tokens seen:
  none
```

## Task 9.4: Add special report-only visibility

When special tags are recognised, show them, even though milestone 1 does not score them.

Example:

```text
Special tags seen:
  Enhanced
  NoMusic
```

## Task 9.5: Add console summary

When `VARIANTREPORT` is active, print:

```text
Variant selection report written to PROGDIR:updates/variant_selection_YYYY-MM-DD_HH-MM-SS.txt
```

## Acceptance Criteria

- `VARIANTREPORT` writes a report.
- Report shows selected and skipped alternatives.
- Special tags appear as report-only data.
- Report does not become enormous during normal use.

---

# Milestone 10: Documentation Updates for First Release

## Goal

Document the feature without overloading the manual.

---

## Task 10.1: Update CLI reference

Add sections for:

```text
SELECTBEST
PROFILE=<name>
VARIANTREPORT
```

Suggested concise description:

```text
SELECTBEST enables profile-based variant selection. When several archives appear to be versions of the same title, whdfetch chooses the highest-scoring candidate according to the active profile and processes only that candidate.
```

Add compatibility note:

```text
This feature is disabled by default. Without SELECTBEST or variant_selection.enabled=true, whdfetch keeps its original behaviour.
```

## Task 10.2: Update sample INI

Add:

```ini
[variant_selection]
enabled=false
active_profile=default
report_skipped_variants=false
```

## Task 10.3: Add profile explanation

Briefly explain:

- Profiles live in `PROGDIR:Data/Profiles/`.
- Include order is preference order.
- Exclude means hard reject.
- Weights control field importance.
- Special tags are recognised but not used for scoring in the first release unless explicitly enabled later.

## Task 10.4: Add warning about grouping

Suggested wording:

```text
Variant selection is conservative and filename-based. It is intended to reduce duplicate chipset, language, video, and memory variants, not to build a full game catalogue.
```

## Acceptance Criteria

- CLI reference mentions the new arguments.
- Sample INI contains the new section.
- Documentation clearly says the feature is opt-in.

---

# Milestone 11: Milestone 2 Feature, Special Tags as Tie-Breakers

## Goal

After the first version is stable, allow `special` tags to influence scoring with a low weight.

Do not start this milestone until the core selector has been tested with real DAT files.

---

## Task 11.1: Review Special.csv contents

Classify special tags into loose categories:

```text
Compatibility or hardware requirement
Edition/content preference
Reduced-content warning
Release-state warning
Source/package label
Unknown/unsafe
```

Do not overfit. The point is to avoid scoring dangerous tags incorrectly.

## Task 11.2: Update starter profiles cautiously

Example:

```ini
[Filter.special]
include=Enhanced,UltimateEdition,Uncensored
exclude=Censored,NoMusic,NoSpeech,NoMovie

[Scoring]
weight.special=5
```

Keep `weight.special` low.

## Task 11.3: Keep CPU-style tags out of generic scoring if possible

Tags like these should eventually become compatibility fields, not subjective special preferences:

```text
68020
68030
68040
68060
A1000
A1200Version
A600Pack
```

For milestone 2, either:

- leave them report-only, or
- allow users to exclude them manually in profile files.

## Task 11.4: Add tests where special breaks a tie

Example group:

```text
Game_v1.0_AGA_En.lha
Game_v1.0_AGA_En_Enhanced.lha
```

Expected with special scoring enabled:

```text
Game_v1.0_AGA_En_Enhanced.lha
```

Example group:

```text
Game_v1.0_AGA_En.lha
Game_v1.0_AGA_En_NoMusic.lha
```

Expected with `NoMusic` excluded:

```text
Game_v1.0_AGA_En.lha
```

## Acceptance Criteria

- Special scoring is optional/profile-driven.
- Default behaviour remains conservative.
- Low-weight special preferences can break ties.
- Reduced-content tags can be excluded when profile says so.

---

# Future Milestones, Not For First Release

## Future: CPU and Machine Compatibility Fields

Split CPU/machine tags out of `Special.csv` into proper compatibility fields.

Possible fields:

```text
cpu
machine
hardware
```

Examples:

```text
68020
68030
68040
68060
A1000
A1200Version
A600Pack
```

These should support hard compatibility checks, not just scoring.

## Future: Profile Creator

Possible command:

```text
whdfetch MAKEPROFILE
```

Or separate tool:

```text
whdfetchconfig
```

Questions could include:

```text
Preferred language?
Amiga chipset?
PAL or NTSC?
Memory available?
Include CD32/CDTV titles?
Prefer complete versions over reduced-content versions?
```

Output:

```text
PROGDIR:Data/Profiles/custom.profile
```

Optional INI update:

```ini
[variant_selection]
enabled=true
active_profile=custom
```

## Future: Better Grouping

Only after conservative grouping is proven safe:

- Use selected name overrides.
- Add exact known equivalence mappings.
- Add safeguards for sequels, demos, editors, data disks, and compilations.

## Future: Catalogue/GUI Intelligence

Leave these out of WHDFetch unless there is a strong CLI use case:

```text
software houses
crack groups
demo groups
contributors
cover disks
magazines
official bundles
pretty display names
```

---

# Suggested Visual Studio Code Agent Prompt Seeds

These are starter prompts you can paste into an agent later.

## Prompt Seed 1: Create Backport Staging Folder

```text
Create a new folder named variant_backport_staging. Copy the GUI project's variant-selection related assets and source files into it without modifying the original GUI project. Use the task list in whdfetch_variant_selection_task_plan.md, Milestone 0. Create notes/backport_inventory.md and classify files into Use In Milestone 1, Use In Milestone 2, Reference Only, Ignore For WHDFetch, and Needs Review. Do not modify WHDFetch source code yet.
```

## Prompt Seed 2: Create PC Harness Scaffold

```text
Create tools/variant_harness with a Makefile, README.md, and src/main.c. Implement a basic command-line parser that supports --help and stubs for --resolve, --parse, --select, and --report. The code must compile with GCC and should stay close to portable C suitable for later Amiga C89 adaptation.
```

## Prompt Seed 3: Implement CSV Loader

```text
Implement the CSV loader for tools/variant_harness. It must load the definition CSV files from data/Defs, support duplicate numeric IDs as aliases, perform exact lookup first and case-insensitive fallback second, and expose a --resolve <field> <token> command. Use the requirements in Milestone 1, Tasks 1.3 and 1.4.
```

## Prompt Seed 4: Implement Filename Parser

```text
Implement filename parsing in the PC harness. Add vh_parse.c and vh_parse.h. The parser should remove .lha/.lzx extensions, split tokens by underscore, detect version tokens, recognise Language, Chipset, Video, Media, Memory, and Special tokens through the CSV loader, handle compact multi-language tokens such as FrDeEn, generate a conservative group_key, and preserve unknown tokens for reports. Add --parse <filename> output.
```

## Prompt Seed 5: Implement Grouping and Scoring

```text
Implement candidate grouping, profile loading, and weighted scoring in the PC harness. Read one archive filename per line from a list file, group by conservative group_key, score only duplicate groups using include/exclude profile rules and field weights, select one winner per group, and output selected archives in original input order. Special tags must be parsed and reported but must not affect scoring while weight.special=0.
```

## Prompt Seed 6: Add Tests

```text
Create the variant harness test files described in Milestone 4 and add a make test target. Tests should exercise basic grouping, memory tokens, compact multi-language tokens, special tags as report-only data, and risky grouping examples. The tests can write output files rather than using a full unit-test framework.
```

## Prompt Seed 7: Port Harness Into WHDFetch

```text
Port the tested variant harness code into WHDFetch under src/variant. Rename vh_ symbols to variant_ symbols, adjust the code for WHDFetch's coding style and Amiga/C89 constraints, add it to the build system, and ensure WHDFetch still builds and behaves unchanged when no variant-selection options are used.
```

## Prompt Seed 8: Integrate SELECTBEST

```text
Add the opt-in WHDFetch variant-selection controls SELECTBEST, PROFILE=<name>, and VARIANTREPORT. Add the [variant_selection] INI section with enabled=false, active_profile=default, and report_skipped_variants=false. Integrate variant selection before the existing download/extract pipeline so selected candidates are processed through the existing code path. Existing filters must act as hard excludes before grouping, and default behaviour must remain unchanged when selection is disabled.
```

---

# Definition of Done for First Public Variant-Selection Release

The first release is ready when:

```text
- WHDFetch behaves exactly as before unless SELECTBEST or variant_selection.enabled=true is active.
- SELECTBEST reduces duplicate variant groups to one selected archive.
- PROFILE=<name> loads profiles from PROGDIR:Data/Profiles/ and implies SELECTBEST.
- Existing hard filters still apply before scoring.
- Scoring supports language, chipset, video, memory, and media.
- Special tags are recognised and can appear in reports, but are not scored by default.
- VARIANTREPORT writes a readable selected/skipped report.
- The existing download/extract pipeline is reused without duplicating logic.
- Existing ArchiveName.txt and .archive_index skip behaviour still works.
- The PC harness can still be used for regression testing.
- Documentation says the feature is opt-in and filename-based.
```
