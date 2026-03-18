# Retroplay WHD Downloader — Project Overview

## What it is

Retroplay WHD Downloader is a **pure CLI tool** for AmigaOS 3.0+ that automates the full download
and installation of WHDLoad game, demo, and magazine packs from the Retroplay FTP site.
It replaces the manual process of browsing the site, downloading archives, extracting
them, and organising them into the correct folder structure on Amiga storage.

The tool runs in a Shell/CLI window. There is no GUI and no Workbench application.

---

## What it does end-to-end

```
Retroplay FTP site                        Local Amiga storage
─────────────────────────────────────────────────────────────────
index.html            ──(download)──▶  temp/index.html
                                             │
                                       parse HTML links
                                             │
                                             ▼
Games(YYYY-MM-DD).zip ──(download)──▶  temp/Zip files/
                                             │
                                       unzip → extract XML
                                             │
                                             ▼
                                       temp/Dat files/Games(YYYY-MM-DD).xml
                                             │
                                       parse <rom name="...">
                                       apply skip filters
                                             │
                                             ▼
                                       temp/Dat files/Games(YYYY-MM-DD).txt
                                             │
                              ┌──────────────┘
                              │  for each ROM name:
                              │    check .archive_index cache
                              │    if not extracted → direct HTTP download
                              ▼
Academy_v1.2.lha ──(direct download)▶  GameFiles/Games/A/Academy_v1.2.lha
                                             │
                                       c:lha extract
                                       write ArchiveName.txt
                                       update .archive_index
                                       (optionally) replace drawer icon
                                             │
                                             ▼
                                       GameFiles/Games/A/Academy/
                                           Academy.slave
                                           Academy.info   ← replaced from Icons/
                                           ArchiveName.txt
```

---

## Pack types

| CLI flag | Pack | Local folder |
|----------|------|-------------|
| `GAME` / `DOWNLOAD` | Games | `GameFiles/Games/` |
| `GAMEBETA` | Games Beta | `GameFiles/Games Beta/` |
| `DEMO` | Demos | `GameFiles/Demos/` |
| `DEMOBETA` | Demos Beta | `GameFiles/Demos Beta/` |
| `MAGAZINE` | Magazines | `GameFiles/Magazines/` |
| `DOWNLOADALL` | All of the above | — |

---

## Key features

### Skip filtering
Before downloading, each ROM name is parsed into metadata. ROMs can be skipped by:
- `SKIP_AGA` — AGA-only games (require A1200/A4000 chipset)
- `SKIP_CD` — CD32 / CDTV / CDRom titles
- `SKIP_NTSC` — NTSC-only releases
- `SKIP_NONENG` — non-English or English-absent releases

These can be set via CLI flags or in the INI file `[filters]` section.

### Archive index cache (`.archive_index`)
AmigaOS filesystem scans are extremely slow on real hardware. Every
`GameFiles/<pack>/<letter>/` directory maintains a hidden `.archive_index` file
(TAB-separated, two columns: archive filename → extracted folder name). Lookups hit
the in-memory cache; the file is flushed to disk on clean shutdown.

### `ArchiveName.txt` marker
Each extracted game folder contains this two-line marker:
```
Games
Academy_v1.2.lha
```
Line 1 is the category display name; line 2 is the exact archive filename.
When whdfetch checks whether to re-download or re-extract a title it reads this
marker. If line 2 matches the incoming archive name, the operation is skipped.
Use `FORCEEXTRACT` or `FORCEDOWNLOAD` to override.

### Icon replacement
After extraction, whdfetch can replace the `.info` (icon) files placed by the
WHDLoad slave with custom ones from `PROGDIR:Icons/`:
- Letter icons: `A.info` … `Z.info` for `GameFiles/<pack>/A/`, etc.
- Game icons: `WHD folder.info` replaces each extracted game's `.info`
- Optionally runs `icon_unsnapshot` to clear saved X/Y snap positions

### INI configuration
`PROGDIR:whdfetch.ini` (optional) overrides built-in defaults.
If only legacy `PROGDIR:WHDDownloader.ini` exists, it is used as fallback.
CLI arguments always take precedence over INI values.
See `docs/whdfetch.ini.sample` for a fully annotated sample.

---

## Runtime requirements

| Requirement | Notes |
|-------------|-------|
| AmigaOS 3.0+ | 3.1+ recommended |
| Roadshow TCP/IP stack | For HTTP downloads (`bsdsocket.library v4+`) |
| `c:lha` | For archive extraction |
| Fast RAM | Strongly recommended — `amiga_malloc` uses `MEMF_ANY` |

The tool works best on an accelerated system (PiStorm, Vampire, or fast WinUAE config)
because it processes large DAT files and performs many filesystem operations.

---

## Directory layout at runtime

```
PROGDIR:                           (the directory containing whdfetch)
├── whdfetch                       binary
├── whdfetch.ini                   optional config
├── GameFiles/
│   ├── Games/
│   │   ├── A/
│   │   │   ├── .archive_index    skip cache (hidden)
│   │   │   ├── A.info            letter drawer icon
│   │   │   └── Academy/
│   │   │       ├── Academy.slave
│   │   │       ├── Academy.info
│   │   │       └── ArchiveName.txt
│   │   ├── B/ …
│   ├── Games Beta/ …
│   ├── Demos/ …
│   ├── Demos Beta/ …
│   └── Magazines/ …
├── icons/
│   ├── A.info … Z.info           letter icon templates
│   └── WHD folder.info           game icon template (optional)
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

## Source architecture

```
src/
├── main.c / main.h               Entry point, CLI parsing, 5-pack loop, do_shutdown()
├── ini_parser.c/h                whdfetch.ini (legacy fallback supported) → runtime overrides
├── gamefile_parser.c/h           <rom name="..."> → game_metadata struct
├── utilities.c/h                 String/file helpers, WB version detection
├── cli_utilities.c/h             Console cursor and size detection
├── tag_text.c/h                  ANSI-coloured formatted text builders
├── linecounter.c/h               Fast buffered line counter (8 KB reads)
├── icon_unsnapshot.c/h           Clears saved icon X/Y snap positions
├── console_output.h              CONSOLE_* macros (zero overhead in release)
├── download/
│   ├── amiga_download.h          Public HTTP API (init / download / cleanup)
│   ├── http_download.c/h         HTTP/1.0 protocol handler, progress bar
│   ├── download_lib.c            State machine, socket/tag management
│   ├── file_crc.c/h              CRC32 verification
│   ├── timer_shared.c/h          Download speed tracking
│   └── display_message.c/h       Progress output
├── extract/
│   └── extract.c/h               LHA dispatch, ArchiveName.txt, .archive_index
├── log/
│   ├── log.h                     LogCategory enum, log_* macros
│   └── log.c                     Timestamped log file writer
└── platform/
    ├── platform.h                amiga_malloc/free macros + AMIGA_APP_NAME
    ├── platform.c                Memory tracking, OOM emergency handler
    ├── amiga_headers.h           All Amiga system includes (guarded)
    └── platform_types.h          platform_bptr abstraction
```

---

## Build overview

```powershell
make                    # release build → Bin/Amiga/whdfetch
make CONSOLE=1          # with console (printf visible in Shell)
make MEMTRACK=1         # with allocation/leak tracking
make CONSOLE=1 MEMTRACK=1
make AUTO=0             # omit -lauto (isolate startup crashes)
make clean
```

Compiler: VBCC `vc +aos68k -c99 -cpu=68000 -O2 -size`  
Output: `Bin/Amiga/whdfetch`  
Build objects: `build/amiga/`

---

## `$VER:` metadata date format

The executable embeds an Amiga-compatible `$VER:` string so the `version` command can
read program version metadata directly from the binary.

- Version source: `PROGRAM_NAME_LITERAL` + `VERSION_STRING_LITERAL` in `src/main.c`
- Date source: build-time generated macro `APP_BUILD_DATE_DMY` (`DD.MM.YYYY`)
- Generated header: `build/amiga/generated/build_version.h`
- Build step: `Makefile` regenerates `build_version.h` each build using PowerShell

Effective embedded format:

```text
$VER: Retroplay WHD Downloader 0.9b (18.03.2026)
```

Notes:

- This avoids relying on `__DATE__` (which uses `Mon DD YYYY` format).
- The scanner helper in `main.c` avoids storing a standalone `"$VER:"` literal so the
      app tag remains discoverable as the primary version string.

---

## Development environment

| Component | Detail |
|-----------|--------|
| Host OS | Windows |
| Compiler | VBCC cross-compiler + NDK 3.2 + Roadshow SDK |
| Editor | VS Code |
| Emulator | WinUAE with shared drive pointing at `build\amiga` |
| Target | AmigaOS 3.0+, 68000+ |

---

## Design documents

| File | Topic |
|------|-------|
| `docs/whdfetch.ini.sample` | Fully annotated INI sample |
| `docs/ini_runtime_test_matrix.md` | Per-key INI test checklist |
| `docs/extraction_plan.md` | Extraction system design |
| `docs/archive_index_plan.md` | Archive index cache design |
| `docs/archive_index_handoff.md` | Archive index implementation handoff |
| `docs/dat_diff_and_report_plan.md` | Future: DAT diff/report feature |
| `docs/dat_discovery_and_ini_plan.md` | Future: DAT discovery improvements |
