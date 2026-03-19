# Archive Audit + Sync Plan (Future Release)

## Status

- Deferred to a future release.
- Not included in current release scope (release is prioritized for near-term delivery).

## Why Deferred

- Current release timing is more important than adding a large new feature surface.
- `src/main.c` is already very large; this feature should not be implemented inline there.
- Audit/sync introduces policy and UX decisions (restore behavior, CRC strategy, long output handling) that deserve focused implementation/testing.

## Architecture Constraint (Important)

Keep `main.c` wiring-only for this feature.

`main.c` should only:
1. Parse flags/INI settings
2. Validate combinations
3. Call one entrypoint (for example `audit_sync_run(...)`)
4. Handle return code + existing shutdown flow

All operational logic must live in a dedicated module, e.g.:
- `src/audit_sync/audit_sync.h`
- `src/audit_sync/audit_sync.c`
- Optional split files if needed (`audit_state.c`, `audit_report.c`, etc.)

## Proposed Future CLI/INI Surface

### CLI flags

- `AUDITARCHIVES` — report-only audit of local archive/extraction state
- `SYNCARCHIVES` — download missing/corrupt/incomplete local archives
- `SYNCEXTRACT` — optional extraction phase for sync results
- `RESTOREMISSING` — explicit opt-in restore of missing extracted folders

### INI keys (`[global]`, all default false)

- `audit_archives=true|false`
- `sync_archives=true|false`
- `sync_extract=true|false`
- `restore_missing=true|false`

## Behavioral Policy (Agreed)

- Default is conservative: report-only for missing extracted folders.
- Missing extracted folders are **not** restored unless explicitly requested (`RESTOREMISSING`).
- Long result lists should not flood Amiga CLI output:
  - Terminal: summary counts + capped sample items
  - Full detail: written to report file

## Core State Model (Per DAT Entry)

Each archive should be classified into one of these states:

1. Healthy
   - Archive exists locally
   - Extraction marker/index is consistent
   - CRC OK when checked

2. Archive missing, extraction present
   - Valid extracted folder/marker exists
   - Local archive file is absent

3. Archive present, CRC failed
   - File exists but integrity check fails

4. Extraction drift
   - Index/marker references missing folder
   - Or `ArchiveName.txt` mismatch/missing where expected

5. Missing both archive and extraction

6. Incomplete download artifact
   - `.downloading` marker present

## Reuse Existing Building Blocks

Use existing project functions to avoid duplicate logic:

- Extraction state/index checks in `src/extract/extract.c`
- Marker metadata (`ArchiveName.txt`) checks
- CRC verification via existing CRC helper API
- Existing report infrastructure in `src/report/report.c` / `src/report/report.h`

## Future Implementation Phases

## Phase 1 — Scaffolding

- Add options to `download_option` in `src/main.h`
- Add parsing in `main.c` and `ini_parser.c`
- Add minimal dispatch call from `main.c`

## Phase 2 — Audit/Sync Module

- Create `src/audit_sync/` module with single public run entrypoint
- Implement shared evaluator for state classification
- Keep module non-destructive in evaluator path

## Phase 3 — Audit Mode

- Implement `AUDITARCHIVES` as report-only
- Produce terminal summary + detailed report file output

## Phase 4 — Sync Mode

- Implement `SYNCARCHIVES` for missing/corrupt/incomplete archives
- Implement optional `SYNCEXTRACT`
- Implement explicit `RESTOREMISSING` behavior only when requested

## Phase 5 — Reporting Extensions

Add audit/sync sections while preserving existing report compatibility:
- Audit summary totals
- Missing archives
- CRC failures
- Extraction drift
- Restores performed/skipped by policy

## Phase 6 — Verification

Build checks:
- `make CONSOLE=1`
- `make CONSOLE=1 MEMTRACK=1`

Runtime checks in WinUAE:
- `AUDITARCHIVES` (with and without `CRCCHECK`)
- `SYNCARCHIVES`
- `SYNCARCHIVES SYNCEXTRACT`
- `SYNCARCHIVES SYNCEXTRACT RESTOREMISSING`
- Confirm no restore occurs unless `RESTOREMISSING` is present
- Confirm existing non-audit workflows remain unchanged

## Explicit Current-Release Non-Goals

- No new runtime behavior changes in current release.
- No large additions to `main.c`.
- No automatic restore policy added now.

## Notes for Future Dev Session

- Keep this feature isolated and testable.
- Prefer small, focused source files under `src/audit_sync/`.
- Maintain current shutdown contract (`do_shutdown()`) and cleanup ordering.
- Keep UX terminal-first and report-file-driven for long outputs.
