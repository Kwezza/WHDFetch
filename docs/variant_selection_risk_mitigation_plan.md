# Variant Selection Risk Mitigation Plan

Date: 2026-04-28
Scope: Pre-merge hardening plan for variant selection before WHDFetch core integration

## Goals

- Prevent false-positive grouping of distinct titles.
- Enforce consistent filter semantics across singleton and duplicate groups.
- Keep milestone 1 policy conservative for special tags.
- Improve selection auditability and regression safety.
- Estimate memory behavior before Amiga integration.

## Decision Summary

1. Existing WHDFetch CLI filters remain hard excludes before grouping.
2. Profile excludes apply to all candidates, including singleton groups.
3. Profile include ordering and weights only decide among remaining non-rejected candidates.
4. Special field remains conservative by default:
- weight.special = 0
- Filter.special include and exclude empty in default profile

## Issue 1: Group Key May Be Too Simple

Risk

- Distinct but related names may be incorrectly collapsed into one group.

Examples to protect

- AlienBreed
- AlienBreed2
- AlienBreedSE
- AlienBreedSpecialEdition
- Lotus2
- LotusTurboChallenge2
- SomeGameDemo
- SomeGameDataDisk

Plan

1. Add a dedicated false-positive watchlist dataset:
- tools/variant_harness/tests/grouping_false_positive_cases.txt
2. Ensure report output proves each risky case keeps a distinct group_key unless intentionally equivalent.
3. Add a merge gate: no false-positive merges in watchlist report.
4. Defer any fuzzy-title improvements until post-integration milestone, behind explicit tests.

Acceptance

- Report from grouping_false_positive_cases shows expected distinct groups for all listed risky pairs.

## Issue 2: Singleton Auto-Selection Must Be After Hard Filters

Risk

- A singleton candidate could be selected even though global hard filters should have removed it.

Plan

1. Add singleton hard-filter validation tests where only one candidate remains and should be excluded by hard filters.
2. Add integration note: in WHDFetch pipeline, apply SKIP_AGA, SKIP_CD, SKIP_NTSC, SKIP_NONENG before candidate grouping.
3. Add merge gate: singleton selection executes only on already hard-filtered candidate set.

Acceptance

- Singleton NTSC candidate is absent when SKIP_NTSC is active.
- Equivalent checks pass for AGA and CD filters.

## Issue 3: Profile Excludes Should Also Apply To Singleton Groups

Risk

- Singleton candidates may bypass profile exclude policy, causing inconsistent behavior.

Target behavior

- Profile excludes reject candidates regardless of group size.

Plan

1. Enforce evaluation order:
- Hard filters
- Profile excludes
- Grouping
- Include and weight scoring for winner selection among remaining candidates
2. Add singleton-exclude tests for multiple fields, including special where configured.
3. Add explicit reject reason in report output for excluded singleton candidates.

Acceptance

- Singleton candidate with excluded token is rejected.
- Duplicate and singleton groups use identical exclude semantics.

## Issue 4: Keep Special Conservative In Milestone 1

Risk

- Mixed semantic meaning in special tags can encode policy too early.

Plan

1. Keep default profile policy:
- weight.special = 0
- Filter.special include empty
- Filter.special exclude empty
2. Continue parsing and report visibility for special tags.
3. Introduce special weighting or excludes only in optional milestone 2 profiles.

Acceptance

- Special tags appear in report fields.
- Default scoring outcome is unchanged when only special tag differs.

## Pre-Merge Additions

## A) Selection Audit Report Hardening

Objective

- Ensure reports are sufficient to diagnose bad selections quickly.

Additions

1. Include these per-group report elements:
- group_key
- selected filename
- selected score
- skipped filenames
- skipped scores
- recognized tokens by field
- unknown tokens
- reject reason for rejected candidates
2. For groups with no selected candidate, emit explicit reason summary.

Acceptance

- Report for synthetic and real samples contains all listed fields.

## B) Real DAT Regression Set

Objective

- Freeze known-good behavior and detect accidental selection drift.

Additions

1. Add datasets:
- tools/variant_harness/tests/dat_samples/games_small_real.txt
- tools/variant_harness/tests/dat_samples/demos_small_real.txt
- tools/variant_harness/tests/dat_samples/language_cases.txt
- tools/variant_harness/tests/dat_samples/memory_cases.txt
- tools/variant_harness/tests/dat_samples/special_cases.txt
- tools/variant_harness/tests/dat_samples/grouping_false_positive_cases.txt
2. Add expected artifacts:
- tools/variant_harness/tests/expected/games_small_real.select
- tools/variant_harness/tests/expected/language_cases.report
- Additional expected files for each sample as they are stabilized
3. Extend make test to compare generated output to expected baselines.

Acceptance

- Regression suite passes with no unexpected diffs.
- Any behavior change requires expected file updates and review notes.

## C) Memory Usage Estimate Before Amiga Integration

Objective

- Reduce integration risk from PC-friendly memory assumptions.

Additions

1. Add harness dev output or report section capturing:
- candidate count
- average filename length
- candidate struct size estimate
- parsed token storage estimate
- peak memory approximation by pack
2. Add stress test with larger real DAT sample.
3. Define safe operational mode for Amiga integration:
- process one pack at a time
- optionally one DAT file at a time

Acceptance

- Memory estimate documented for representative and stress samples.
- Integration plan references bounded per-pack processing strategy.

Current measured results (Phase 3)

- Representative sample (games_small_real, 10 candidates):
	- candidate_count: 10
	- average_filename_length: 27.90
	- candidate_struct_size_bytes: 4908
	- parsed_token_count: 16
	- parsed_token_storage_estimate_bytes: 1088
	- peak_memory_estimate_bytes: 50208
	- Evidence: [tools/variant_harness/test_output/phase3_games_memory_report.txt](tools/variant_harness/test_output/phase3_games_memory_report.txt)
- Stress sample (games_stress_large, 150 candidates):
	- candidate_count: 150
	- average_filename_length: 37.03
	- candidate_struct_size_bytes: 4908
	- parsed_token_count: 600
	- parsed_token_storage_estimate_bytes: 40800
	- peak_memory_estimate_bytes: 777600
	- Evidence: [tools/variant_harness/test_output/phase3_games_stress_report.txt](tools/variant_harness/test_output/phase3_games_stress_report.txt)

## Execution Phases

Phase 1: Test and policy hardening

Status: in progress (major items completed)

- [x] Add false-positive grouping cases.
	Evidence:
	[tools/variant_harness/tests/grouping_false_positive_cases.txt](tools/variant_harness/tests/grouping_false_positive_cases.txt)
	[tools/variant_harness/test_output/grouping_false_positive_select.txt](tools/variant_harness/test_output/grouping_false_positive_select.txt)
	[tools/variant_harness/test_output/grouping_false_positive_report.txt](tools/variant_harness/test_output/grouping_false_positive_report.txt)
- [x] Add singleton profile-exclude tests and enforce excludes for singleton groups.
	Evidence:
	[tools/variant_harness/tests/singleton_profile_exclude_cases.txt](tools/variant_harness/tests/singleton_profile_exclude_cases.txt)
	[tools/variant_harness/tests/profiles/phase1_singleton_exclude.profile](tools/variant_harness/tests/profiles/phase1_singleton_exclude.profile)
	[tools/variant_harness/test_output/singleton_exclude_select.txt](tools/variant_harness/test_output/singleton_exclude_select.txt)
	[tools/variant_harness/test_output/singleton_exclude_report.txt](tools/variant_harness/test_output/singleton_exclude_report.txt)
- [ ] Add singleton hard-filter validation for SKIP_AGA/SKIP_CD/SKIP_NTSC/SKIP_NONENG semantics.
	Note: this is deferred to WHDFetch integration path where those hard filters exist natively.
- [x] Keep special conservative defaults for milestone 1 policy.
	Evidence:
	[tools/variant_harness/data/Profiles/default.profile](tools/variant_harness/data/Profiles/default.profile)
	[tools/variant_harness/test_output/special_report_only.txt](tools/variant_harness/test_output/special_report_only.txt)
- [x] Add reject reasons to report output.
	Evidence:
	[tools/variant_harness/src/vh_score.c](tools/variant_harness/src/vh_score.c)
	[tools/variant_harness/src/vh_group.c](tools/variant_harness/src/vh_group.c)
	[tools/variant_harness/test_output/singleton_exclude_report.txt](tools/variant_harness/test_output/singleton_exclude_report.txt)

Phase 2: Regression infrastructure

Status: in progress (baseline framework now active)

- [x] Add dat_samples and expected baselines.
	Evidence:
	[tools/variant_harness/tests/dat_samples/games_small_real.txt](tools/variant_harness/tests/dat_samples/games_small_real.txt)
	[tools/variant_harness/tests/dat_samples/demos_small_real.txt](tools/variant_harness/tests/dat_samples/demos_small_real.txt)
	[tools/variant_harness/tests/dat_samples/language_cases.txt](tools/variant_harness/tests/dat_samples/language_cases.txt)
	[tools/variant_harness/tests/expected/games_small_real.select](tools/variant_harness/tests/expected/games_small_real.select)
	[tools/variant_harness/tests/expected/demos_small_real.select](tools/variant_harness/tests/expected/demos_small_real.select)
	[tools/variant_harness/tests/expected/language_cases.report](tools/variant_harness/tests/expected/language_cases.report)
- [x] Extend make test to run comparisons.
	Evidence:
	[tools/variant_harness/tests/run_milestone4_tests.ps1](tools/variant_harness/tests/run_milestone4_tests.ps1)
	[tools/variant_harness/test_output/phase2_games_small_real.select](tools/variant_harness/test_output/phase2_games_small_real.select)
	[tools/variant_harness/test_output/phase2_demos_small_real.select](tools/variant_harness/test_output/phase2_demos_small_real.select)
	[tools/variant_harness/test_output/phase2_language_cases.report](tools/variant_harness/test_output/phase2_language_cases.report)

Phase 3: Amiga readiness

Status: completed

- [x] Add memory estimate output.
	Evidence:
	[tools/variant_harness/src/vh_group.c](tools/variant_harness/src/vh_group.c)
	[tools/variant_harness/tests/dat_samples/games_stress_large.txt](tools/variant_harness/tests/dat_samples/games_stress_large.txt)
	[tools/variant_harness/tests/run_milestone4_tests.ps1](tools/variant_harness/tests/run_milestone4_tests.ps1)
	[tools/variant_harness/test_output/phase3_games_memory_report.txt](tools/variant_harness/test_output/phase3_games_memory_report.txt)
	[tools/variant_harness/test_output/phase3_games_stress_report.txt](tools/variant_harness/test_output/phase3_games_stress_report.txt)
- [x] Confirm bounded per-pack processing strategy for WHDFetch integration.
	Decision:
	- Keep variant selection processing scoped to one pack list input at a time.
	- For WHDFetch integration, process one DAT file at a time within a pack when needed to cap memory growth.
	Evidence:
	[docs/variant_selection_filtering_and_compatibility.md](docs/variant_selection_filtering_and_compatibility.md)

## Merge Gates

1. False-positive grouping watchlist passes.
2. Singleton hard-filter and profile-exclude semantics pass.
3. Special remains report-visible and non-scoring by default.
4. Selection audit report includes reject reasons.
5. Regression samples compare cleanly against expected outputs.
6. Memory estimate documented with bounded processing recommendation.
