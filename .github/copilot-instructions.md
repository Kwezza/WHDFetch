# Retroplay WHD Downloader (whdfetch) - GitHub Copilot Instructions

## Project Overview

**Retroplay WHD Downloader** (`whdfetch`) is a VBCC/AmigaOS CLI tool that automates downloading and extracting WHDLoad game archives from the Retroplay FTP site. It:

- Downloads `index.html`, parses HTML links, and fetches matching `.zip` packs from the Retroplay site
- Extracts XML DAT files from ZIPs, parses `<rom name="...">` entries, applies skip filters (AGA/CD/NTSC/language)
- Downloads individual `.lha` ROM archives via built-in HTTP download code into `GameFiles/<pack>/<letter>/`
- Extracts `.lha` archives using `c:lha`, writes `ArchiveName.txt` markers, and optionally replaces drawer icons
- Caches extracted-game state in `.archive_index` files to avoid expensive AmigaOS filesystem scans

**This is a pure CLI tool — there is no GUI.** Do not introduce ReAction/GadTools code.

Target: **AmigaOS 3.0+**, compiled with **VBCC v0.9x (`+aos68k`)**, CPU **68000+**, Roadshow TCP/IP stack required at runtime.

---

## Project Structure

```
WHDDownloader/
├── Makefile
├── src/
│   ├── main.c                      <- entry point, CLI parsing, 5-pack download orchestration
│   ├── main.h                      <- whdload_pack_def + download_option structs
│   ├── ini_parser.c/h              <- whdfetch.ini loader (legacy fallback supported)
│   ├── gamefile_parser.c/h         <- archive filename → game_metadata parser
│   ├── utilities.c/h               <- string/file helpers, Workbench version detection
│   ├── cli_utilities.c/h           <- console cursor/size detection
│   ├── tag_text.c/h                <- formatted text builders with ANSI colours
│   ├── linecounter.c/h             <- fast buffered line counter (8 KB reads, MEMF_FAST)
│   ├── icon_unsnapshot.c/h         <- clears saved icon X/Y positions
│   ├── console_output.h            <- conditional printf macros (zero overhead in release)
│   ├── download/
│   │   ├── amiga_download.h        <- public HTTP download API (init/download/cleanup)
│   │   ├── http_download.c/h       <- HTTP/1.0 protocol, progress display
│   │   ├── download_lib.c          <- core state machine, socket/tag management
│   │   ├── file_crc.c/h            <- CRC32 verification
│   │   ├── timer_shared.c/h        <- speed tracking
│   │   └── display_message.c/h     <- progress output
│   ├── extract/
│   │   └── extract.c/h             <- LHA dispatch, ArchiveName.txt marker, .archive_index cache
│   ├── log/
│   │   ├── log.h                   <- LogCategory enum, log_* macros
│   │   └── log.c                   <- logging implementation
│   └── platform/
│       ├── platform.h              <- amiga_malloc/free macros + AMIGA_APP_NAME
│       ├── platform.c              <- memory tracking + OOM emergency handler
│       ├── amiga_headers.h         <- all Amiga system includes (guarded)
│       └── platform_types.h        <- platform_bptr type abstraction
├── build/amiga/                    <- compiler output (gitignored)
├── Bin/Amiga/whdfetch              <- final binary (gitignored)
├── Bin/Amiga/GameFiles/            <- downloaded .lha archives and extracted games
├── Bin/Amiga/temp/                 <- index.html, ZIP files, DAT/xml work area
├── Bin/Amiga/icons/                <- letter icon templates (A.info … Z.info)
└── docs/                           <- design plans, .ini sample, ReAction tips
```

---

## Build

```powershell
# Standard release build
make

# With console window (debug/development)
make CONSOLE=1

# With memory leak tracking
make MEMTRACK=1

# Both (recommended for first build of new modules)
make CONSOLE=1 MEMTRACK=1

# Link with -lauto (default); isolate startup crash: AUTO=0
make AUTO=0

# Clean
make clean
```

Build output: `Bin/Amiga/whdfetch`  
Log output at runtime: `PROGDIR:logs/`

**Note:** VBCC on Windows strips quotes from `-D` string values on the command line, so `AMIGA_APP_NAME` cannot be set via a Makefile `-D` flag. Edit `src/platform/platform.h` directly.

Adding a new `.c` module requires:
1. Adding `src/<file>.c` to `SRCS` and `build/amiga/<file>.o` to `OBJS` in `Makefile`
2. If in a subdirectory, adding `build/amiga/<subdir>` to the `directories` target
3. Including `platform/platform.h` in the new file (required for `amiga_malloc`/`amiga_free`)

---

## Data Flow

### Macro summary

```
1. Download index.html → parse HTML links → filter by HTML_LINK_PREFIX_FILTER / CONTENT_FILTER
2. Download matching .zip packs → temp/Zip files/
3. Extract XML DAT files from ZIPs → temp/Dat files/<filter>(<date>).xml
4. Parse <rom name="..."> entries → apply skip filters → write .txt list
5. For each ROM name: check .archive_index cache → if absent, direct HTTP download → GameFiles/<pack>/<letter>/<rom>
6. Extract .lha via c:lha → write ArchiveName.txt marker → update .archive_index → optionally replace icon
```

### 5 Pack definitions (`whdload_pack_def` array in `main.h`)

| Index | Name | Local dir | DAT filter |
|-------|------|-----------|------------|
| 0 | Demos Beta | `GameFiles/Demos Beta/` | `Demos-Beta(` |
| 1 | Demos | `GameFiles/Demos/` | `Demos(` |
| 2 | Games Beta | `GameFiles/Games Beta/` | `Games-Beta` |
| 3 | Games | `GameFiles/Games/` | `Games(` |
| 4 | Magazines | `GameFiles/Magazines/` | `Magazines` |

---

## CLI Arguments

```
HELP                 Show help and exit
DOWNLOAD             Download Games pack
DEMO                 Download Demos pack
DEMOBETA             Download Demos Beta pack
GAME                 Download Games pack (alias)
GAMEBETA             Download Games Beta pack
MAGAZINE             Download Magazines pack
DOWNLOADALL          Download all packs
NOEXTRACT            Skip extraction after download
EXTRACTTO=<path>     Extract to alternate path
KEEPARCHIVES         Do not delete .lha after extraction
DELETEARCHIVES       Delete .lha after extraction
EXTRACTONLY          Extract already-downloaded archives only
FORCEEXTRACT         Re-extract even if ArchiveName.txt present
NODOWNLOADSKIP       Download even if already extracted
SKIP_AGA             Filter: skip AGA games
SKIP_CD              Filter: skip CD32/CDTV
SKIP_NTSC            Filter: skip NTSC games
SKIP_NONENG          Filter: skip non-English versions
NOICONS              Skip icon replacement
NOSKIP               Show all skip messages
VERBOSE              Detailed unzip output (default is quiet)
```

---

## INI File Format (`PROGDIR:whdfetch.ini`)

INI is optional; all keys override compiled defaults. Legacy `PROGDIR:WHDDownloader.ini` is also supported as fallback. See `docs/whdfetch.ini.sample`.

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

Boolean values accept: `true`/`false`, `yes`/`no`, `1`/`0`.

---

## Archive Index Cache (`.archive_index`)

Avoids expensive `ExNext` + `ArchiveName.txt` reads on slow Amiga storage.

**Location:** `GameFiles/<pack>/<letter>/.archive_index`  
**Format:** TAB-separated, two columns per line:

```
A10TankKiller_v2.0_2Disk.lha<TAB>A10TankKiller2Disk
Academy_v1.2.lha<TAB>Academy
```

**Lifecycle:** Loaded at startup → queried during extraction → updated in-memory on success → flushed to disk by `extract_index_flush()` in `do_shutdown()` → stale entries (deleted folders) auto-pruned on flush.

## Marker File (`ArchiveName.txt`)

Written inside each extracted game folder:

```
Line 1: Category display name  (e.g., "Games")
Line 2: Exact archive filename (e.g., "Academy_v1.2.lha")
```

If line 2 matches the current archive filename, extraction is skipped (unless `FORCEEXTRACT`).

---

## Logging System

### Log Categories

```c
typedef enum {
    LOG_GENERAL,    /* Startup, shutdown, config, main flow */
    LOG_MEMORY,     /* Allocs/frees (high-frequency, often disabled) */
    LOG_APP,        /* App-specific (available for new use) */
    LOG_DOWNLOAD,   /* HTTP operations, FTP link parsing */
    LOG_PARSER,     /* DAT parsing, metadata extraction */
    LOG_ERRORS,     /* All errors auto-aggregated here */
    LOG_CATEGORY_COUNT
} LogCategory;
```

**Adding a new category:**
1. Insert above `LOG_ERRORS` in `log.h`
2. Add matching string to `g_categoryNames[]` in `log.c` at the same index
3. `LOG_CATEGORY_COUNT` stays last automatically

### Log macros

```c
log_debug(LOG_DOWNLOAD, "Connecting: %s\n", url);
log_info(LOG_GENERAL,   "Pack loaded: %s\n", pack->full_text_name_of_pack);
log_warning(LOG_PARSER, "Unknown token: %s\n", token);
log_error(LOG_GENERAL,  "download failed: %ld\n", (long)rc);
```

Log files written to `PROGDIR:logs/<category>_YYYY-MM-DD_HH-MM-SS.log`.

### Initialisation / shutdown

```c
initialize_log_system(TRUE);   /* TRUE = delete old logs */
/* ... */
shutdown_log_system();
```

---

## Memory System

| Macro / Function | Purpose |
|-----------------|---------|
| `amiga_malloc(size)` | Allocate (tracked when `MEMTRACK=1`) |
| `amiga_free(ptr)` | Free |
| `amiga_strdup(str)` | Duplicate string |
| `amiga_memory_init()` | Call once at startup (no-op in release) |
| `amiga_memory_report()` | Print leak report to `memory_*.log` (no-op in release) |
| `amiga_memory_suspend_logging()` | Silence during bulk ops |
| `amiga_memory_resume_logging()` | Re-enable |

`amiga_malloc()` uses `AllocVec(size, MEMF_ANY | MEMF_CLEAR)` — prefers Fast RAM automatically.

### Memory management rules

1. Always use `amiga_malloc`/`amiga_free` for heap allocations
2. Always check for `NULL` after allocation
3. Free in reverse allocation order where practical
4. Match allocator to deallocator:
   - `amiga_malloc` → `amiga_free`
   - `AllocDosObject` → `FreeDosObject`
   - `AllocVec` → `FreeVec` (not `free()`)
   - `Lock` → `UnLock`
5. In `DEBUG_MEMORY_TRACKING` builds, untracked frees **must** use `FreeVec`, not `free()`

---

## Console Output

```c
CONSOLE_PRINTF("Processing: %s\n", path);   /* general info */
CONSOLE_STATUS("Progress: %d%%\n", pct);    /* progress     */
CONSOLE_WARNING("Skipping: %s\n", name);    /* warnings     */
CONSOLE_ERROR("Failed: %ld\n", (long)err);  /* errors       */
CONSOLE_BANNER("=== Starting ===\n");       /* startup msg  */
CONSOLE_SEPARATOR();                         /* ==== line    */
```

All macros expand to `((void)0)` when `CONSOLE=0` — zero overhead in release builds.

---

## Language and Coding Standards

- **Compiler**: VBCC v0.9x with `+aos68k` configuration
- **Standard**: C99 (`-c99` flag) — `//` comments and inline declarations are OK
- **Naming**: `snake_case` for all functions, variables, types
- **Prefixes**: `amiga_` for library functions; `AMIGA_` for macros
- **Booleans**: `BOOL`, `TRUE`, `FALSE` — not C99 `bool`
- **Strings**: ASCII only — no Unicode. Amiga Topaz font is ASCII-only.
- **Indentation**: 4 spaces (no tabs)

### AmigaOS type reference

```c
BOOL    /* 2-byte boolean          */
STRPTR  /* char *                  */
BPTR    /* DOS file/lock           */
LONG    /* 32-bit signed           */
ULONG   /* 32-bit unsigned         */
WORD    /* 16-bit signed           */
UWORD   /* 16-bit unsigned         */
```

### Structure field ordering (68k alignment)

Group fields by size — largest first — to minimise padding:

```c
typedef struct {
    ULONG  count;         /* 4 bytes */
    char  *name;          /* 4 bytes */
    BOOL   is_valid;      /* 2 bytes */
} MyStruct;
```

### 68000 stack frame limit

VBCC allocates locals for **all** `case` / `if` branches simultaneously at function entry. Keep any function's local frame under ~32 KB. Never put two large structs (>4 KB each) in the same function — heap-allocate instead.

---

## Shutdown Sequence (`do_shutdown()` in `main.c`)

Every exit path must call `do_shutdown()`:

1. `ad_cleanup_download_lib()` — close sockets/timer
2. `extract_index_flush()` — flush `.archive_index` cache to disk
3. `ini_parser_cleanup_overrides()` — free INI allocations
4. `amiga_memory_report()` — leak report (no-op in release)
5. `shutdown_log_system()` — close all log files

---

## Pitfalls and Known Quirks

- **No GUI** — this is a pure CLI tool. Never add ReAction/GadTools code.
- **VBCC `-D` string stripping** — cannot set string macros via `make -DFOO="bar"` on Windows; edit `platform.h` directly.
- **`FIBF_HIDDEN` guard** — some NDK header sets don't define it; wrap hidden-file calls in `#ifdef FIBF_HIDDEN`.
- **Varargs/format mismatches** — on 68k these corrupt the stack but may only crash at program exit. Always match `%ld` with `(long)` cast for `LONG` values in `printf`/`log_*`.
- **New modules** must `#include "platform/platform.h"` — otherwise VBCC emits implicit declaration warnings for `amiga_malloc`/`amiga_free`.
- **Archive index is critical for performance** — do not bypass it; filesystem scans on SCSI/IDE are extremely slow on classic hardware.
- **Case-insensitive filesystem** — AmigaOS is case-insensitive but case-preserving; use exact case when constructing paths programmatically.

---

## Development Environment

- **Host**: Windows PC
- **Compiler**: VBCC cross-compiler (`vc`) with NDK 3.2 + Roadshow SDK
- **Emulator**: WinUAE — shared drive maps `build\amiga` into Amiga filesystem
- **Target OS**: AmigaOS 3.0+, Roadshow TCP/IP required at runtime

### Build and test cycle

1. Edit source on PC in VS Code
2. `make CONSOLE=1` in PowerShell terminal
3. Binary auto-appears in WinUAE shared drive
4. Run in WinUAE, check output + `PROGDIR:logs/`
5. For leak checks: `make MEMTRACK=1`, check `memory_*.log`
