# WHDDownloader — Migration Handoff

## What this project is

This is a migration of **Retroplay-WHDLoad-downloader** (a SAS/C Amiga CLI tool that
downloads WHDLoad packs from the Turran/Retroplay FTP site) into a new VBCC cross-compile
project based on the **AmigaBase** skeleton framework.

This project was just created by cloning AmigaBase. The AmigaBase skeleton files are
already in place (Makefile, src/main.c, src/log/, src/platform/). The next step is to
bring in the source code and configure it.

---

## Source repositories (all local on this machine)

| Role | Local path | GitHub |
|---|---|---|
| **This project (new)** | `C:\Amiga\Programming\WHDDownloader` | `Kwezza/WHDDownloader` (private) |
| **Old SAS/C project** | `C:\Amiga\Programming\Retroplay-WHDLoad-downloader` | `Kwezza/Retroplay-WHDLoad-downloader` (public) |
| **Download library (VBCC-updated)** | `C:\Amiga\Programming\TestDownload-1` | `Kwezza/TestDownload` (private) |

---

## Decisions already made

- **Compiler:** VBCC cross-compile on Windows, targeting `+aos68k` / `m68k-amigaos`
- **C standard:** `-c99` (not strict C89 — owner is happy with C99)
- **WB requirement:** Bumped from 2.04 to **Workbench 3.0+** (simplifies old version detection code)
- **Memory:** Use `amiga_malloc`/`amiga_free` from AmigaBase `platform.h` throughout (backed by AllocVec on Amiga, with optional leak tracking via `make MEMTRACK=1`)
- **ReAction:** NOT used — this is a pure CLI tool. The AmigaBase ReAction skeleton in `src/main.c` gets replaced entirely.
- **Download library:** Source files copied directly into `src/download/` — no pre-compiled `.lib`
- **Structure**: Keep AmigaBase's `src/log/` and `src/platform/` as-is. Add project source files into `src/`.

---

## AmigaBase files already present (do not modify these)

```
src/console_output.h       ← CONSOLE_* macros
src/log/log.c + log.h      ← multi-category logging (write to PROGDIR:logs/)
src/platform/platform.h+.c ← amiga_malloc/free, MEMTRACK
src/platform/amiga_headers.h
src/platform/platform_types.h
Makefile                   ← vbcc build system (needs updating for this project)
```

---

## Files to bring in from the download library

Copy these 6 files from `C:\Amiga\Programming\TestDownload-1\src\` into `src\download\`:

```
http_download.c
http_download.h
download_lib.c
file_crc.c
file_crc.h
timer_shared.c
timer_shared.h
display_message.c
display_message.h
```

And copy the public API header from `C:\Amiga\Programming\TestDownload-1\lib\`:
```
amiga_download.h  → src/download/amiga_download.h
```

The download library was already updated for VBCC (documented in
`C:\Amiga\Programming\Retroplay-WHDLoad-downloader\VBCC_version.md`). The changes were:
- Removed `#include <proto/socket.h>` and `#include <proto/utility.h>` (SAS/C only)
- Added local `strnicmp()` guarded by `#ifndef __SASC`
- Replaced `GetTagData()` with a local `local_get_tag_data()` walker
- Fixed `ad_init_download_lib_taglist()` to use proper `<stdarg.h>` va_args

---

## Files to bring in from the old project

Copy these from `C:\Amiga\Programming\Retroplay-WHDLoad-downloader\` into `src\`:

```
Main.c              → src/Main.c       (replaces AmigaBase's src/main.c)
main.h              → src/main.h
utilities.c         → src/utilities.c
utilities.h         → src/utilities.h
gamefile_parser.c   → src/gamefile_parser.c
gamefile_parser.h   → src/gamefile_parser.h
tag_text.c          → src/tag_text.c
tag_text.h          → src/tag_text.h
cli_utilities.c     → src/cli_utilities.c
cli_utilities.h     → src/cli_utilities.h
linecounter.c       → src/linecounter.c
linecounter.h       → src/linecounter.h
```

**Delete** AmigaBase's `src/main.c` once `src/Main.c` is in place.

---

## Source code fixes required during migration

### 1. Remove `#include <clib/exec_protos.h>` — SAS/C only, does not exist in vbcc NDK
- `src/Main.c` (was line 64)
- `src/main.h` (was line 4)
- The `<proto/exec.h>` already present in both files covers everything `clib/` provided.

### 2. Add `stricmp_custom()` — SAS/C built-in, not in vbcc
Add to `src/utilities.h` (alongside the existing `strncasecmp_custom` declaration):
```c
int stricmp_custom(const char *s1, const char *s2);
```
Add implementation to `src/utilities.c` (next to `strncasecmp_custom`):
```c
int stricmp_custom(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        int d = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (d != 0) return d;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
```
Replace 3 call sites:
- `src/utilities.c` — `stricmp(*(const char **)a, *(const char **)b)` in `Compare()`
- `src/Main.c` — `stricmp(&fileName[nameLen - 4], ".zip")`
- `src/Main.c` — `stricmp(file_name + name_len - 4, ".xml")`

### 3. Fix `malloc()` in `convertWBVersionWithDot()` in `utilities.c`
Replace `malloc(...)` with `amiga_malloc(...)` and `free(result)` calls with `amiga_free(result)`.
Add `#include "platform/platform.h"` to `utilities.c` if not already present.

### 4. Remove dead WB version detection code (WB 3.0+ floor)
In `utilities.c`:
- Delete `get_kickstart_version()` entirely (only existed for pre-2.0 detection)
- Simplify `get_workbench_version()` — remove the Kickstart version guard block at the top.
  The function can start directly with opening `dos.library`.
- In `LookupWorkbenchVersion()` — optionally trim the 1.x / 2.x / 3.5 / 3.9 / OS4 cases
  from the switch, keeping only 3.0+ entries. Not strictly required, just tidying.

### 5. Replace AllocVec/FreeVec with amiga_malloc/amiga_free (whole project)
Do a search for `AllocVec(` and `FreeVec(` across all migrated `.c` files and replace with
`amiga_malloc(` / `amiga_free(`. Remember:
- `AllocVec(size, MEMF_CLEAR)` → `amiga_malloc(size)` then `memset(ptr, 0, size)` if zeroing is needed,
  OR just `amiga_malloc(size)` if the code initialises the memory itself.
- `AllocVec(size, 0)` → `amiga_malloc(size)` directly.
- `FreeVec(ptr)` → `amiga_free(ptr)`.

---

## Makefile changes required

Edit `Makefile` at the top of the file:
```makefile
APP_NAME = WHDDownloader
PROJECT  = WHDDownloader
```

Add the Roadshow SDK include path and download library include path to `CFLAGS`.
Current CFLAGS line (from AmigaBase):
```makefile
CFLAGS = +aos68k -c99 -cpu=68000 -O2 -size \
         -I$(SRC_DIR) \
         -DPLATFORM_AMIGA=1 \
         -D__AMIGA__ \
         -DDEBUG \
         $(CONSOLE_FLAG) \
         $(MEMTRACK_FLAG)
```
Change to:
```makefile
# Set these to match your local installation paths
ROADSHOW_INC = C:/Amiga/Roadshow-SDK-1.8/netinclude
NDK_INC      = C:/Amiga/AmigaIncludes

CFLAGS = +aos68k -c99 -cpu=68000 -O2 -size \
         -I$(SRC_DIR) \
         -I$(NDK_INC) \
         -I$(ROADSHOW_INC) \
         -DPLATFORM_AMIGA=1 \
         -D__AMIGA__ \
         -DDEBUG \
         $(CONSOLE_FLAG) \
         $(MEMTRACK_FLAG)
```

Add all new source files to `SRCS` and `OBJS`, and add their build subdirectories
to the `directories` target. New entries needed:

**SRCS additions:**
```makefile
    $(SRC_DIR)/Main.c \
    $(SRC_DIR)/utilities.c \
    $(SRC_DIR)/gamefile_parser.c \
    $(SRC_DIR)/tag_text.c \
    $(SRC_DIR)/cli_utilities.c \
    $(SRC_DIR)/linecounter.c \
    $(SRC_DIR)/download/http_download.c \
    $(SRC_DIR)/download/download_lib.c \
    $(SRC_DIR)/download/file_crc.c \
    $(SRC_DIR)/download/timer_shared.c \
    $(SRC_DIR)/download/display_message.c \
```

**OBJS additions** (matching the above):
```makefile
    $(OUT_DIR)/Main.o \
    $(OUT_DIR)/utilities.o \
    $(OUT_DIR)/gamefile_parser.o \
    $(OUT_DIR)/tag_text.o \
    $(OUT_DIR)/cli_utilities.o \
    $(OUT_DIR)/linecounter.o \
    $(OUT_DIR)/download/http_download.o \
    $(OUT_DIR)/download/download_lib.o \
    $(OUT_DIR)/download/file_crc.o \
    $(OUT_DIR)/download/timer_shared.o \
    $(OUT_DIR)/download/display_message.o \
```

**directories target** — add:
```makefile
    @if not exist "$(OUT_DIR)\download"  mkdir "$(OUT_DIR)\download"
```

**Compile rules** — add one rule per new `.c` file following the existing pattern:
```makefile
$(OUT_DIR)/Main.o: $(SRC_DIR)/Main.c
	$(CC) $(CFLAGS) -c $< -o $@
# ... etc for each file
```

---

## Add `amiga_download.h` include path note

`src/Main.c` currently includes `"amiga_download.h"`. After migration this becomes
`"download/amiga_download.h"` — update the include in `Main.c`.

---

## Log categories to add (optional but recommended)

In `src/log/log.h`, add before `LOG_ERRORS`:
```c
LOG_DOWNLOAD,   /* HTTP download operations */
LOG_PARSER,     /* DAT file / HTML parsing  */
```
Add matching strings to `g_categoryNames[]` in `src/log/log.c`.

---

## What is NOT needed from AmigaBase

- The ReAction skeleton code in `src/main.c` — delete it once `src/Main.c` is in place
- No ReAction libraries need opening (this is a pure CLI tool)
- `CONSOLE_*` macros are optional — `Main.c` uses `printf` directly with ANSI colour codes

---

## Build environment paths (verify these exist on this machine)

- VBCC: `%VBCC%` env var should be set, `vc.exe` in PATH
- NDK 3.2 headers: likely `C:\Amiga\AmigaIncludes` or similar
- Roadshow SDK: likely `C:\Amiga\Roadshow-SDK-1.8\netinclude` or similar

Check with:
```powershell
where.exe vc
$env:VBCC
```

---

## Current state

- [x] AmigaBase cloned into `C:\Amiga\Programming\WHDDownloader`
- [x] New GitHub repo `Kwezza/WHDDownloader` created and pushed
- [x] Makefile updated (APP_NAME, SRCS, OBJS, include paths)
- [x] `src/download/` created and download library files copied in
- [x] Old project source files copied into `src/`
- [x] AmigaBase `src/main.c` deleted (replaced by `src/Main.c`)
- [x] Source fixes applied (clib headers, stricmp, malloc, WB version cleanup)
- [x] AllocVec/FreeVec → amiga_malloc/amiga_free sweep
- [x] First build attempt
- [x] Fix any remaining build errors
