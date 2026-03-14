# amiga-base

Minimal AmigaOS starter project for VBCC / Workbench 3.2+ development.

Provides out-of-the-box:
- **Memory tracking** — `amiga_malloc`/`amiga_free` with leak detection (`MEMTRACK=1`)
- **Multi-category logging** — timestamped log files in `PROGDIR:logs/`
- **ReAction window skeleton** — a working window that compiles and runs immediately
- **Conditional printf** — `CONSOLE_*` macros that produce zero overhead in release builds

## Requirements

| Tool | Version |
|------|---------|
| VBCC cross-compiler | 0.9x |
| NDK | 3.2 |
| Workbench (target) | 3.2+ |
| CPU (target) | 68000+ |

## Quick Start

```powershell
# 1. Clone
git clone https://github.com/Kwezza/amiga-base.git
cd amiga-base

# 2. Set your app name (edit Makefile lines 22-23)
#    APP_NAME = MyTool
#    PROJECT  = MyTool

# 3. Build
make

# 4. Find your binary at Bin/Amiga/<PROJECT>
#    Copy it to your WinUAE shared drive and run
```

## Build Options

```powershell
make                    # release (no console, no memtrack)
make CONSOLE=1          # open console window with printf output
make MEMTRACK=1         # enable allocation/leak tracking
make CONSOLE=1 MEMTRACK=1
make clean
make help
```

## Customising for Your Project

### 1. App name
Edit the top two lines of `Makefile`:
```makefile
APP_NAME = MyTool
PROJECT  = MyTool
```
`AMIGA_APP_NAME` flows through to the window title and OOM dialog automatically.

### 2. Add your source files
Add `.c` files to `SRCS` and matching `.o` entries to `OBJS` in the Makefile.  
Add build subdirectory creation to the `directories` target.

### 3. Add log categories
In `src/log/log.h` add new entries **before** `LOG_ERRORS`:
```c
typedef enum {
    LOG_GENERAL,
    LOG_MEMORY,
    LOG_APP,
    LOG_MY_MODULE,   // <- add here
    LOG_ERRORS,
    LOG_CATEGORY_COUNT
} LogCategory;
```
In `src/log/log.c` add a matching string to `g_categoryNames[]` at the same index.

### 4. Add a version string (Amiga `version` command)

To make your binary recognisable by the Amiga shell `version` command, add the following near the top of `src/main.c` (before any functions):

```c
// #define VERSION_STRING "$VER: MyTool 1.0 (06.03.2026)"
static const char version_string[] = "$VER: MyTool " MYAPP_VERSION " (" __DATE__ ")";
const char version[] = MYAPP_VERSION;
```

- The `$VER:` tag is scanned by the Amiga's `version` command to display name, version number, and build date.
- Replace `MyTool` with your app name and define `MYAPP_VERSION` (e.g. `"1.0"`) near the top of the file or in a dedicated header.
- The commented-out `#define` line is a handy reference for the expected format.
- `__DATE__` is substituted by the compiler at build time (format: `Mmm DD YYYY`); reformat manually if you need the Amiga `DD.MM.YYYY` date style.

### 5. Replace the skeleton window
`src/main.c` contains a minimal single-window example. Build your own windows alongside it following the patterns in `.github/copilot-instructions.md`.

## Log Files

At runtime, logs are written to `PROGDIR:logs/`:

```
general_YYYY-MM-DD_HH-MM-SS.log
memory_YYYY-MM-DD_HH-MM-SS.log   (only when MEMTRACK=1)
app_YYYY-MM-DD_HH-MM-SS.log
errors_YYYY-MM-DD_HH-MM-SS.log   (receives copies of all ERRORs)
```

## Memory Tracking

```c
void *buf = amiga_malloc(1024);   // tracked alloc
amiga_free(buf);                  // tracked free

// At program exit:
amiga_memory_report();            // writes leak report to memory_*.log
```

In release builds (`MEMTRACK=0`) all tracking code is compiled out — `amiga_malloc` becomes `malloc`, `amiga_free` becomes `free`, and the report function is a no-op.

## Copilot Instructions

`.github/copilot-instructions.md` contains the full reference for AI-assisted development: API summary, ReAction patterns, coding standards, and a project-start checklist.

## WHDDownloader INI Configuration

WHDDownloader supports optional runtime configuration from `PROGDIR:WHDDownloader.ini`.

- Sample file: `docs/WHDDownloader.ini.sample`
- Per-key runtime verification checklist: `docs/ini_runtime_test_matrix.md`

Precedence order:

1. Built-in defaults
2. INI overrides
3. CLI arguments

When no INI file exists, behavior remains unchanged.
