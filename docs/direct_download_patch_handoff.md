# Direct Download Patch Handoff

Date: 2026-03-15

## Summary

This patch removes the runtime dependency on `wget` for individual WHDLoad archive downloads.
Archive downloads now use the existing in-project HTTP downloader (`src/download/*`) that was
already used for index and DAT ZIP downloads.

## What Changed

1. Replaced `.lha` archive download transport in `src/main.c`:
	- Old path: shell command + `SystemTagList("wget ...")`
	- New path: `ad_download_http_file(downloadUrl, fileName, FALSE)`
2. Renamed archive download function:
	- `execute_wget_download_command(...)` -> `execute_archive_download_command(...)`
3. Removed hard startup checks for `c:wget`:
	- Startup no longer exits if `c:wget` is missing.
	- Wget version display was removed from startup information.
4. Removed stale wget shell plumbing:
	- Removed `silent_wget_command` global/extern.
5. Added direct error reporting for archive download failures:
	- Calls `ad_print_download_error(...)`
	- Logs with `LOG_DOWNLOAD`
6. Neutral naming follow-up:
	- `no_wget_output` renamed to `verbose_output` in code (`download_option` + uses).

## Files Touched

- `src/main.c`
- `src/main.h`
- `README.md`
- `PROJECT_OVERVIEW.md`
- `.github/copilot-instructions.md`
- `docs/ProjectInfo.md`
- `docs/dat_discovery_and_ini_plan.md`
- `docs/dat_diff_and_report_plan.md`
- `docs/extraction_plan.md`

## Behavior Notes

1. Archive downloads no longer require `c:wget`.
2. Default mode is quiet for unzip output; `VERBOSE` enables detailed unzip output.
3. Archive download progress remains visible (direct downloader called with `silent=FALSE`).
4. Existing skip behavior is unchanged:
	- archive-file exists check
	- marker/index pre-download skip checks
5. Post-download extraction flow remains unchanged.

## Build Status

Verified after patch:

```powershell
make MEMTRACK=1
```

Build completed successfully and produced `Bin/Amiga/WHDDownloader`.

## Tomorrow Test Checklist (Manual)

1. Remove/rename `c:wget` on the Amiga test setup.
2. Run `WHDDownloader GAME`:
	- Confirm startup does not fail due to missing wget.
	- Confirm archives download successfully.
3. Re-run same command:
	- Confirm skip counters increment for existing/extracted titles.
4. Run `WHDDownloader GAME FORCEDOWNLOAD`:
	- Confirm downloads happen even when markers exist.
5. Run `whdfetch DOWNLOADGAMES VERBOSE`:
	- Confirm detailed unzip output is shown.
	- Confirm archive download progress still appears.
6. Trigger a known failure (bad URL/network down):
	- Confirm readable direct-download error output.
	- Confirm error log entry in `PROGDIR:logs/errors_*.log`.
7. Run a full flow check `WHDDownloader DOWNLOADALL`:
	- Confirm download + extraction flow remains stable.

## Risk/Regression Watch

1. Any scripts or docs expecting wget-specific messaging may now be stale.
2. `VERBOSE` semantics are unzip-focused behavior.
3. If later desired, a second patch can expose separate archive-download verbosity control.

## Quick Rollback Point

If needed, rollback target is the archive transport function in `src/main.c`:

- `execute_archive_download_command(...)`

That function now encapsulates the transport swap, so reverting only that function can restore
previous behavior quickly.
