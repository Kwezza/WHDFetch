```
Short:        WHDLoad pack downloader, extractor, organiser
Author:       See README.md
Type:         util/cli
Version:      See build header
Architecture: m68k-amigaos >= 3.0
Requires:     Roadshow bsdsocket.library v4+, c:lha
```

---

Fetches WHDLoad game, demo, and magazine packs from the Retroplay FTP site, extracts
them into a letter-organised drawer structure, and maintains a skip cache so titles
already on disk are never re-processed. Runs from a Shell window. Does not run from
Workbench.

The key design point: run the same command every week and it does the right thing.
Archives already extracted are skipped instantly via a per-letter index cache. Only
titles that are genuinely new — or that have been updated by Retroplay since your last
run — are downloaded and extracted. Nothing else is touched. There is no manual tracking
or list to maintain; whdfetch handles it automatically on every run.

---

## Requirements

- AmigaOS 3.0 or later (3.1+ recommended)
- Roadshow TCP/IP stack, `bsdsocket.library v4+`
- `c:lha` — required for `.lha` archive extraction
- `c:unlzx` — optional; `.lzx` archives are silently skipped if not present
- Fast RAM recommended — the tool allocates from `MEMF_ANY`, Fast preferred

---

## Installation

1. Copy `whdfetch` and `whdfetch.ini` to a drawer on your Amiga.
2. Ensure Roadshow is running.
3. Ensure `c:lha` is present.
4. Edit `whdfetch.ini` as needed — or leave it and control everything via CLI arguments.

No Installer script is provided. No assigns required.

The program reads `PROGDIR:whdfetch.ini` at startup. If only the legacy `PROGDIR:WHDDownloader.ini`
is found, that file is used as a fallback. CLI arguments override both.

---

## Usage

```
whdfetch [command...] [option...]
```

Running `whdfetch` without arguments displays the help screen and exits.

Arguments are case-insensitive and can appear in any order. At least one pack must be
selected — either via a pack command or via the INI file — for the program to do anything.

---

### Selecting packs

```
whdfetch DOWNLOADGAMES           Games pack
whdfetch DOWNLOADBETAGAMES       Games (Beta & Unofficial) pack
whdfetch DOWNLOADDEMOS           Demos pack
whdfetch DOWNLOADBETADEMOS       Demos (Beta & Unofficial) pack
whdfetch DOWNLOADMAGS            Magazines pack
whdfetch DOWNLOADALL             All five packs at once
```

Archives are stored under `GameFiles/<pack>/<letter>/` relative to `PROGDIR:`.

If any pack command appears on the command line, it overrides all `enabled=` settings in
the INI for that run.

---

### Extraction

By default whdfetch extracts archives immediately after downloading. After a successful
extraction it:

1. Writes an `ArchiveName.txt` marker inside the extracted game folder
2. Updates the `.archive_index` cache for that letter drawer
3. Applies a custom drawer icon from `PROGDIR:icons/` if present
4. Deletes the archive file (default behaviour; see `KEEPARCHIVES` below)

On subsequent runs, whdfetch checks the cache before downloading. If the game is found and
the marker matches, the title is skipped without touching the network.

```
NOEXTRACT                Download but do not extract; archives are left as .lha/.lzx files
EXTRACTONLY              Extract already-downloaded archives without fetching anything new
EXTRACTTO=<path>         Extract to a different location, e.g. EXTRACTTO=Games:
KEEPARCHIVES             Keep .lha/.lzx after extraction (overrides INI and compiled default)
DELETEARCHIVES           Delete .lha/.lzx after extraction (already the default)
FORCEEXTRACT             Re-extract even when ArchiveName.txt already matches
FORCEDOWNLOAD            Re-download even when a local archive or extracted marker exists
NODOWNLOADSKIP           Download archives regardless of extraction marker (file-exists
                         check still applies)
```

**FORCEDOWNLOAD** bypasses both the file-exists check and the extraction-marker check.
**NODOWNLOADSKIP** bypasses only the marker check.
**FORCEEXTRACT** does not force a re-download; it affects extraction only.

Use `FORCEDOWNLOAD FORCEEXTRACT` together for a full rebuild from scratch.

WARNING: `FORCEDOWNLOAD` re-fetches every archive in the selected pack unconditionally.
The Games pack alone is several thousand files. Use with care on slow or metered
connections.

---

### Skip filters

Filters are based on tokens embedded in the archive filename (`_AGA`, `_NTSC`, `_CD32`,
language codes such as `_De`, `_Fr`, etc.). They stack — an archive must pass every active
filter to be processed.

```
SKIPAGA            Skip AGA chipset titles (require A1200 or A4000)
SKIPCD             Skip CD32, CDTV, and CDRom titles
SKIPNTSC           Skip NTSC-only releases
SKIPNONENGLISH     Skip non-English releases
```

Example — OCS/ECS PAL machine, English only:

```
whdfetch DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

---

### Output options

```
NOSKIPREPORT       Suppress per-archive skip messages (skips are still counted in summary)
VERBOSE            Show detailed output from DAT archive extraction
NOICONS            Skip custom drawer icon replacement and unsnapshotting
DISABLECOUNTERS    Disable "Download X of Y" counter output
CRCCHECK           Enable CRC verification against DAT metadata (default: off)
TIMEOUT=<secs>     HTTP timeout in seconds, range 5–60 (default: 30)
```

---

### Purge commands

```
PURGETEMP          Delete PROGDIR:temp/ recursively. Asks for confirmation first.
PURGEARCHIVES      Delete all .lha/.lzx files under GameFiles/. Asks for confirmation.
                   Extracted game folders are not touched.
```

---

### Practical examples

Download and extract the Games pack, standard settings:

```
whdfetch DOWNLOADGAMES
```

Download everything, no extraction (archive files kept for later):

```
whdfetch DOWNLOADALL NOEXTRACT
```

Extract previously downloaded Games archives to an external drive:

```
whdfetch DOWNLOADGAMES EXTRACTONLY EXTRACTTO=Games:
```

Download only English PAL games for an OCS/ECS machine:

```
whdfetch DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

Re-extract all Games archives without re-downloading:

```
whdfetch DOWNLOADGAMES FORCEEXTRACT
```

Full clean rebuild — re-download and re-extract everything:

```
whdfetch DOWNLOADGAMES FORCEDOWNLOAD FORCEEXTRACT
```

---

## INI Configuration

`PROGDIR:whdfetch.ini` is optional. When present, its values override compiled defaults.
CLI arguments override the INI for the current run.

Precedence: **compiled defaults → INI → CLI**

```ini
[global]
download_website=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/
extract_archives=true
skip_existing_extractions=true
skip_download_if_extracted=true
verbose_output=false
extract_path=                     ; empty = extract in place
delete_archives_after_extract=true
use_custom_icons=true
unsnapshot_icons=true
disable_counters=false
crccheck=false
timeout_seconds=30

[filters]
skip_aga=false
skip_cd=false
skip_ntsc=false
skip_non_english=false

[pack.games]
enabled=true
display_name=Games
download_url=http://...
local_dir=Games/
filter_dat=Games(
filter_zip=Games(
```

Pack section names: `games`, `games_beta`, `demos`, `demos_beta`, `magazines`.

Boolean values accept: `true`/`false`, `yes`/`no`, `1`/`0`.

Pack selection via INI uses `enabled=true` / `enabled=false` in each `[pack.*]` section.
If any CLI pack command is present, all INI `enabled=` values are ignored for that run.

A fully annotated sample is in `docs/whdfetch.ini.sample`.

---

## How skip detection works

Before downloading an archive, whdfetch checks whether the title has already been extracted:

1. The archive filename is looked up in the in-memory `.archive_index` cache.
   Cache files are stored at `GameFiles/<pack>/<letter>/.archive_index`.
2. If found, the corresponding game folder is checked for `ArchiveName.txt`.
   If line 2 of that file is an exact match for the archive filename, the title is skipped.

The cache avoids full directory scans. On classic Amiga SCSI or IDE storage this matters —
scanning thousands of subdirectories per run is very slow.

If a cached entry points to a folder that no longer exists, the stale entry is removed and
the archive is downloaded and extracted as normal.

The cache is flushed to disk during the normal shutdown sequence. If the program is
interrupted by Ctrl+C or crashes, any entries added during that run may not be saved. They
will be rebuilt from `ArchiveName.txt` markers on the next run.

---

## Interrupted download recovery

If a download is interrupted (Ctrl+C, timeout, socket error), a zero-byte marker file is
left alongside the partial archive:

```
GameFiles/<pack>/<letter>/<archive>.downloading
```

On the next run, before the normal file-exists skip check, whdfetch detects this marker,
deletes the partial archive, removes the marker, and re-downloads cleanly.

For non-retryable server errors (400, 401, 403, 404) the marker and partial archive are
deleted immediately. This prevents repeated futile download attempts on every subsequent run.

---

## Session reports

A plain-text report is written at end of run to:

```
PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt
```

Categories:

| Entry | Meaning |
|-------|---------|
| New | Newly downloaded archive with no previous version in the index |
| Updated | Downloaded archive replaces an older version already in the index |
| Local cache reuse | Archive was already on disk; no HTTP download performed |
| Extraction skipped | Extraction skipped (e.g. `c:unlzx` missing for an `.lzx` file) |

No report is written if no activity occurred.

---

## Directory layout

```
PROGDIR:
├── whdfetch
├── whdfetch.ini
├── GameFiles/
│   ├── Games/
│   │   ├── A/
│   │   │   ├── .archive_index          skip cache, updated each run
│   │   │   ├── A.info                  letter drawer icon
│   │   │   └── Academy/
│   │   │       ├── Academy.slave
│   │   │       ├── Academy.info
│   │   │       └── ArchiveName.txt     extraction marker
│   │   └── B/ ...
│   ├── Games Beta/ ...
│   ├── Demos/ ...
│   ├── Demos Beta/ ...
│   └── Magazines/ ...
├── icons/
│   ├── A.info ... Z.info               letter icon templates
│   └── WHD folder.info                 game drawer icon template (optional)
├── logs/
│   ├── general_YYYY-MM-DD_HH-MM-SS.log
│   ├── download_YYYY-MM-DD_HH-MM-SS.log
│   ├── parser_YYYY-MM-DD_HH-MM-SS.log
│   └── errors_YYYY-MM-DD_HH-MM-SS.log
├── temp/
│   ├── index.html
│   ├── Zip files/
│   └── Dat files/
└── updates/
    └── updates_YYYY-MM-DD_HH-MM-SS.txt
```

---

## Log files

Logs are written to `PROGDIR:logs/` on every run. Check `errors_*.log` first when
diagnosing problems — every error is copied there regardless of which category generated it.

---

## Known problems

- Not tested on Kickstart 1.3. AmigaOS 3.0 or later required.
- `.lzx` extraction requires `c:unlzx`. If missing, `.lzx` archives are skipped with a
  warning and the run continues normally.
- No specific emulator testing has been done. Tested on real hardware with Roadshow.
- `FORCEDOWNLOAD` re-downloads every archive in the selected packs. On the Games pack this
  can mean several thousand files. Can take hours on a slow link.
- The `.archive_index` cache is written at normal shutdown. A crash or Ctrl+C mid-run
  may leave new entries unsaved. They rebuild from `ArchiveName.txt` on the next run.
- Archive detection filters are based on filename tokens only. Metadata inside the archive
  is not inspected. An incorrectly named archive will pass filters it should not.
