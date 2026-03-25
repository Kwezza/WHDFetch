# Retroplay WHD Downloader

AmigaOS CLI tool that automates downloading, extracting, and organising
[WHDLoad](http://www.whdload.de/) game, demo, and magazine packs from the
[Retroplay](http://turran.retroplay.org/) FTP site.

Run it from a Shell window on your Amiga (or WinUAE). It handles everything:
fetching the pack index, downloading only what is new, extracting archives,
writing skip-markers so already-installed titles are never re-processed, and
optionally replacing drawer icons.

---

## Requirements

| Requirement | Notes |
|-------------|-------|
| AmigaOS 3.0+ | 3.1+ recommended |
| Roadshow TCP/IP stack | `bsdsocket.library v4+` required for HTTP |
| `c:lha` | Required for `.lha` extraction |
| `c:unlzx` | Optional for `.lzx` extraction (`.lzx` archives are skipped if missing) |
| Fast RAM | Strongly recommended — tool uses `MEMF_ANY` allocations |

---

## Quick Start

1. Copy `whdfetch` and `whdfetch.ini` to a directory on your Amiga
2. Edit `whdfetch.ini` (or use CLI flags — see below)
3. Open a Shell in that directory and run:

```
whdfetch GAME
```

Games will be downloaded to `GameFiles/Games/` organised into letter sub-folders.

---

## CLI Arguments

### What to download

| Argument | Action |
|----------|--------|
| `GAME` / `DOWNLOAD` | Download Games pack |
| `GAMEBETA` | Download Games Beta pack |
| `DEMO` | Download Demos pack |
| `DEMOBETA` | Download Demos Beta pack |
| `MAGAZINE` | Download Magazines pack |
| `DOWNLOADALL` | Download all five packs |
| `PURGETEMP` | Delete `PROGDIR:temp` recursively (asks for confirmation first) |
| `PURGEARCHIVES` | Purge downloaded archives (`.lha`/`.lzx`) under `GameFiles/` (asks for confirmation; keeps extracted folders) |
| `HELP` | Show help and exit |

### Extraction control

| Argument | Action |
|----------|--------|
| `NOEXTRACT` | Download archives but do not extract them |
| `EXTRACTONLY` | Extract already-downloaded archives without downloading anything new |
| `EXTRACTTO=<path>` | Extract to an alternate path (preserves pack/letter layout) |
| `KEEPARCHIVES` | Keep archive files (`.lha`/`.lzx`) after successful extraction |
| `DELETEARCHIVES` | Delete archive files (`.lha`/`.lzx`) after successful extraction |
| `FORCEEXTRACT` | Re-extract even when `ArchiveName.txt` already matches |
| `FORCEDOWNLOAD` | Re-download even when the title is already extracted |
| `NODOWNLOADSKIP` | Download even if an extraction marker exists |
| `VERIFYMARKER` | Enable extra `ArchiveName.txt` verification before download skip |

### Skip filters

| Argument | Skips |
|----------|-------|
| `SKIP_AGA` | AGA-only titles (require A1200/A4000) |
| `SKIP_CD` | CD32 / CDTV / CDRom titles |
| `SKIP_NTSC` | NTSC-only releases |
| `SKIP_NONENG` | Non-English or English-absent releases |

### Output

| Argument | Action |
|----------|--------|
| `NOICONS` | Skip icon replacement after extraction |
| `NOSKIP` | Print a line for every skipped title |
| `VERBOSE` | Show detailed unzip output (default is quiet) |
| `DISABLECOUNTERS` | Disable counter features (including pre-count and terminal counter display) |
| `CRCCHECK` | Enable CRC verification for downloaded archives |

---

## INI Configuration

`PROGDIR:whdfetch.ini` is optional. When present it overrides built-in defaults.
If only legacy `PROGDIR:WHDDownloader.ini` exists, it is still supported as a fallback.
CLI arguments always take precedence over INI values.

**Precedence:** built-in defaults → INI → CLI arguments

A fully annotated sample is at `docs/whdfetch.ini.sample`.  
A per-key test checklist is at `docs/ini_runtime_test_matrix.md`.

### `[global]`

```ini
download_website=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/
extract_archives=true
skip_existing_extractions=true
skip_download_if_extracted=true
verify_archive_marker_before_download=false
verbose_output=false
extract_path=                     ; empty = extract in place
delete_archives_after_extract=true
use_custom_icons=true
unsnapshot_icons=true
disable_counters=false
crccheck=false
```

### `[filters]`

```ini
skip_aga=false
skip_cd=false
skip_ntsc=false
skip_non_english=false
```

### `[pack.<type>]`

Types: `games`, `games_beta`, `demos`, `demos_beta`, `magazines`

```ini
[pack.games]
enabled=true
display_name=Games
download_url=http://...
local_dir=Games/
filter_dat=Games(
filter_zip=Games(
```

Boolean values accept `true`/`false`, `yes`/`no`, `1`/`0`.

---

## How skip detection works

Before downloading a title, whdfetch checks whether it has already been extracted:

1. It looks up the archive filename in the in-memory `.archive_index` cache  
    (loaded from `GameFiles/<pack>/<letter>/.archive_index` at startup)
2. If found, it verifies the indexed extracted folder still exists.
3. Optional: when `VERIFYMARKER` is enabled (or
    `verify_archive_marker_before_download=true` in INI), it also checks
    `<game folder>/ArchiveName.txt` — a two-line marker:

```
Games
Academy_v1.2.lha
```

If line 2 is an exact match for the incoming archive filename, the title is skipped.
Without `VERIFYMARKER`, the pre-download skip uses the index+folder fast path.

Use `FORCEEXTRACT` to bypass the extraction skip check.  
Use `FORCEDOWNLOAD` or `NODOWNLOADSKIP` to bypass the download skip check.

---

## Runtime directory layout

```
PROGDIR:
├── whdfetch
├── whdfetch.ini
├── GameFiles/
│   ├── Games/
│   │   ├── A/
│   │   │   ├── .archive_index       (hidden skip cache)
│   │   │   ├── A.info               (letter drawer icon)
│   │   │   └── Academy/
│   │   │       ├── Academy.slave
│   │   │       ├── Academy.info
│   │   │       └── ArchiveName.txt
│   │   └── B/ …
│   ├── Games Beta/ …
│   ├── Demos/ …
│   ├── Demos Beta/ …
│   └── Magazines/ …
├── icons/
│   ├── A.info … Z.info              (letter icon templates)
│   └── WHD folder.info              (game icon template, optional)
├── logs/
│   ├── general_YYYY-MM-DD_HH-MM-SS.log
│   ├── download_YYYY-MM-DD_HH-MM-SS.log
│   ├── parser_YYYY-MM-DD_HH-MM-SS.log
│   └── errors_YYYY-MM-DD_HH-MM-SS.log
└── temp/
    ├── index.html
    ├── Zip files/
    └── Dat files/
```

---

## Log files

Logs are written to `PROGDIR:logs/` on every run. The `errors_*.log` file receives a
copy of every error regardless of which category produced it — check it first when
diagnosing problems.

---

## Building from source

```powershell
make                    # release build
make CONSOLE=1          # with console output (use when debugging)
make MEMTRACK=1         # with allocation/leak tracking
make CONSOLE=1 MEMTRACK=1
make AUTO=0             # omit -lauto (isolates startup crashes)
make refresh-version    # refresh build/amiga/generated/build_version.h to today's date
make release            # clean release build with fresh date and stripped symbols
make clean
```

**Requirements:** VBCC cross-compiler + NDK 3.2 + Roadshow SDK on Windows host.

**Build note (Windows):** normal `make` builds use the pinned date in
`build/amiga/generated/build_version.h` (created from `PINNED_BUILD_DATE` in `Makefile`
if missing). Use `make refresh-version` to update that header date manually.
`make release` always regenerates the header date in `DD.MM.YYYY` format via PowerShell
before compiling and links with symbol stripping enabled.

Output binary: `Bin/Amiga/whdfetch`

---

## Documentation

| File | Contents |
|------|----------|
| `PROJECT_OVERVIEW.md` | Full architecture and data-flow overview |
| `AGENTS.md` | AI agent operational guide (module map, task patterns, pitfalls) |
| `.github/copilot-instructions.md` | Coding standards, API reference, build rules |
| `docs/whdfetch.ini.sample` | Fully annotated INI sample |
| `docs/ini_runtime_test_matrix.md` | Per-key INI test checklist |
| `docs/extraction_plan.md` | Extraction system design |
| `docs/archive_index_plan.md` | Archive index cache design |
