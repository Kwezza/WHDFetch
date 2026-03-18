# AGENTS.md — whdfetch Agent Guide

This file is for AI coding agents (GitHub Copilot, Claude, etc.). It describes the
**operational rules, task patterns, and module ownership** for this codebase.

For the full technical reference see `.github/copilot-instructions.md`.  
For the user-facing project description see `PROJECT_OVERVIEW.md`.

---

## ABSOLUTE RULES

These are non-negotiable. Violating them will break the project.

1. **No GUI** — This is a pure CLI tool. Never add ReAction, GadTools, Intuition windows,
   or requesters to runtime code. The only Intuition call allowed is the OOM `EasyRequest`
   emergency handler already in `platform.c`.

2. **No `free()` or `malloc()` directly** — Always use `amiga_malloc()` / `amiga_free()`.
   On Amiga these use `AllocVec`/`FreeVec`. In `DEBUG_MEMORY_TRACKING` builds, untracked
   frees that bypass this will corrupt the leak log.

3. **Every exit path must call `do_shutdown()`** — This flushes the archive index, frees
   INI allocations, writes the memory report, and closes log files.

4. **`%ld` + `(long)` cast for all integer `printf`/`log_*`** — On 68k, format/type
   mismatches corrupt the stack silently and may only crash at exit. Always cast `LONG`,
   `int`, and enum values to `(long)` when passing to variadic functions.

5. **Never put two large structs (>4 KB each) on the same stack frame** — VBCC allocates
   all locals simultaneously at function entry. Heap-allocate with `amiga_malloc` instead.

6. **New `.c` files must `#include "platform/platform.h"`** — This provides
   `amiga_malloc`/`amiga_free`. Without it VBCC emits implicit-declaration warnings and
   may generate broken code.

---

## Build and test workflow

```powershell
# Standard dev build (always use CONSOLE=1 when investigating runtime behaviour)
make CONSOLE=1

# First build of any new module — also check for leaks
make CONSOLE=1 MEMTRACK=1

# Clean build
make clean; make CONSOLE=1

# Isolate a startup crash (omit -lauto)
make AUTO=0 CONSOLE=1
```

After building, the binary appears at `Bin/Amiga/whdfetch`. If WinUAE is running
with the shared drive mapped, it is immediately available for testing.

Check `Bin/Amiga/logs/` for runtime output. Errors go to `errors_*.log` regardless of
which category generated them.

There is **no automated test runner**. Testing is manual in WinUAE.

---

## Adding a new source module

1. Create `src/<subdir>/<name>.c` and `src/<subdir>/<name>.h`
2. Add `src/<subdir>/<name>.c` to `SRCS` in `Makefile`
3. Add `build/amiga/<subdir>/<name>.o` to `OBJS` in `Makefile`
4. If `<subdir>` is new, add `build/amiga/<subdir>` to the `directories` target
5. `#include "platform/platform.h"` at the top of the `.c` file
6. Run `make CONSOLE=1 MEMTRACK=1` to verify clean compilation

---

## Adding a new log category

1. Insert the new enum value **above** `LOG_ERRORS` in `src/log/log.h`
2. Insert the matching label string at the **same index** in `g_categoryNames[]` in `src/log/log.c`
3. `LOG_CATEGORY_COUNT` stays last and updates automatically

---

## Module ownership map

Use this to find the right file for a given change.

| Task | File(s) |
|------|---------|
| CLI argument parsing | `src/main.c` |
| Pack definitions (URLs, filters, directories) | `src/main.h` |
| Shutdown/cleanup sequence | `src/main.c` → `do_shutdown()` |
| INI key parsing | `src/ini_parser.c/h` |
| ROM filename → metadata (title, version, AGA, NTSC…) | `src/gamefile_parser.c/h` |
| Skip filter logic (`should_include_game`) | `src/gamefile_parser.c` |
| HTTP download (progress, socket, CRC) | `src/download/` |
| LHA extraction, `ArchiveName.txt`, icon replacement | `src/extract/extract.c/h` |
| Archive index cache (load / query / update / flush) | `src/extract/extract.c/h` |
| Log categories, `log_*` macros | `src/log/log.h` + `src/log/log.c` |
| Memory allocation, OOM handler | `src/platform/platform.c/h` |
| `CONSOLE_*` output macros | `src/console_output.h` |
| Amiga system includes | `src/platform/amiga_headers.h` |
| String/file helpers | `src/utilities.c/h` |
| Console cursor / window size | `src/cli_utilities.c/h` |
| ANSI-coloured output builders | `src/tag_text.c/h` |
| Fast line counting | `src/linecounter.c/h` |
| Icon unsnapshotting | `src/icon_unsnapshot.c/h` |

---

## Data formats

### `.archive_index` (per letter directory)

Location: `GameFiles/<pack>/<letter>/.archive_index`

```
Academy_v1.2.lha\tAcademy
Alien3_v1.1_AGA.lha\tAlien3AGA
```

Two TAB-separated columns: archive filename → extracted folder name. One entry per line.
Load on startup → query in `extract.c` → flush in `do_shutdown()` via `extract_index_flush()`.

### `ArchiveName.txt` (per extracted game folder)

```
Games
Academy_v1.2.lha
```

Line 1: category display name. Line 2: exact archive filename. If line 2 matches the
incoming archive name, skip extraction (unless `FORCEEXTRACT`).

### INI file (`PROGDIR:whdfetch.ini`)

Legacy fallback is also supported via `PROGDIR:WHDDownloader.ini`.

Optional. Parsed by `ini_parser.c`. Sections: `[global]`, `[filters]`, `[pack.<type>]`.
Boolean values accept `true`/`false`, `yes`/`no`, `1`/`0`.

---

## Common tasks — quick guide

### Add a new CLI flag

1. Add the parsing logic in `main.c` near the existing `ReadArgs` / manual arg checks
2. Add the corresponding field to `download_option` in `main.h` (or `whdload_pack_def` if pack-specific)
3. Wire it up to the INI equivalent in `ini_parser.c`
4. Document the flag in `README.md` and `.github/copilot-instructions.md`

### Add a new INI key

1. Add parsing in `ini_parser.c` — use existing key blocks as a pattern
2. `amiga_strdup()` for string values; use `ini_parser_cleanup_overrides()` to free them
3. Expose via an accessor or by writing directly into `download_option` / `whdload_pack_def`
4. Add the key to `docs/whdfetch.ini.sample` with a comment

### Modify skip/filter logic

All filter decisions go through `should_include_game()` in `gamefile_parser.c`.
The function receives a `game_metadata *` and the current skip flags. Add new filter
logic there rather than scattering checks.

### Modify extraction behaviour

`src/extract/extract.c` owns everything after an `.lha` file has been downloaded:
- `extract_lha()` — calls `c:lha`, writes `ArchiveName.txt`, handles icon replacement
- `extract_index_load()` / `extract_index_update()` / `extract_index_flush()` — cache management
- The skip check (via `ArchiveName.txt` and `.archive_index`) lives here

### Change download behaviour

`src/download/download_lib.c` is the state machine.  
`src/download/http_download.c` handles the HTTP/1.0 protocol.  
The public API is in `src/download/amiga_download.h` — only call that from `main.c`.

---

## Known pitfalls

| Pitfall | Safe practice |
|---------|---------------|
| VBCC strips quotes from `-D"string"` on Windows | Edit `src/platform/platform.h` directly for `AMIGA_APP_NAME` |
| `FIBF_HIDDEN` not defined in some NDK header sets | Wrap with `#ifdef FIBF_HIDDEN` |
| Large local variables blowing the stack frame | Heap-allocate structs > 4 KB |
| Format string / type mismatch on 68k | Cast all integers to `(long)` for `printf`/`log_*` |
| Premature free of INI strings | All INI string allocations live until `ini_parser_cleanup_overrides()` |
| Archive index not flushed on abnormal exit | Always route exit through `do_shutdown()` |
| `free()` in `DEBUG_MEMORY_TRACKING` build | Use `amiga_free()` — or `FreeVec()` for AllocVec-allocated blocks not tracked via `amiga_malloc` |

---

## What is out of scope

- **GUI of any kind** — CLI only, no Intuition/ReAction/GadTools
- **Networking protocols other than HTTP/1.0** — the download library is purpose-built
- **Non-AmigaOS targets** — all code assumes AmigaOS 3.0+ and 68000+
- **Unicode / multi-byte strings** — ASCII only, Topaz font
