# WHDFetch Variant Selection: Current Filtering, Weights, and Compatibility

## Short Answer About TLV

The full advanced TLV/catalog system from the previous program is **not** being ported into WHDFetch for this release path.

What is being used now is a simplified, filename-driven selector that borrows only the useful token knowledge:

- CSV token definitions (Language, Chipset, Video, Media, Memory, Special)
- compact language token handling (for example `FrDeEn`)
- profile-based include/exclude and weighted scoring

It does **not** include full catalogue enrichment features (pretty naming, broad metadata graph joins, fuzzy matching, etc.).

## What Is Implemented So Far (Harness)

Implementation lives under `tools/variant_harness/`.

### Milestone 1 delivered

- PC-side harness with GCC build.
- `--resolve <field> <token>` implemented.
- CSV lookup behavior:
  - exact match first
  - case-insensitive fallback
  - duplicate numeric IDs allowed (aliases)

### Milestone 2 delivered

- `--parse <filename>` implemented.
- Parser behavior:
  - remove `.lha` / `.lzx`
  - split by `_`
  - detect version tokens (`v1.3`, `1.3`)
  - resolve tokens in order: Language, Chipset, Video, Media, Memory, Special
  - compact language expansion (`FrDeEn` -> `Fr`, `De`, `En`) when exact token is absent
  - preserve unknown tokens
  - generate conservative `group_key` from lower-case alphanumeric title token

### Milestone 3 delivered

- `--select <listfile> --profile <name|path>` implemented.
- `--report <listfile> --profile <name|path>` implemented.
- Group and selection behavior:
  - one archive filename per input line
  - blank/comment lines ignored
  - candidates grouped by `group_key`
  - singleton groups auto-selected
  - duplicate groups scored and reduced to one winner
  - selected output stays in original input order

### Milestone 4 delivered

- test data and parity checks added under `tools/variant_harness/tests/`
- `make test` added in harness Makefile
- output artifacts written to `tools/variant_harness/test_output/`

## How Filtering and Weights Work Now

## 1) Hard reject first (exclude lists)

For each field, if any candidate token matches that field's profile `exclude` list, that candidate is rejected.

This reject is immediate for that candidate.

## 2) Weight zero means no scoring contribution

If a field weight is `0`, that field contributes no score.

Current conservative rule: `weight.special = 0` by default.

## 3) Include list rank scoring

For each field with nonzero weight:

- Find the best include-rank match among the candidate's tokens.
- Lower rank is better (earlier in include list = stronger preference).
- Field score formula:

`field_score = include_count - rank`

`contribution = field_score * weight`

- Sum contributions across fields.

## 4) Winner selection and tie-break

- Highest total score wins in each duplicate group.
- If scores tie, lower original input index wins.

## 5) Report behavior

`--report` shows grouped detail:

- selected entry and score
- skipped entries and scores
- recognized `special` tags (report visibility)
- unknown tokens

## Why This Is Simpler Than Full TLV

The simplified system is intentionally filename-first and conservative:

- no fuzzy title matching
- no pretty-name/catalog enrichment
- no software house / contributor graph scoring
- no broad cross-file metadata joins

This is safer for first WHDFetch integration because incorrect grouping is the primary risk.

## How This Will Integrate Into WHDFetch

Planned integration follows milestone sequence in `docs/whdfetch_variant_selection_task_plan.md`.

### Data packaging (Milestone 5)

WHDFetch runtime data layout will contain:

- `Data/Defs/` CSV files used by parser/scorer
- `Data/Profiles/` profile files
- optional `Data/pack_types.ini` only if needed by selector logic

Important: WHDFetch network/pack discovery config remains in `Bin/Amiga/whdfetch.ini.sample` and runtime `whdfetch.ini`.

### Code port (Milestone 6)

Harness modules are moved into `src/variant/` with Amiga/C89 compatibility adjustments.

### Opt-in config and CLI (Milestone 7)

Variant selection is controlled by new settings/flags such as:

- `SELECTBEST`
- `PROFILE=<name>`
- `VARIANTREPORT`

### Main pipeline insertion (Milestone 8)

Integration point is before download/extract processing:

1. DAT entries loaded
2. existing hard filters applied
3. candidates grouped/scored when selection is enabled
4. selected entries passed through existing download/extract logic unchanged

## Backward Compatibility Contract

Backward compatibility is preserved by making selector behavior opt-in.

## Default behavior (unchanged)

If variant selection is not enabled:

- WHDFetch behaves exactly as today.
- Existing pack selection, download, extract, skip logic remains unchanged.

## Existing hard filters remain authoritative

Current hard filters still apply before weighted selection when selection is enabled:

- `SKIP_AGA`
- `SKIP_CD`
- `SKIP_NTSC`
- `SKIP_NONENG`

## Existing extraction/download safeguards remain intact

Selection does not replace these mechanisms:

- `ArchiveName.txt` marker behavior
- `.archive_index` caching and skip behavior
- `FORCEDOWNLOAD` / `FORCEEXTRACT` / related flags

## Special tags stay conservative in first release

- parsed and visible in reports
- not scored by default (`weight.special=0`)

## Config source-of-truth boundaries

To avoid dead config and duplication:

- URL/link scanning and pack download settings stay in WHDFetch INI (`whdfetch.ini`).
- Variant parser/scorer preferences live in `Data/Defs` + `Data/Profiles`.
- Do not duplicate website query snippets from external tooling if WHDFetch already has equivalent pack/URL settings.

## Current Practical Status

As of now, the harness has validated:

- token resolution
- parsing
- grouping
- weighted selection
- report generation
- operation on real DAT-derived lists

The next work is packaging and WHDFetch-side integration while preserving default behavior and existing CLI/INI semantics.
