# amiga-base - GitHub Copilot Instructions

## Project Overview

**amiga-base** is a minimal AmigaOS starter project providing:
- Memory allocation/deallocation tracking (`amiga_malloc`/`amiga_free`)
- Multi-category file logging
- A working ReAction window skeleton

Target: **AmigaOS / Workbench 3.2+ (V47+)**, compiled with **VBCC v0.9x**, CPU **68000+**.

---

## Project Structure

```
amiga-base/
├── Makefile
├── src/
│   ├── main.c                  <- entry point, ReAction skeleton
│   ├── console_output.h        <- conditional printf macros
│   ├── platform/
│   │   ├── platform.h          <- amiga_malloc/free macros + AMIGA_APP_NAME
│   │   ├── platform.c          <- memory tracking implementation
│   │   ├── amiga_headers.h     <- all Amiga system includes (guarded)
│   │   └── platform_types.h    <- platform_bptr type abstraction
│   └── log/
│       ├── log.h               <- LogCategory enum, log_* macros
│       └── log.c               <- logging implementation
├── build/amiga/                <- compiler output (gitignored)
└── Bin/Amiga/                  <- final executables (gitignored)
```

---

## Build

```powershell
# Standard release build
make

# With console window (debug)
make CONSOLE=1

# With memory leak tracking
make MEMTRACK=1

# Both
make CONSOLE=1 MEMTRACK=1

# Set your app name (overrides AMIGA_APP_NAME default)
make APP_NAME=MyTool PROJECT=MyTool

# Clean
make clean
```

Build output: `Bin/Amiga/<PROJECT>`  
Log output at runtime: `PROGDIR:logs/`

---

## Changing the App Name

Edit `src/platform/platform.h` line ~24:

```c
#ifndef AMIGA_APP_NAME
    #define AMIGA_APP_NAME "MyTool"
#endif
```

Also edit the top of `Makefile` to set the output binary name:

```makefile
APP_NAME = MyTool   /* informational only - does NOT set AMIGA_APP_NAME */
PROJECT  = MyTool
```

`AMIGA_APP_NAME` appears in:
- The window title (`WA_Title`)
- The OOM emergency requester dialog

**Note:** VBCC on Windows strips quotes from `-D` string values on the command line,
so `AMIGA_APP_NAME` cannot be set via a Makefile `-D` flag. Edit `platform.h` directly.

---

## ⚠️ GUI Framework: ReAction (Workbench 3.2+)

All GUI code uses **ReAction** classes. Never use GadTools.

### Required libraries (opened in `main.c`)

| Library | Class getter |
|---------|-------------|
| `window.class` | `WINDOW_GetClass()` |
| `gadgets/layout.gadget` | `LAYOUT_GetClass()` |
| `gadgets/button.gadget` | `BUTTON_GetClass()` |

### Core pattern

```c
/* Create objects */
Object *btn = NewObject(ButtonClass, NULL,
    GA_ID,        MY_GADGET_ID,
    GA_Text,      "Click Me",
    GA_RelVerify, TRUE,
    TAG_DONE);

Object *layout = NewObject(LayoutClass, NULL,
    LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
    LAYOUT_SpaceOuter,  TRUE,
    LAYOUT_AddChild,    btn,
    TAG_DONE);

Object *win = NewObject(WindowClass, NULL,
    WA_Title,        AMIGA_APP_NAME,
    WA_CloseGadget,  TRUE,
    WINDOW_Position, WPOS_CENTERSCREEN,
    WINDOW_Layout,   layout,
    TAG_DONE);

/* Open */
struct Window *w = (struct Window *)RA_OpenWindow(win);

/* Event loop */
while ((result = RA_HandleInput(win, &code)) != WMHI_LASTMSG) {
    switch (result & WMHI_CLASSMASK) {
        case WMHI_CLOSEWINDOW: done = TRUE; break;
        case WMHI_GADGETUP:
            switch (result & WMHI_GADGETMASK) {
                case MY_GADGET_ID: /* handle */ break;
            }
            break;
    }
}

/* ONLY dispose top-level object - children disposed automatically */
DisposeObject(win);
```

### CRITICAL cleanup rule

**Never manually dispose child objects.** `DisposeObject(window_obj)` disposes the entire tree.

---

## Memory System

### API

| Macro / Function | Purpose |
|-----------------|---------|
| `amiga_malloc(size)` | Allocate memory (tracked when `MEMTRACK=1`) |
| `amiga_free(ptr)` | Free memory |
| `amiga_strdup(str)` | Duplicate a string |
| `amiga_memory_init()` | Call once at startup (no-op in release) |
| `amiga_memory_report()` | Print leak report to `memory_*.log` (no-op in release) |
| `amiga_memory_suspend_logging()` | Silence memory log during bulk ops |
| `amiga_memory_resume_logging()` | Re-enable memory log |

### Fast RAM

On Amiga, `amiga_malloc()` uses `AllocVec(size, MEMF_ANY | MEMF_CLEAR)` which prefers Fast RAM automatically — up to **15x faster** than Chip RAM on expanded systems.

### Emergency OOM handler

`amiga_memory_init()` pre-allocates a 1KB Chip RAM buffer and opens `RAM:CRITICAL_FAILURE.log`. If the system runs out of memory an `EasyRequest` dialog appears with `AMIGA_APP_NAME` in the title before the program exits with code 20 (`RETURN_FAIL`).

### Release vs debug builds

```c
/* MEMTRACK=0 (default, release) - these expand to nothing */
amiga_memory_init();       /* -> ((void)0) */
amiga_memory_report();     /* -> ((void)0) */
amiga_malloc(128);         /* -> malloc(128) */
amiga_free(ptr);           /* -> free(ptr) */
```

---

## Logging System

### Log Categories

```c
typedef enum {
    LOG_GENERAL,   /* General program flow                */
    LOG_MEMORY,    /* Memory allocs/frees (high-frequency) */
    LOG_APP,       /* Application-specific (add more here) */
    LOG_ERRORS,    /* Auto-receives copies of all errors   */
    LOG_CATEGORY_COUNT
} LogCategory;
```

**Adding categories for your project:**
1. Add new entries to the enum above `LOG_ERRORS` in `log.h`
2. Add matching strings to `g_categoryNames[]` in `log.c` in the same position
3. Update `LOG_CATEGORY_COUNT` — it stays last automatically

### Log macros

```c
log_debug(LOG_GENERAL,  "Scanning: %s\n", path);
log_info(LOG_APP,       "Window opened\n");
log_warning(LOG_APP,    "File not found: %s\n", name);
log_error(LOG_GENERAL,  "Critical failure: %ld\n", (long)ioErr);
```

### Log files (written to `PROGDIR:logs/`)

```
general_YYYY-MM-DD_HH-MM-SS.log
memory_YYYY-MM-DD_HH-MM-SS.log
app_YYYY-MM-DD_HH-MM-SS.log
errors_YYYY-MM-DD_HH-MM-SS.log  <- all ERROR-level messages from every category
```

### Initialisation / shutdown

```c
/* In main(), before any log_* calls */
initialize_log_system(FALSE);   /* FALSE = keep old logs */

/* At end of main(), after amiga_memory_report() */
shutdown_log_system();
```

### Runtime control

```c
set_global_log_level(LOG_LEVEL_INFO);         /* suppress DEBUG globally */
enable_log_category(LOG_MEMORY, FALSE);        /* silence memory category */
set_memory_logging_enabled(TRUE);              /* re-enable + force DEBUG level */
```

---

## Language and Coding Standards

- **Compiler**: VBCC v0.9x with `+aos68k` configuration
- **Standard**: C99 (`-c99` flag) — `//` comments and inline declarations are OK
- **Naming**: `snake_case` for all functions, variables, types
- **Prefixes**: `amiga_` for library functions; `AMIGA_` for macros
- **Booleans**: Use AmigaOS `BOOL`, `TRUE`, `FALSE` (not C99 `bool`)
- **Strings**: ASCII only — no Unicode. Amiga Topaz font is ASCII-only.
- **Indentation**: 4 spaces (no tabs)

### AmigaOS type reference

```c
BOOL    /* 2-byte boolean */
STRPTR  /* char *         */
BPTR    /* DOS file/lock  */
LONG    /* 32-bit signed  */
ULONG   /* 32-bit unsigned */
WORD    /* 16-bit signed  */
UWORD   /* 16-bit unsigned */
```

### Structure field ordering (68k alignment)

Group fields by size to minimise padding:
```c
typedef struct {
    /* 4-byte fields first */
    ULONG  some_value;
    char  *some_string;
    Object *some_object;
    /* 2-byte fields last (BOOL = 2 bytes on Amiga) */
    BOOL   is_active;
    BOOL   is_valid;
} MyStruct;
```

### 68000 stack frame limit

VBCC allocates locals for **all** `case` and `if` branches simultaneously at function entry. Keep any single function's local variable frame under ~32 KB. Never put two large structs (> ~4 KB each) on the stack in the same function — heap-allocate with `amiga_malloc()` instead.

---

## Memory Management Rules

1. **Always use `amiga_malloc`/`amiga_free`** for general heap allocations in production code
2. **Always check for NULL** after allocation
3. **Free in reverse allocation order** where practical
4. Match correctly:
   - `amiga_malloc` → `amiga_free`
   - `AllocDosObject` → `FreeDosObject`
   - `AllocVec` → `FreeVec`
   - `NewObject` → `DisposeObject` (disposes children)
   - `Lock` → `UnLock`

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

## Development Environment

- **Host**: Windows PC
- **Compiler**: VBCC cross-compiler (`vc`)
- **SDK**: NDK 3.2 (includes ReAction classes)
- **Emulator**: WinUAE — shared drive maps `build\amiga` into Amiga filesystem
- **Target OS**: Workbench 3.2+

### Build and test cycle

1. Edit source on PC in VS Code
2. `make` in PowerShell terminal
3. Binary auto-appears in WinUAE shared drive
4. Run in WinUAE, check `PROGDIR:logs/`
5. For leaks: `make MEMTRACK=1`, check `memory_*.log`

---

## Checklist: Starting a New Project from amiga-base

- [ ] Edit `APP_NAME` and `PROJECT` in `Makefile`
- [ ] Rename/replace `main.c` window title and gadget IDs
- [ ] Add project-specific log categories to `LOG_CATEGORY_COUNT` in `log.h` + `log.c`  
- [ ] Add your source files to `SRCS` and `OBJS` in `Makefile`
- [ ] Add any new `build/amiga/<subdir>` entries to the `directories` target
- [ ] `make CONSOLE=1 MEMTRACK=1` for first build to verify no leaks
- [ ] Verify in WinUAE before declaring done

---

## ReAction Gadget Quick Reference

| Class | Purpose | Key Tags |
|-------|---------|----------|
| `button.gadget` | Push / toggle buttons | `GA_Text`, `GA_RelVerify` |
| `checkbox.gadget` | Boolean toggle | `GA_Selected`, `GA_Text` |
| `string.gadget` | Text input | `STRINGA_TextVal`, `STRINGA_MaxChars` |
| `integer.gadget` | Numeric input | `INTEGER_Number`, `INTEGER_Minimum` |
| `chooser.gadget` | Dropdown / cycle | `CHOOSER_Labels`, `CHOOSER_Selected` |
| `listbrowser.gadget` | Scrollable list | `LISTBROWSER_Labels`, `LISTBROWSER_Selected` |
| `layout.gadget` | Automatic arrangement | `LAYOUT_Orientation`, `LAYOUT_AddChild` |
| `requester.class` | Modal dialog | `REQ_Type`, `REQ_BodyText`, `REQ_GadgetText` |

Layout orientations: `LAYOUT_ORIENT_HORIZ`, `LAYOUT_ORIENT_VERT`

AutoDocs for all classes are in `docs/AutoDocs/` from the 3.2 NDK.

The document docs\Reaction_tips.md contains common patterns and gotchas for ReAction development.