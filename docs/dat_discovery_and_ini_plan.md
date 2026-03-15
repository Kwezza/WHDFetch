# DAT File Discovery Pipeline — How the Program Finds and Downloads WHDLoad Packs

This document traces the complete journey from the initial HTML download to the final
ROM-by-ROM download, explaining every filter and matching step. It is written with the
goal of making these hard-coded values configurable via an INI settings file.

---

## 1. Overview of the five pack types

The program recognises five WHDLoad pack categories, indexed 0–4:

| Index | Constant     | Display name                  |
|-------|-------------|-------------------------------|
| 0     | DEMOS_BETA  | Demos (Beta & Unreleased)     |
| 1     | DEMOS       | Demos                         |
| 2     | GAMES_BETA  | Games (Beta & Unreleased)     |
| 3     | GAMES       | Games                         |
| 4     | MAGAZINES   | Magazines                     |

Each pack is described by a `whdload_pack_def` struct (defined in `main.h`) which
holds all the per-pack configuration needed for every stage:

```
download_url           – base URL for downloading individual ROM archives
extracted_pack_dir     – local sub-directory under GameFiles/ for this pack
filter_dat_files       – substring used to match DAT zip files in the HTML index
filter_zip_files       – (struct field exists but never correctly populated — see bug note)
full_text_name_of_pack – human-readable label shown in console output
```

These values are populated by `setup_app_defaults()` in `main.c` from the hard-coded
`const char *` variables at file scope.

---

## 2. Stage-by-stage pipeline

### Stage 1 — Download the index page

**Function:** `main()` (around line 358)

The program downloads the Turran FTP directory listing:

```
DOWNLOAD_WEBSITE = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/"
```

The HTML is saved to `temp/index.html`. This is a standard Apache-style auto-index
page containing `<a href="...">` links to every file and sub-directory.

### Stage 2 — Scan the HTML for matching DAT zip links

**Functions:** `extract_and_validate_HTML_links()` → `download_WHDLoadPacks_From_Links()`  
(main.c lines ~683–779)

The HTML file is read line-by-line. For each line the code:

1. Looks for `<a href="` tags.
2. Extracts the `href` value (the raw link text).
3. Checks that the link contains the pattern `Commodore%20Amiga` **and** the word
   `WHDLoad` — this filters out non-WHDLoad entries like JST or HD Loaders.
4. Cleans the link to produce a simplified `fileName` used for filter matching.

#### The cleaning steps (in order)

Starting from the raw href, e.g.:  
`Commodore%20Amiga%20-%20WHDLoad%20-%20Games%20-%20Beta%20&amp;%20Unofficial%20(2026-02-28).zip`

| Step | Operation | Purpose |
|------|-----------|---------|
| 1 | `remove_all_occurrences(link, "amp;")` | Decode HTML `&amp;` entities → `&` |
| 2 | Copy link → fileName | Work on a separate buffer |
| 3 | `remove_all_occurrences(fileName, FILE_PART_TO_REMOVE)` | Strip `"Commodore%20Amiga%20-%20WHDLoad%20-%20"` prefix |
| 4 | `remove_all_occurrences(fileName, "%20")` | Remove URL-encoded spaces |
| 5 | `remove_all_occurrences(fileName, "CommodoreAmiga-")` | Catch any residual prefix variation |
| 6 | `remove_all_occurrences(fileName, "&amp;")` | Second pass for any remaining entities |
| 7 | `remove_all_occurrences(fileName, "&Unreleased")` | Strip the old "&Unreleased" suffix |

After these steps, the **cleaned fileName** for each current (2025/2026) HTML link
looks like this:

| Raw href (decoded) | Cleaned fileName |
|---|---|
| `...WHDLoad - Demos (2025-09-20).zip` | `Demos(2025-09-20).zip` |
| `...WHDLoad - Demos - Beta & Unofficial (2025-07-24).zip` | `Demos-Beta&Unofficial(2025-07-24).zip` |
| `...WHDLoad - Games (2026-03-07).zip` | `Games(2026-03-07).zip` |
| `...WHDLoad - Games - Beta & Unofficial (2026-02-28).zip` | `Games-Beta&Unofficial(2026-02-28).zip` |
| `...WHDLoad - Magazines (2025-07-24).zip` | `Magazines(2025-07-24).zip` |

> **Note:** The old site used "Unreleased" so step 7 would strip `&Unreleased`
> leaving `Demos-Beta(...)`. The new site uses "Unofficial" so step 7 no longer
> strips anything, leaving `Demos-Beta&Unofficial(...)`. The filters still match
> because they use prefix/substring matching (see below), but this is fragile.

#### The filter matching

The code iterates all 5 pack definitions and checks:

```c
if (strstr(fileName, WHDLoadPackDefs[i].filter_dat_files))
```

The `filter_dat_files` values used for matching are:

| Pack | `filter_dat_files` (intended) | `filter_dat_files` (actual — see bug) |
|------|------|------|
| DEMOS_BETA | `"Demos-Beta("` | `"Demos-Beta"` (overwritten by ZIP filter) |
| DEMOS | `"Demos("` | `"Demos("` (overwritten but same value) |
| GAMES_BETA | `"Games-Beta"` | `"Games-Beta"` (overwritten but same value) |
| GAMES | `"Games("` | `"Games("` (overwritten but same value) |
| MAGAZINES | `"Magazines("` | `"Magazines"` (overwritten by ZIP filter) |

> **BUG:** In `setup_app_defaults()`, `filter_dat_files` is assigned twice for every
> pack — first with the DAT filter value, then immediately overwritten with the ZIP
> filter value. The struct has a separate `filter_zip_files` field that is never set.
> This means the program always uses the ZIP filter strings for matching.
> It still works because the ZIP filters are equal or more permissive substrings.

**Match results with the current HTML:**

| Cleaned fileName | Matching filter | Pack matched |
|---|---|---|
| `Demos(2025-09-20).zip` | `"Demos("` | DEMOS ✓ |
| `Demos-Beta&Unofficial(2025-07-24).zip` | `"Demos-Beta"` | DEMOS_BETA ✓ |
| `Games(2026-03-07).zip` | `"Games("` | GAMES ✓ |
| `Games-Beta&Unofficial(2026-02-28).zip` | `"Games-Beta"` | GAMES_BETA ✓ |
| `Magazines(2025-07-24).zip` | `"Magazines"` | MAGAZINES ✓ |

The filters currently work, but only because the substring matching is loose enough.
If Turran ever adds another pack starting with "Magazines" or "Games-Beta", false
positives could occur.

#### Date comparison and download decision

**Function:** `compare_and_decide_DatFileDownload()` (main.c ~781)

Once a filter matches and the user requested that pack:

1. Extract the date from the remote filename, e.g. `(2026-03-07)` → `2026-03-07`.
2. Scan the local `temp/Zip files/` directory for existing ZIPs matching the same
   filter string (`Get_latest_filename_from_directory()`).
3. Compare dates — if the remote is newer (or no local file exists), download it.
4. If downloaded, the ZIP is saved to `temp/Zip files/<cleanedFileName>`.

### Stage 3 — Extract DAT XML from the downloaded ZIPs

**Function:** `extract_Zip_file_and_rename()` (main.c ~833)

For each ZIP file in `temp/Zip files/`:

1. Check if the filename matches `filter_dat_files` for any pack where the user
   requested a download and `updated_dat_downloaded == 1`.
2. Extract the date from the ZIP filename.
3. Pipe the ZIP contents to an XML file in `temp/Dat files/`, e.g.:
   - Filter `"Games("` + date `2026-03-07` → `temp/Dat files/Games(2026-03-07).xml`
   - Filter `"Games-Beta"` + date `2026-02-28` → `temp/Dat files/Games-Beta(2026-02-28).xml`
4. The extraction uses `unzip -a -p` (pipe to stdout) redirected to the XML file.

### Stage 4 — Parse the XML DAT files to get ROM filenames

**Function:** `process_and_archive_WHDLoadDat_Files()` (main.c ~966)

For each `.xml` file in `temp/Dat files/`:

1. Match the filename against each pack's `filter_dat_files` string.
2. Call `extract_and_save_rom_names_from_XML()` which:
   - Reads the XML line by line looking for `<rom` tags.
   - Extracts the `name="..."` attribute from each `<rom>` tag.
   - Parses the filename into a `game_metadata` struct (machine type, language, etc.).
   - Applies user-selected filters (skip AGA, skip NTSC, skip CD, skip non-English).
   - Writes passing ROM names to a `.txt` file, e.g. `Games(2026-03-07).txt`.
3. Deletes the XML file after processing.

### Stage 5 — Download individual ROM archives

**Functions:** `download_roms_if_file_exists()` → `download_roms_from_file()` →
`execute_archive_download_command()` (main.c)

1. `get_first_matching_fileName()` scans `temp/Dat files/` for a `.txt` file whose
   name starts with the pack's `filter_dat_files` string.
2. Each line in the `.txt` file is a ROM filename (e.g. `AbandonShip_v1.0_0455.lha`).
3. For each ROM:
   - Build the download path: `<download_url><first-letter-folder>/<filename>`
   - Build the local save path: `GameFiles/<extracted_pack_dir>/<first-letter-folder>/<filename>`
   - Skip if the local file already exists.
   - Otherwise, download via the direct HTTP downloader.

---

## 3. What has changed on the Turran website

Comparing the hard-coded values against the **current** `index.html` (grabbed
March 2026):

### ZIP filename change: "Unreleased" → "Unofficial"

| Pack | Old href pattern | New href pattern |
|---|---|---|
| DEMOS_BETA | `...Demos - Beta & Unreleased (date).zip` | `...Demos - Beta & Unofficial (date).zip` |
| GAMES_BETA | `...Games - Beta & Unreleased (date).zip` | `...Games - Beta & Unofficial (date).zip` |

**Impact on ZIP detection:** Minimal — the filters `"Demos-Beta"` and `"Games-Beta"`
match by prefix, so both old and new names match.

**Impact on the cleanup step:** `remove_all_occurrences(fileName, "&Unreleased")` no
longer strips anything from the new filenames. The cleaned fileName now contains
`&Unofficial` instead of being stripped clean. This doesn't break matching but leaves
extra text in the local filename.

### Directory (folder) name change: "Unreleased" → "Unofficial"

| Pack | Old folder href | New folder href |
|---|---|---|
| DEMOS_BETA | `Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_%26_Unreleased/` | `Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_&_Unofficial/` |
| GAMES_BETA | `Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_%26_Unreleased/` | `Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_&_Unofficial/` |

### Download URL mismatch (BREAKING)

The hard-coded `download_url` values for the beta packs still point to the old
directory names:

```
WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED =
  ".../Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_%26_Unreleased/"

WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED =
  ".../Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_%26_Unreleased/"
```

The server directories are now named with `_Unofficial` not `_Unreleased`. If the
server doesn't have a redirect in place, **downloading individual beta ROMs will fail
with 404 errors**. These URLs need updating.

---

## 4. Known bug: `filter_dat_files` vs `filter_zip_files`

In `setup_app_defaults()`, every pack definition assigns `filter_dat_files` twice:

```c
WHDLoadPackDefs[DEMOS_BETA].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_BETA_AND_UNRELEASED;
WHDLoadPackDefs[DEMOS_BETA].filter_dat_files = WHDLOAD_FILE_FILTER_ZIP_BETA_AND_UNRELEASED;
//                          ^^^^^^^^^^^^^^^^ should be filter_zip_files on this line
```

The intended second assignment should be to `filter_zip_files`. The existing code
works by accident because both filter values are similar substrings, but the
distinction (DAT filters end with `"("`, ZIP filters don't) is lost.

---

## 5. Complete per-pack configuration table

This is everything that would need to be in an INI file, gathered from the hard-coded
constants and the `whdload_pack_def` struct:

| Setting | DEMOS_BETA | DEMOS | GAMES_BETA | GAMES | MAGAZINES |
|---|---|---|---|---|---|
| `download_url` | `http://ftp2.grandis.nu/.../Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_%26_Unreleased/` | `http://ftp2.grandis.nu/.../Commodore_Amiga_-_WHDLoad_-_Demos/` | `http://ftp2.grandis.nu/.../Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_%26_Unreleased/` | `http://ftp2.grandis.nu/.../Commodore_Amiga_-_WHDLoad_-_Games/` | `http://ftp2.grandis.nu/.../Commodore_Amiga_-_WHDLoad_-_Magazines/` |
| `extracted_pack_dir` | `Demos Beta/` | `Demos/` | `Games Beta/` | `Games/` | `Magazines/` |
| `filter_dat_files` | `Demos-Beta(` | `Demos(` | `Games-Beta` | `Games(` | `Magazines(` |
| `filter_zip_files` | `Demos-Beta` | `Demos(` | `Games-Beta` | `Games(` | `Magazines` |
| `full_text_name_of_pack` | `Demos (Beta & Unreleased)` | `Demos` | `Games (Beta & Unreleased)` | `Games` | `Magazines` |

Global settings (not per-pack):

| Setting | Current value | Used in |
|---|---|---|
| `DOWNLOAD_WEBSITE` | `http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/` | Index page URL |
| `FILE_PART_TO_REMOVE` | `Commodore%20Amiga%20-%20WHDLoad%20-%20` | Link cleaning step 3 |
| Link cleanup removals | `"amp;"`, `"%20"`, `"CommodoreAmiga-"`, `"&amp;"`, `"&Unreleased"` | Link cleaning steps 1,4,5,6,7 |
| HTML link pre-filters | `"Commodore%20Amiga"` and `"WHDLoad"` | Stage 2 initial filtering |

---

## 6. Proposed INI file design

### File location

`PROGDIR:WHDDownloader.ini`

The program should attempt to read this file at startup. If the file does not exist,
all current hard-coded defaults are used (zero behaviour change for existing users).

### Format

Standard INI with `[section]` headers and `key=value` pairs. No quotes around values.
Lines starting with `;` are comments. The AmigaOS-compatible format keeps parsing
simple — `fgets()` line-by-line, `strchr()` for `=`, `stricmp_custom()` for section
and key names.

### Example file

```ini
; WHDDownloader configuration
; Delete a key or the whole file to fall back to built-in defaults.
; This file is never written by the program — edit it manually.

[global]
; Base URL for the Turran index page
download_website=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/

; Prefix stripped from href links before filter matching
file_part_to_remove=Commodore%20Amiga%20-%20WHDLoad%20-%20

; Substrings removed from filenames during link cleaning (comma-separated)
; These are applied in order after file_part_to_remove
link_cleanup_removals=amp;,%20,CommodoreAmiga-,&amp;,&Unofficial

; Substrings the href must contain to be considered a WHDLoad link
html_link_prefix_filter=Commodore%20Amiga
html_link_content_filter=WHDLoad

[pack.demos_beta]
enabled=true
display_name=Demos (Beta & Unofficial)
download_url=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_&_Unofficial/
local_dir=Demos Beta/
filter_dat=Demos-Beta(
filter_zip=Demos-Beta

[pack.demos]
enabled=true
display_name=Demos
download_url=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Demos/
local_dir=Demos/
filter_dat=Demos(
filter_zip=Demos(

[pack.games_beta]
enabled=true
display_name=Games (Beta & Unofficial)
download_url=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_&_Unofficial/
local_dir=Games Beta/
filter_dat=Games-Beta
filter_zip=Games-Beta

[pack.games]
enabled=true
display_name=Games
download_url=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games/
local_dir=Games/
filter_dat=Games(
filter_zip=Games(

[pack.magazines]
enabled=true
display_name=Magazines
download_url=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Magazines/
local_dir=Magazines/
filter_dat=Magazines(
filter_zip=Magazines

[filters]
; Game content filters (overridden by CLI arguments)
skip_aga=false
skip_cd=false
skip_ntsc=false
skip_non_english=false
```

### Parsing strategy

```
1. At startup, before setup_app_defaults(), attempt to open PROGDIR:WHDDownloader.ini
2. If file not found → call setup_app_defaults() with all hard-coded values (current behaviour)
3. If file found → parse sections:
   a. [global] keys override DOWNLOAD_WEBSITE, FILE_PART_TO_REMOVE, and the
      removal list used in download_WHDLoadPacks_From_Links()
   b. [pack.*] sections populate the whdload_pack_def array entries
      - Map section names to indices: demos_beta=0, demos=1, games_beta=2, games=3, magazines=4
      - For any key not present in the INI, use the hard-coded default
   c. [filters] keys set the skip_AGA / skip_CD / skip_NTSC / skip_NonEnglish globals
      (CLI arguments should still override these)
4. Any pack with enabled=false is treated as if the user did not request it
```

### What needs to change in the code

1. **New file:** `src/ini_parser.c` / `ini_parser.h` — a small line-based INI reader.
   Only needs to handle `[section]`, `key=value`, `;` comments, and blank lines.
   Allocate string values using `amiga_strdup()` so they follow memory tracking.

2. **`setup_app_defaults()`** — restructure to:
   - First set all hard-coded defaults (unchanged).
   - Then call `load_ini_overrides(whdload_pack_defs, &download_options)` which
     overwrites any defaults with INI values where present.

3. **`download_WHDLoadPacks_From_Links()`** — the hard-coded removal strings
   (`"amp;"`, `"%20"`, etc.) should be read from a configurable list rather than
   being inline string literals. Store them in a global array populated from
   `link_cleanup_removals`.

4. **Fix the `filter_dat_files` / `filter_zip_files` bug** — the second assignment
   in `setup_app_defaults()` should target `filter_zip_files`. Both fields should
   then be populated from the INI's `filter_dat` / `filter_zip` keys.

5. **`DOWNLOAD_WEBSITE`** and **`FILE_PART_TO_REMOVE`** — change from `#define` to
   `const char *` globals so they can be overridden at runtime from the INI.

### Fallback behaviour

Every setting has a compiled-in default. The INI file is entirely optional:

- No INI file → exact current behaviour, no warnings.
- Partial INI file → only the keys present override defaults.
- Malformed lines → skip silently (log a warning to `LOG_GENERAL`).

---

## 7. Summary of immediate fixes needed (independent of INI)

These are issues discovered during this analysis that should be addressed regardless
of whether the INI system is implemented:

| # | Issue | Severity | Fix |
|---|---|---|---|
| 1 | `filter_dat_files` assigned twice, `filter_zip_files` never set | Bug | Change second assignment to `filter_zip_files` |
| 2 | Beta pack download URLs use `_Unreleased` but site now uses `_Unofficial` | **Breaking** | Update `WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED` and `WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED` |
| 3 | `remove_all_occurrences(fileName, "&Unreleased")` doesn't match `&Unofficial` | Cosmetic | Change to `"&Unofficial"` or remove both suffixes |
| 4 | Display names say "Unreleased" but packs are now "Unofficial" | Cosmetic | Update `WHDLOAD_TEXT_NAME_*` strings |
