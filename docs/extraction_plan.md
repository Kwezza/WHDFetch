# WHDDownloader — Post-Download Extraction Plan

## Summary

Integrate LHA archive extraction directly into WHDDownloader so that
downloaded WHDLoad archives are automatically extracted after download.
This removes the need for the user to run WHDArchiveExtractor as a
separate step.

---

## Current Behaviour

1. WHDDownloader downloads `.lha` archives into a folder structure:

   ```
   GameFiles/
   ├── Games/
   │   ├── A/
   │   │   ├── Aladdin_v1.0.lha
   │   │   └── Area88_v1.2_Hack_by_Earok.lha
   │   └── C/
   │       └── Castlevania_v1.2_AGA_November2025.lha
   ├── Games Beta/
   │   └── A/
   │       └── AbuSimbelProfanation_v1.0_AGA.lha
   ├── Demos/
   ├── Demos Beta/
   └── Magazines/
   ```

2. The user must then run **WHDArchiveExtractor** separately to extract
   all archives into a target folder.

---

## Planned Behaviour

After each successful archive download, WHDDownloader will:

1. **Extract the archive** using `c:lha` to a target directory.
2. **Write an `ArchiveName.txt`** metadata file inside the extracted game
   folder.
3. **Optionally delete the `.lha` archive** once extraction succeeds.

---

## Extraction Target — Two Modes

### Mode 1: In-Place Extraction (Default)

When no `EXTRACTTO` argument is provided, archives are extracted into the
same directory where they were downloaded:

```
GameFiles/Games/A/Area88_v1.2_Hack_by_Earok.lha   ← downloaded
GameFiles/Games/A/Area88/                           ← extracted by lha
    Area88.Slave
    Area88.info
    ...
    ArchiveName.txt                                 ← written by WHDDownloader
```

With the default `delete after extract` behaviour, the `.lha` file is
deleted after successful extraction, leaving only the game folder.

**Why this is the default:** Most WinUAE users have a single shared
drive. Extracting in-place and deleting the archive keeps things simple —
no extra configuration, no wasted disk space, and the `GameFiles/`
directory ends up containing ready-to-play WHDLoad folders.

### Mode 2: Extract to a Separate Path (`EXTRACTTO=<path>`)

When `EXTRACTTO` is provided (e.g. `EXTRACTTO=FUN:` or
`EXTRACTTO=Games:`), the program recreates the same directory structure
under the target path:

```
Source:  GameFiles/Games/A/Area88_v1.2_Hack_by_Earok.lha
Target:  FUN:Games/A/Area88/
             Area88.Slave
             Area88.info
             ...
             ArchiveName.txt
```

The subdirectory hierarchy (`Games/A/`) is preserved under the target
path, keeping games organised by pack and first letter, matching the
source layout.

**Why support a separate target:** On real Amiga hardware, users often
have archives on one partition and their WHDLoad games on another (e.g. a
dedicated `Games:` volume). With `EXTRACTTO`, the user can download to
one drive and extract directly to their WHDLoad games volume.

**Planned default behavior (future phase):** when `EXTRACTTO` is set, the
default will flip to keeping archives. This preserves a backup in
`GameFiles/`. The user can still force deletion with `DELETEARCHIVES`.

**Current implementation note (Phase 1-5):** archive deletion behavior
follows `delete_archives_after_extract` unless overridden by CLI.

---

## CLI Arguments

| Argument | Effect |
|----------|--------|
| `NOEXTRACT` | Disable extraction entirely (download only, legacy behaviour) |
| `EXTRACTTO=<path>` | Extract to a separate target path instead of in-place |
| `KEEPARCHIVES` | Keep `.lha` files after extraction (overrides default delete) |
| `DELETEARCHIVES` | Delete `.lha` files after extraction (overrides default keep when `EXTRACTTO` is set) |
| `EXTRACTONLY` | Extract archives that already exist in `GameFiles/` without downloading |

### Default Behaviour Matrix

| Mode | Archives deleted? | Rationale |
|------|-------------------|-----------|
| In-place (no `EXTRACTTO`) | Yes | Games are right there; archive is redundant |
| Separate target (`EXTRACTTO`) | No | Archive in `GameFiles/` is the only local copy |

Both defaults can be overridden by `KEEPARCHIVES` or `DELETEARCHIVES`.

---

## INI Configuration

New keys in `[global]` section of `WHDDownloader.ini`:

```ini
[global]
; Enable or disable extraction (default: true)
extract_archives=true

; Path to extract archives to (default: empty = in-place extraction)
; Example: extract_path=Games:
;          extract_path=FUN:
extract_path=

; Delete archives after successful extraction (default: true for in-place,
; false when extract_path is set)
delete_archives_after_extract=true
```

CLI arguments override INI settings. INI settings override built-in
defaults.

---

## ArchiveName.txt — Future Version Checking

After extraction, an `ArchiveName.txt` file is written into the game's
top-level folder:

```
ArchiveName.txt contents:
─────────────────────────
Games
Area88_v1.2_Hack_by_Earok.lha
```

- **Line 1:** Category name (`Games`, `Games (Beta & Unofficial)`,
  `Demos`, `Demos (Beta & Unofficial)`, `Magazines`)
- **Line 2:** Original archive filename

### Why this file exists

This file enables a future enhancement: **automatic update detection**.

The planned workflow:

1. WHDDownloader downloads the latest DAT file listing all current
   archives and their versions.
2. Before downloading, the program scans the extraction target for
   existing game folders.
3. For each game folder found, it reads `ArchiveName.txt` to determine:
   - Which category the game belongs to (so it checks the right DAT
     file).
   - Which archive version is currently installed (from the filename,
     e.g. `v1.2`).
4. It compares the installed version against the latest version in the
   DAT file.
5. Only games with newer versions are re-downloaded and re-extracted.

This turns WHDDownloader from a `download everything new` tool into a
`keep your library up to date` tool — especially valuable on real Amiga
hardware where bandwidth and disk space are limited.

### Folder Name Derivation — Current Approach and Future Improvement

The extracted folder name is derived from the archive filename by taking
everything before the first underscore:

```
Area88_v1.2_Hack_by_Earok.lha  →  Area88
Castlevania_v1.2_AGA.lha       →  Castlevania
DonkeyKong_v1.2.lha            →  DonkeyKong
```

This is a heuristic that works for the vast majority of WHDLoad archives
because the naming convention is consistent. However, it is not
guaranteed to match the actual top-level directory inside the `.lha`
file.

**Future improvement (noted in code):** The correct approach is to list
the archive contents before extracting:

```
lha vq "<archive>" >ram:listing.txt
```

Then parse `ram:listing.txt` to find the actual first directory entry.
This is exactly what WHDArchiveExtractor already does for its protection-
bit reset logic. This should be implemented and tested in a future
update, replacing the filename heuristic.

---

## Extraction Flow (Per Archive)

```
1. Download completes successfully (direct HTTP result code 0)
       │
2. Is extraction enabled? (`extract_archives == TRUE`)
       │ No → skip, return
       │ Yes ↓
3. Determine extract target directory:
       │ - EXTRACTTO set? → <EXTRACTTO>/<pack_dir>/<letter>/
       │ - In-place?      → same dir as the .lha file
       │
4. Create target directories if needed
       │
5. Derive game folder name (text before first '_' in archive name)
       │  *** FUTURE: use lha vq listing instead ***
       │
6. Does target game folder already exist?
       │ Yes → reset protection bits (protect <folder>/#? ALL rwed >NIL:)
       │ No  → continue
       │
7. Execute: lha -T -M -N -m x "<archive>" "<target_dir>/"
       │
8. Check return code
       │ Non-zero → log error, skip metadata/deletion
       │ Zero ↓
       │
9. Write <target_dir>/<game_folder>/ArchiveName.txt
       │   Line 1: category name
       │   Line 2: archive filename
       │
10. Should archive be deleted?
       │ Yes → DeleteFile(<archive_path>)
       │ No  → keep
```

---

## Graceful Degradation

- **`c:lha` not installed while extraction is enabled:** Startup fails
       with an error. Users can run download-only mode with `NOEXTRACT`.

- **Extraction fails (corrupt archive, disk full):** The error is
  logged. The archive is **not** deleted (regardless of settings). The
  program continues with the next file.

- **`EXTRACTTO` path does not exist or is not writable:** Reported as an
       error at startup and the program exits (hard-fail policy).

---

## Files Changed

| File | Change |
|------|--------|
| **NEW** `src/extract/extract.c` | Extraction logic, folder name parsing, `ArchiveName.txt` writing |
| **NEW** `src/extract/extract.h` | Public API for the extraction module |
| `src/main.h` | New fields in `download_option` struct, new directory constant |
| `src/main.c` | Post-download hook, CLI argument parsing, help text, defaults, startup extraction validation |
| `src/ini_parser.c` | Parse new `[global]` keys for extraction settings |
| `docs/WHDDownloader.ini.sample` | Document new INI keys |
| `Makefile` | Add extract source/object files, build directory |

---

## Design Rationale Summary

| Decision | Rationale |
|----------|-----------|
| Extract after each download, not as a batch | If the user cancels mid-run, everything downloaded so far is already extracted and usable |
| Default to in-place extraction | Simplest for the common WinUAE case; zero configuration needed |
| Default to deleting archives in-place | Avoids doubling disk usage; extracted folder is the usable result |
| Default to keeping archives with `EXTRACTTO` | Archive in `GameFiles/` is the user's only backup copy |
| Separate `extract` module | Keeps `main.c` clean; extraction logic is self-contained and testable |
| `ArchiveName.txt` metadata file | Enables future version-checking without re-parsing filenames or re-downloading DAT files |
| Folder name from filename heuristic | Works now for 99%+ of WHDLoad archives; noted for future replacement with `lha vq` listing |
| `c:lha` and invalid `EXTRACTTO` checks are startup validations | Fail fast for misconfigured extraction; `NOEXTRACT` remains available for download-only mode |
