# whdfetch

AmigaOS CLI tool for fetching Retroplay WHDLoad packs.

It downloads the Retroplay pack index, finds the current ZIP packs, extracts the DAT/XML lists,
walks every archive entry in those lists, applies your skip filters, downloads matching `.lha`
or `.lzx` archives into `GameFiles/`, and can extract them immediately.

No GUI. Run it from Shell.

## What it does

- Downloads Retroplay WHDLoad pack metadata.
- Handles five pack groups: Games, Games Beta, Demos, Demos Beta, and Magazines.
- Filters titles before download: AGA, CD, NTSC, non-English.
- Downloads individual game/demo archives into letter-based drawers under `GameFiles/`.
- Extracts `.lha` archives with `c:lha` and `.lzx` archives with `c:unlzx` when installed.
- Writes `ArchiveName.txt` into extracted folders so later runs can skip work safely.
- Maintains a per-letter `.archive_index` cache so it does not need to rescan slow drawers every run.
- Can keep or delete archives after extraction.
- Can reuse already downloaded archives instead of fetching them again.
- Can force re-download or re-extraction when you need a clean refresh.

## Requirements

- AmigaOS 3.0 or newer.
- Roadshow / `bsdsocket.library` for HTTP downloads.
- `c:lha` if extraction is enabled.
- `c:unlzx` only if you want `.lzx` extraction. Without it, `.lzx` files are skipped with a warning.
- Enough disk space for `temp/`, `GameFiles/`, extracted folders, and logs.

## How it works

Normal run:

1. Download `index.html` from the Retroplay WHDLoad pack site.
2. Parse the page and find the ZIP files for the selected packs.
3. Download those ZIP files into `temp/Zip files/`.
4. Extract the DAT/XML files into `temp/Dat files/`.
5. Read each `<rom name="...">` entry from the DAT data.
6. Parse archive filename metadata and apply your filters.
7. Check whether the archive already exists locally.
8. Check whether the title was already extracted by looking at `ArchiveName.txt` and `.archive_index`.
9. Download missing archives into `GameFiles/<pack>/<letter>/`.
10. If extraction is enabled, extract the archive, write `ArchiveName.txt`, update `.archive_index`, apply icons, and optionally delete the archive.

Extraction-only run:

1. Skip network work.
2. Read the existing local DAT data for the selected packs.
3. Look for matching `.lha` or `.lzx` files already present in `GameFiles/`.
4. Extract those archives using the normal extraction path.

## Runtime layout

Main directories used by the program:

- `GameFiles/Games/`
- `GameFiles/Games Beta/`
- `GameFiles/Demos/`
- `GameFiles/Demos Beta/`
- `GameFiles/Magazines/`
- `temp/index.html`
- `temp/Zip files/`
- `temp/Dat files/`
- `logs/`
- `updates/`

Archives are stored by first letter:

```text
GameFiles/Games/A/AnotherWorld_v1.3.lha
GameFiles/Games/B/BatmanTheMovie_v1.2.lha
```

Extracted folders contain a marker file:

```text
ArchiveName.txt
```

Line 1 is the pack name. Line 2 is the exact archive filename. If line 2 matches on a later run,
extraction can be skipped.

Each letter drawer also uses:

```text
.archive_index
```

This is a TAB-separated cache of:

```text
archive filename<TAB>real extracted folder name
```

That cache is loaded on demand, updated after successful extraction, and flushed during shutdown.

## Pack commands

Arguments are case-insensitive. Order does not matter.

If no pack is selected on the CLI or in the INI, the program shows help and exits.

### `DOWNLOADGAMES`

Process the Games pack.

### `DOWNLOADBETAGAMES`

Process the Games Beta pack.

### `DOWNLOADDEMOS`

Process the Demos pack.

### `DOWNLOADBETADEMOS`

Process the Demos Beta pack.

### `DOWNLOADMAGS`

Process the Magazines pack.

### `DOWNLOADALL`

Process all five packs.

### `HELP`

Show built-in help and exit.

## Filter arguments

Filters stack. A file must pass every active filter.

### `SKIPAGA`

Skip AGA titles.

### `SKIPCD`

Skip CD32, CDTV, and other CD-based entries.

### `SKIPNTSC`

Skip NTSC entries.

### `SKIPNONENGLISH`

Skip titles that are not marked as English.

## Extraction and storage arguments

### `NOEXTRACT`

Download archives only. Do not extract them. No `ArchiveName.txt` is written. No `.archive_index`
entry is added from a new extraction. No archive deletion happens.

Use this when you want to build an archive cache first and extract later.

### `EXTRACTONLY`

Extract archives that are already present under `GameFiles/`. This forces extraction on, even if
`NOEXTRACT` is also present.

This is not a pack selector. You still need `DOWNLOADGAMES`, `DOWNLOADALL`, or another pack choice.

### `EXTRACTTO=<path>`

Extract to a separate destination instead of next to the archive files.

Pack and letter drawers are still created below the given path:

```text
<path>/<pack>/<letter>/<game folder>/
```

If the value is empty, in-place extraction is used again.

### `KEEPARCHIVES`

Keep `.lha` and `.lzx` files after successful extraction.

### `DELETEARCHIVES`

Delete `.lha` and `.lzx` files after successful extraction. This is the normal default unless your
INI says otherwise.

### `FORCEEXTRACT`

Ignore the `ArchiveName.txt` extraction match and extract again.

Use this if you want to rebuild extracted folders, reapply icons, or recover from a bad extraction.

### `NOICONS`

Do not copy custom drawer icons onto extracted folders.

## Download skip and refresh arguments

There are two skip layers:

1. Local archive exists already.
2. Extracted folder already matches the archive filename via `ArchiveName.txt` and `.archive_index`.

### `NODOWNLOADSKIP`

Disable the extracted-marker skip. If the archive file is missing locally, it will be downloaded even
if the title was already extracted before.

This does not bypass the local archive file check.

### `FORCEDOWNLOAD`

Bypass both skip layers. Download the archive again even if it already exists locally or already has
an extracted-marker match.

Use this when you suspect a bad local archive or want a full refresh from the server.

## Output, checks, and maintenance arguments

### `VERBOSE`

Show more detail from ZIP extraction work. Default runs are quieter.

### `NOSKIPREPORT`

Suppress per-file skip messages for entries that are skipped because they already exist or were
already extracted.

### `DISABLECOUNTERS`

Disable queued counters and the pre-count pass.

### `CRCCHECK`

Enable CRC verification for downloaded archives when CRC data exists in the DAT metadata.

The program prints `CRC verification: ON` or `OFF` at startup.

### `TIMEOUT=<seconds>`

Set the HTTP timeout for downloads.

- Minimum: 5 seconds.
- Maximum: 60 seconds.
- Default: 30 seconds.

The value is clamped into range.

### `PURGEARCHIVES`

Delete downloaded `.lha` and `.lzx` files under `GameFiles/` after a confirmation prompt.

This does not delete extracted game drawers.

## Examples

Download the Games pack:

```text
whdfetch DOWNLOADGAMES
```

Download all packs, skipping AGA:

```text
whdfetch DOWNLOADALL SKIPAGA
```

Download games, extract to another volume, keep the archives:

```text
whdfetch DOWNLOADGAMES EXTRACTTO=Games: KEEPARCHIVES
```

Extract already downloaded beta game archives without re-downloading:

```text
whdfetch DOWNLOADBETAGAMES EXTRACTONLY EXTRACTTO=Games: KEEPARCHIVES
```

Force re-extraction:

```text
whdfetch DOWNLOADGAMES FORCEEXTRACT
```

Force a clean re-download:

```text
whdfetch DOWNLOADGAMES FORCEDOWNLOAD
```

Enable CRC checking and raise the network timeout:

```text
whdfetch DOWNLOADALL CRCCHECK TIMEOUT=45
```

## INI file

`PROGDIR:whdfetch.ini` is optional.

It can hold default pack choices, extraction settings, filter flags, icon settings, timeout, CRC mode,
and counter mode. CLI arguments override the INI for the current run.

Legacy fallback is still supported:

```text
PROGDIR:WHDDownloader.ini
```

Relevant global keys include:

- `download_website`
- `timeout_seconds`
- `extract_archives`
- `skip_existing_extractions`
- `skip_download_if_extracted`
- `verbose_output`
- `extract_path`
- `delete_archives_after_extract`
- `use_custom_icons`
- `unsnapshot_icons`
- `disable_counters`
- `crccheck` or `crc_check`

Filter keys:

- `skip_aga`
- `skip_cd`
- `skip_ntsc`
- `skip_non_english`

Pack sections:

- `[pack.games]`
- `[pack.games_beta]`
- `[pack.demos]`
- `[pack.demos_beta]`
- `[pack.magazines]`

## Notes

- `EXTRACTONLY` works from archives and DAT data already on disk.
- `NOEXTRACT` is useful if you want to use another extraction tool later.
- `.lzx` support depends on `c:unlzx` being installed.
- Every normal exit path flushes the archive index cache and shuts down the download system cleanly.
- Logs are written to `PROGDIR:logs/`.
- Session update summaries are written to `PROGDIR:updates/` when there is reportable activity.