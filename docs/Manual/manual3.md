# whdfetch

**A WHDLoad Archive Downloader and Installer for AmigaOS 3.0+**

This program is designed to set up an internet-connected Amiga with all the latest WHDLoad games, demos, and magazines from Retroplay's collection on the Turran server.   It allows pack selection, basic filtering, and can also be run again at a later date to keep your collection up to date by automatically detecting new and updated releases.

---

## Table of Contents

1. [Requirements](#requirements)
2. [Introduction](#introduction)
3. [Getting Started](#getting-started)
4. [How It Works](#how-it-works)
5. [Pack Selection](#pack-selection)
6. [Extraction Options](#extraction-options)
7. [Download Skip Options](#download-skip-options)
8. [Filter Options](#filter-options)
9. [Output and Reporting](#output-and-reporting)
10. [Maintenance Commands](#maintenance-commands)
11. [Connection Settings](#connection-settings)
12. [Icon Handling](#icon-handling)
13. [INI File Configuration](#ini-file-configuration)
14. [Session Reports](#session-reports)
15. [Skip Detection and Caching](#skip-detection-and-caching)
16. [Tips & Troubleshooting](#tips--troubleshooting)

---

## Requirements

- AmigaOS 3.0 or newer (3.1 or later recommended)
- TCP/IP stack (`bsdsocket.library v4` or higher) — required for all network operations
- `c:unzip` required to extract extract the DAT filess. Aminet: util/arc/UnZip.lha
- `c:lha` — required for `.util/arc/UnZip.lha` archive extraction.  Aminet: util/arc/lha
- `c:unlzx` — optional; required for `.lzx` archive extraction. If not installed, `.lzx` archives are skipped with a warning and the run continues. Aminet:  TBC
- Fast RAM recommended — Should work with at least 2MB memory but more is better.
- Enough hard drive space to store all the files.  A more modern file system, like PFS is recommended, as all the extracted archives can easily shoot past the 4GB limit.

---

## Introduction

One of my pet peeves of setting up a new Amiga install is getting all the WHDLoad games and demos set up.  This program is designed to download the latest WHDLoad packs from RetroPlay and the individual archives directly from the Turran server.  It allows you to choose one or more packs (Games, Demos, etc.) and offers very basic (for now) filtering by chipset, language, CD, etc.  It can then be told to extract the archive directly to a path of your choice, creating a logical file path (e.g., "Games/A/A10TankKiller3Disk") along with custom or default icons. You can choose whether to keep the downloaded archive on disk or delete it after extraction.  Running the program a second time on the same target path will scan it to see if there are any new or updated WHDLoad items, and download only what's needed.

If desired, the downloaded archive can have its CRC checked against the server’s CRC to verify that the download was successful. Extracting the archives directly on the Amiga (real or emulated) is the only current way I know of to ensure that special metadata in files and attributes is set correctly, which is why I wanted to create a tool like this.

This program is currently CLI only, and can be used with an optional .ini file to pass default settings, and adjust the filtering system and URLs if they should change in the future.

---

## Getting Started

1. Copy `whdfetch` to a directory on your Amiga. Copy `whdfetch.ini` to the same directory (or omit it to use built-in defaults).

2. Ensure your Amiga is currently connected to the internet.  I use Roadshow for my TCP/IP stack, but it's also been thoroughly tested with the bsdsocket.library in WinUAE.  Any other stack should theoretically work as long as you can access the internet.

3. Open a Shell in the directory where you placed `whdfetch`.

4. Run a first download.  Note the command below will download the largest WHDLoad pack with thousands of game archives in it.  Ahen running a download like this I normally leave the Amiga running overnight once im happy its downloading and extracting:

   ```
   whdfetch DOWNLOADGAMES
   ```

   Archives are downloaded to `GameFiles/Games/` organised into letter subfolders (`A/`, `B/`, `C/`, ...).

5. Check the output. A summary showing totals for new downloads, skipped titles, and extraction results is printed at the end of the run. A session report is also written to `PROGDIR:updates/`.

**Tip:** For a first run, consider using `NOEXTRACT` to download the archives without immediately extracting them. This lets you check disk space and examine what arrived before committing to extraction.

**Tip:** By default, archives are deleted after successful extraction. Use `KEEPARCHIVES` if you want to keep them as a local backup.

---

## How It Works

A whdfetch run follows a fixed sequence of steps for each selected pack:

1. **Fetch the index.** The program downloads `index.html` from the Retroplay FTP site and scans it for links matching the expected pack filename pattern.

2. **Download the pack ZIP.** Each matching pack link is a ZIP file containing a DAT (XML) file listing every archive in that pack, including filenames, sizes, and CRC values. The ZIP is downloaded to `temp/Zip files/` and extracted to `temp/Dat files/`.

3. **Parse the DAT file.** Every `<rom name="...">` entry in the XML is parsed. The filename is decoded to extract metadata: title, version, chipset requirement, video format, language codes, media type, and number of disks. Active filters are applied at this stage.

4. **Process each entry.** For each archive that passes the filters, whdfetch checks whether it is already present and up to date using the `.archive_index` cache and `ArchiveName.txt` markers. If the archive needs to be downloaded, an HTTP fetch is performed from the Retroplay FTP server directly to `GameFiles/<pack>/<letter>/`.

5. **Extract.** If extraction is enabled (the default), the archive is run through `c:lha` or `c:unlzx`. An `ArchiveName.txt` marker is written inside the extracted folder. Optional icon replacement follows.

6. **Report.** At the end of the run, per-pack statistics are printed to the console and a full session report is written to `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`.

The `.archive_index` cache is updated in memory throughout the run and flushed to disk on clean exit. Stale entries pointing to folders that no longer exist are pruned automatically on each flush.

---

## Pack Selection

whdfetch organises the Retroplay collection into five packs. You must select at least one pack per run using a CLI command or via the INI file. Running whdfetch with no pack selected and no other arguments displays the built-in help screen.

If any CLI pack-selection command is present, it overrides all pack selections in the INI file for that run.

**General syntax:**

```
whdfetch [COMMAND...] [OPTION...]
```

Arguments are case-insensitive and can be given in any order.

---

### DOWNLOADGAMES

Selects the **Games** pack. Archives are downloaded and extracted to `GameFiles/Games/<letter>/`.

```
whdfetch DOWNLOADGAMES
```

### DOWNLOADBETAGAMES

Selects the **Games (Beta & Unofficial)** pack. These are WHDLoad game conversions that are still in development or were produced by unofficial sources. Archives go to `GameFiles/Games Beta/<letter>/`.

```
whdfetch DOWNLOADBETAGAMES
```

### DOWNLOADDEMOS

Selects the **Demos** pack. Archives go to `GameFiles/Demos/<letter>/`.

```
whdfetch DOWNLOADDEMOS
```

### DOWNLOADBETADEMOS

Selects the **Demos (Beta & Unofficial)** pack. Archives go to `GameFiles/Demos Beta/<letter>/`.

```
whdfetch DOWNLOADBETADEMOS
```

### DOWNLOADMAGS

Selects the **Magazines** pack. Archives go to `GameFiles/Magazines/<letter>/`.

```
whdfetch DOWNLOADMAGS
```

### DOWNLOADALL

Selects all five packs at once. Equivalent to specifying each pack-selection command individually.

```
whdfetch DOWNLOADALL
```

### HELP

Prints the built-in usage screen and exits immediately without performing any downloads or extraction.

```
whdfetch HELP
```

---

## Extraction Options

By default, whdfetch extracts each archive immediately after downloading it. The extraction pipeline:

1. Runs `c:lha` for `.lha` archives, or `c:unlzx` for `.lzx` archives
2. Writes an `ArchiveName.txt` marker inside the extracted game folder
3. Applies custom drawer icons if available in `PROGDIR:Icons/`
4. Updates the `.archive_index` cache
5. Optionally deletes the archive file

The options below control how the extraction pipeline behaves.

---

### EXTRACTONLY

Processes only archives that are already present on disk in `GameFiles/<pack>/<letter>/`. Nothing is downloaded from the server.

This is intended for situations where you have previously fetched archives using `NOEXTRACT` and now want to extract them. The program reads the existing DAT list files from `temp/Dat files/`, checks each entry against disk, and runs extraction on archives it finds.

When `EXTRACTONLY` is active, extraction is forced on regardless of any other setting. If `NOEXTRACT` is also given, it is ignored.

```
whdfetch DOWNLOADGAMES EXTRACTONLY
```

Extracting all packs to an external volume:

```
whdfetch DOWNLOADALL EXTRACTONLY EXTRACTTO=Games:
```

---

### NOEXTRACT

Disables the extraction pipeline entirely. Archives are downloaded and left as `.lha` or `.lzx` files. No extractor is called, no `ArchiveName.txt` is written, no icons are applied.

This is useful for downloading a large batch now and extracting later, or when you want to use an external extraction tool. Because extraction is skipped, startup validation of extraction tools is also skipped, so the program runs even if `c:lha` is not installed.

Pre-download skip logic still works normally. If an `ArchiveName.txt` marker from a previous run matches an archive name, that download is still skipped.

```
whdfetch DOWNLOADGAMES NOEXTRACT
```

---

### EXTRACTTO=\<path\>

Redirects extraction output to a different location instead of extracting alongside the downloaded archives. The pack and letter directory structure is preserved under the given path:

```
<path>/<pack>/<letter>/<game folder>/
```

For example, `EXTRACTTO=Games:` with the Games pack extracts to `Games:Games/A/`, `Games:Games/B/`, and so on.

The path must already exist and be writable. The program validates this at startup and exits with an error if the path is missing or not writable. No downloads begin until validation passes.

Setting `EXTRACTTO=` with an empty value clears any active extraction path and reverts to in-place extraction.

```
whdfetch DOWNLOADGAMES EXTRACTTO=Games:
```

Extracting all packs to a separate partition:

```
whdfetch DOWNLOADALL EXTRACTTO=Work:WHDLoad/
```

---

### KEEPARCHIVES

Keeps archive files after successful extraction. By default, archives are deleted once they have been extracted. This overrides that default and any INI setting.

Useful if you want to maintain a local archive cache alongside the extracted games for backup or to re-extract later with different options.

`KEEPARCHIVES` and `DELETEARCHIVES` oppose each other. Only one should be used per run.

```
whdfetch DOWNLOADGAMES KEEPARCHIVES
```

---

### DELETEARCHIVES

Explicitly enables post-extraction archive deletion. This is the compiled default, so this command is mainly useful for overriding an INI setting that keeps archives.

If extraction fails for a particular archive, that archive is not deleted. Only successfully extracted archives are removed.

`DELETEARCHIVES` has no effect if `NOEXTRACT` is active, since deletion only occurs after a successful extraction pass.

```
whdfetch DOWNLOADGAMES DELETEARCHIVES
```

---

### FORCEEXTRACT

Bypasses the `ArchiveName.txt` skip check and re-extracts every archive, even when the marker already matches.

Normally, if a game folder contains an `ArchiveName.txt` whose second line matches the current archive filename, extraction is skipped. `FORCEEXTRACT` disables this check for the current run.

This does not affect the pre-download skip. If the archive has not been downloaded and the marker already matches, the download itself may still be skipped unless `FORCEDOWNLOAD` or `NODOWNLOADSKIP` is also used.

Use `FORCEEXTRACT` to re-apply updated icons to all game folders, repair a suspected incomplete extraction, or re-extract to a new `EXTRACTTO` path.

```
whdfetch DOWNLOADGAMES FORCEEXTRACT
```

Full rebuild — re-download and re-extract everything:

```
whdfetch DOWNLOADALL FORCEEXTRACT FORCEDOWNLOAD
```

---

## Download Skip Options

whdfetch has two layers of skip logic to avoid redundant work:

1. **Local archive skip** — if the `.lha` or `.lzx` file already exists on disk, the download is skipped and the existing archive is passed to the extraction pipeline.
2. **Extraction marker skip** — if no local archive exists but an `ArchiveName.txt` marker in the extracted game folder matches the archive filename, both the download and extraction are skipped. This uses the `.archive_index` cache for speed.

The options below control the second layer.

---

### NODOWNLOADSKIP

Disables the extraction-marker-based pre-download skip. When active, every archive that does not already exist on disk as a file is downloaded regardless of whether a matching `ArchiveName.txt` marker exists.

The local archive file check (layer one) still applies. If the `.lha` already exists on disk, the download is skipped.

**Use case:** you have deleted some extracted game folders and want to re-download and re-extract only the archives that are missing.

```
whdfetch DOWNLOADGAMES NODOWNLOADSKIP
```

---

### FORCEDOWNLOAD

Forces every archive to be re-downloaded, bypassing both the local-file-exists check and the extraction marker check. Even if the `.lha` is already present on disk, it is re-fetched from the Retroplay server.

This is more aggressive than `NODOWNLOADSKIP`. It bypasses both skip layers unconditionally.

**Warning:** this re-downloads every archive in the selected packs. On a large pack this may involve thousands of files and many gigabytes. Use with care on slow or metered connections.

```
whdfetch DOWNLOADGAMES FORCEDOWNLOAD
```

Full clean re-download and re-extract:

```
whdfetch DOWNLOADGAMES FORCEDOWNLOAD FORCEEXTRACT
```

---

## Filter Options

Filter options exclude certain categories of archive from being processed. Filtering is based on metadata encoded in the archive filename: chipset type (`_AGA`), video format (`_NTSC`), media type (`_CD32`, `_CDTV`), and language codes (`_De`, `_Fr`, `_It`, and others).

Multiple filters can be combined. An archive is processed only if it passes all active filters. Filters apply equally to all selected packs.

---

### SKIPAGA

Excludes archives whose filename indicates they require the AGA chipset (filenames containing `_AGA`). AGA hardware requires an Amiga 1200 or Amiga 4000. Use this if you are running on an OCS or ECS machine such as an A500, A600, or A2000.

```
whdfetch DOWNLOADGAMES SKIPAGA
```

---

### SKIPCD

Excludes archives for CD-based formats, including CD32, CDTV, and CDRom titles. These require a CD drive or CD32 hardware that may not be available in a standard WHDLoad setup.

```
whdfetch DOWNLOADGAMES SKIPCD
```

---

### SKIPNTSC

Excludes archives flagged as NTSC-only. NTSC games were designed for the 60 Hz video standard used in North America and Japan. On a PAL Amiga (50 Hz), some NTSC titles may have timing or display issues.

```
whdfetch DOWNLOADGAMES SKIPNTSC
```

---

### SKIPNONENGLISH

Excludes archives identified as non-English language versions. The parser recognises language codes embedded in the archive filename: `De`, `Fr`, `It`, `Es`, `NL`, `Fi`, `Dk`, `Pl`, `Gr`, `Cz`, and several others.

Archives with no language code are treated as English and are always included.

```
whdfetch DOWNLOADGAMES SKIPNONENGLISH
```

---

### Combining Filters

Filters are cumulative. Use them together for a tightly filtered run:

```
whdfetch DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

This downloads only English-language, PAL, OCS/ECS games with no CD requirement.

---

## Output and Reporting

### NOSKIPREPORT

Suppresses the per-archive messages normally printed when an archive is skipped. On large update runs where most archives are already present, this keeps console output focused on what is actually happening.

Skipped archives are still counted in the final summary statistics and recorded in log files.

```
whdfetch DOWNLOADALL NOSKIPREPORT
```

---

### VERBOSE

Shows detailed output from the DAT ZIP extraction step. Without this option, ZIP extraction runs quietly. Archive downloads always show progress information regardless of this setting.

```
whdfetch DOWNLOADGAMES VERBOSE
```

---

### DISABLECOUNTERS

Disables the pre-count pass and suppresses `Download X of Y` style status lines. This also disables the `MB left` running total during large runs.

Use this on slower systems if you want the leanest possible output path, or when running in scripted configurations that do not need progress feedback.

```
whdfetch DOWNLOADALL DISABLECOUNTERS
```

---

### CRCCHECK

Enables CRC verification for downloaded archives using CRC values from the DAT metadata. When a mismatch is detected, the download is retried within the configured retry budget.

CRC checking is off by default for fastest operation. Enable it when you suspect a connection is introducing corruption, or after a run with many unexplained extraction failures.

```
whdfetch DOWNLOADGAMES CRCCHECK
```

---

## Maintenance Commands

These commands perform destructive operations on local data. Both require explicit confirmation before proceeding.

---

### PURGETEMP

Permanently deletes the entire `PROGDIR:temp` directory, including all downloaded index files, ZIP packs, and extracted DAT files. The program prints a brief warning and requires you to type `Y` before deletion proceeds.

After purging, the next full run will re-download index and DAT files from scratch.

```
whdfetch PURGETEMP
```

---

### PURGEARCHIVES

Permanently deletes all downloaded `.lha` and `.lzx` archive files under `GameFiles/` recursively. Extracted game folders and their contents are not touched. The program prints a brief warning and requires explicit `Y` confirmation.

Use this to reclaim disk space while keeping extacted game installations intact. On the next run, any archive that is missing and not already covered by an `ArchiveName.txt` marker will be re-downloaded.

```
whdfetch PURGEARCHIVES
```

---

## Connection Settings

### TIMEOUT=\<seconds\>

Sets the maximum number of seconds whdfetch waits for any HTTP activity before aborting the current transfer. The timeout applies to every HTTP operation during a run: the index fetch, ZIP pack downloads, and individual `.lha` archive downloads. A timeout is triggered whenever a socket stays idle for longer than the configured value.

Valid values are 5 through 60. The default is 30 seconds. Values outside this range are clamped rather than rejected.

CLI arguments override INI settings, so `TIMEOUT=45` overrides any `timeout_seconds` value in the INI for that run.

```
whdfetch DOWNLOADALL TIMEOUT=45
```

The INI equivalent is `timeout_seconds=45` in the `[global]` section.

---

## Icon Handling

After each directory in the extraction hierarchy is created, whdfetch writes a drawer icon so every folder appears correctly in Workbench. Icons are looked up from `PROGDIR:Icons/` before falling back to the system default.

### Custom icon lookup

If `PROGDIR:Icons/` exists, whdfetch matches each folder's name against a `.info` file inside it. For example:

- `PROGDIR:Icons/A.info` — used for the letter-A folder in any pack
- `PROGDIR:Icons/Games.info` — used for the Games pack folder
- `PROGDIR:Icons/Games Beta.info` — used for the Games Beta pack folder

If no matching custom icon is found, `GetDefDiskObject(WBDRAWER)` is used as a fallback. If the `Icons` folder does not exist, custom lookup is silently skipped — the feature is effectively enabled just by placing the folder there.

An existing icon is never overwritten.

### WHDLoad game folder icons

WHDLoad archives typically include their own `.info` sidecar next to the game folder. After successful extraction, whdfetch can replace this icon with a consistent template from `PROGDIR:Icons/WHD folder.info`. The replacement handles write-protected icons gracefully.

Replaced icons are automatically unsnapshotted so saved Workbench X/Y positions are cleared. This prevents icons from stacking at the same screen coordinates when viewed in Workbench.

### NOICONS

Disables all custom icon handling. The extraction pipeline will not apply icons from `PROGDIR:Icons/`. System default drawer icons may still be created by AmigaOS if a folder has no icon at all, but nothing from `PROGDIR:Icons/` is used. Icon unsnapshotting is also skipped.

Use this when running in a headless configuration, when icons are irrelevant, or when you have your own icon set and do not want whdfetch overwriting it.

```
whdfetch DOWNLOADGAMES NOICONS
```

---

## INI File Configuration

`PROGDIR:whdfetch.ini` is optional. When present it overrides compiled defaults. The legacy filename `PROGDIR:WHDDownloader.ini` is also recognised as a fallback if the newer name is absent.

**Precedence:** compiled defaults → INI file → CLI arguments

CLI arguments always win. For example, if the INI has `delete_archives_after_extract=false` but you pass `DELETEARCHIVES` on the command line, archives will be deleted.

A fully annotated sample file is provided at `docs/whdfetch.ini.sample`.

---

### \[global\] section

```ini
[global]
download_website=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/
extract_archives=true
skip_existing_extractions=true
skip_download_if_extracted=true
force_download=false
verbose_output=false
timeout_seconds=30
extract_path=
delete_archives_after_extract=true
use_custom_icons=true
unsnapshot_icons=true
disable_counters=false
crccheck=false
```

The `download_website` key sets the base URL used to fetch the index and pack downloads. If the Retroplay site is restructured, you can update this value without recompiling. The URL must end with a trailing slash.

`extract_path` is empty by default, meaning extraction happens in place alongside downloaded archives. Set it to a path such as `Games:` to redirect extraction to a separate volume.

---

### \[filters\] section

```ini
[filters]
skip_aga=false
skip_cd=false
skip_ntsc=false
skip_non_english=false
```

Boolean values accept `true`/`false`, `yes`/`no`, or `1`/`0`.

---

### \[pack.\<type\>\] sections

Each pack has its own section. Types are: `games`, `games_beta`, `demos`, `demos_beta`, `magazines`.

```ini
[pack.games]
enabled=true
display_name=Games
download_url=http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games/
local_dir=Games/
filter_dat=Games(
filter_zip=Games(
```

`enabled=true` selects the pack when whdfetch is run without CLI pack-selection arguments. `display_name` is the human-readable label used in console output and inside `ArchiveName.txt` marker files. `local_dir` is the subdirectory under `GameFiles/` where archives and extracted folders are stored.

---

### Pack-selection precedence rule

Pack selection follows this order at runtime:

1. Start with no packs selected
2. Apply INI `[pack.*].enabled` values
3. If any CLI pack-selection command is present (`DOWNLOADGAMES`, `DOWNLOADALL`, etc.), all INI pack selections are cleared and only the CLI selections apply

In short: if you name a pack on the command line, INI pack settings are set aside entirely for that run.

---

### CLI to INI key mapping

| CLI Argument        | INI Key                               | INI Section         |
| ------------------- | ------------------------------------- | ------------------- |
| `DOWNLOADGAMES`     | `enabled=true`                        | `[pack.games]`      |
| `DOWNLOADBETAGAMES` | `enabled=true`                        | `[pack.games_beta]` |
| `DOWNLOADDEMOS`     | `enabled=true`                        | `[pack.demos]`      |
| `DOWNLOADBETADEMOS` | `enabled=true`                        | `[pack.demos_beta]` |
| `DOWNLOADMAGS`      | `enabled=true`                        | `[pack.magazines]`  |
| `DOWNLOADALL`       | all five `enabled=true`               | `[pack.*]`          |
| `NOEXTRACT`         | `extract_archives=false`              | `[global]`          |
| `EXTRACTTO=<path>`  | `extract_path=<path>`                 | `[global]`          |
| `KEEPARCHIVES`      | `delete_archives_after_extract=false` | `[global]`          |
| `DELETEARCHIVES`    | `delete_archives_after_extract=true`  | `[global]`          |
| `NODOWNLOADSKIP`    | `skip_download_if_extracted=false`    | `[global]`          |
| `FORCEDOWNLOAD`     | `force_download=true`                 | `[global]`          |
| `NOICONS`           | `use_custom_icons=false`              | `[global]`          |
| `DISABLECOUNTERS`   | `disable_counters=true`               | `[global]`          |
| `CRCCHECK`          | `crccheck=true`                       | `[global]`          |
| `TIMEOUT=<seconds>` | `timeout_seconds=<seconds>`           | `[global]`          |
| `SKIPAGA`           | `skip_aga=true`                       | `[filters]`         |
| `SKIPCD`            | `skip_cd=true`                        | `[filters]`         |
| `SKIPNTSC`          | `skip_ntsc=true`                      | `[filters]`         |
| `SKIPNONENGLISH`    | `skip_non_english=true`               | `[filters]`         |

`FORCEEXTRACT` and `NOSKIPREPORT` have no INI equivalents and are per-run only.

---

## Session Reports

At the end of each run, when any reportable activity occurred, whdfetch writes a plain-text session report to `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`.

Reports are grouped by pack and list each archive under one of four headings:

- **New** — archives downloaded for the first time with no matching version already installed
- **Updated** — archives downloaded that replace an older version of the same title
- **Local cache reuse (no download)** — archives handled from existing files in `GameFiles/` without any network fetch
- **Extraction skipped** — archives where extraction was intentionally skipped, for example when `c:unlzx` is not installed for a `.lzx` archive

When no reportable activity occurred, the program prints `No new archives this session.` and no file is written.

### How UPDATE detection works

Update classification uses the `.archive_index` cache at download time, before the cache entry is updated for the new archive. An archive is classified as Updated only when:

- It shares the same strict identity as a cached entry: matching title, group, chipset, video format, language, media type, and disk count
- The new archive's version number is strictly greater than the cached version

If the new archive has no numeric version token, it is treated as New. If a cached candidate has no version token, it is ignored for update matching. This prevents false positives where many different releases share the same primary title prefix.

---

## Skip Detection and Caching

### Archive index (`.archive_index`)

Each letter folder under `GameFiles/<pack>/` contains a hidden file called `.archive_index`. This file is a TAB-separated text file with one entry per line:

```
Academy_v1.2.lha	Academy
AcademyAGA_v1.0.lha	AcademyAGA
```

Column one is the exact archive filename. Column two is the name of the extracted game folder on disk.

The index is loaded into memory at startup, queried during download skip checks, updated after each successful extraction, and flushed to disk during clean shutdown. Stale entries that point to folders which no longer exist are pruned automatically on each flush.

The index exists because the extracted folder name does not always match the archive filename in a predictable way. Without it, confirming that a title is already installed would require scanning every subfolder and reading its `ArchiveName.txt` — an expensive operation on classic Amiga storage.

### Marker file (`ArchiveName.txt`)

Inside each extracted game folder, whdfetch writes a two-line text file:

```
Games
Academy_v1.2.lha
```

Line one is the pack display name (for example `Games` or `Demos (Beta & Unofficial)`). Line two is the exact archive filename.

If line two of this file matches the archive filename being processed, extraction is skipped unless `FORCEEXTRACT` is active. The download itself is also skipped unless `NODOWNLOADSKIP` or `FORCEDOWNLOAD` is used.

### Incomplete download recovery (`.downloading`)

Before each download begins, an empty marker file is created alongside the archive:

```
GameFiles/Games/A/Academy_v1.2.lha.downloading
```

On successful completion, the marker is deleted. If the program is interrupted mid-download, both the marker and the partial archive remain on disk.

On the next run, whdfetch detects the marker, deletes both the marker and the partial archive, and starts the download cleanly. Non-retryable server errors (400, 401, 403, 404) also clean up the marker immediately so failed entries do not generate futile retries on every subsequent run.

---

## Tips & Troubleshooting

**Nothing downloads — the program shows the help screen.**  
At least one pack must be selected, either by a CLI pack command or via `enabled=true` in an `[pack.*]` INI section. Running whdfetch with no pack selected and no other arguments is treated as a request for help.

**The run downloads nothing new even though new titles should be available.**  
Check whether `skip_download_if_extracted` is true in your INI. If existing game folders have `ArchiveName.txt` markers that match archive names, whdfetch skips them by design. Use `NODOWNLOADSKIP` to bypass the marker check while keeping the local-file check active, or `FORCEDOWNLOAD` to re-download everything.

**A previous run was interrupted and archives are not being retried.**  
Look for `.downloading` marker files alongside the partial archives in `GameFiles/<pack>/<letter>/`. If they are present, the next run will detect them and retry. If they are absent but partial archives remain, delete the partial archives manually and run again.

**Extraction completes but game folders have the wrong icon.**  
Check that `PROGDIR:Icons/WHD folder.info` exists. The icon must be named exactly `WHD folder.info`. If `NOICONS` was passed or `use_custom_icons=false` is set in the INI, custom icons are disabled for that run.

**`.lzx` archives are being skipped.**  
`c:unlzx` is not installed or not in `C:`. Install it from Aminet. whdfetch does not fail the run when `unlzx` is absent — it skips `.lzx` archives with a warning and processes everything else normally.

**Downloads time out repeatedly.**  
Try increasing the timeout. The default is 30 seconds. Use `TIMEOUT=60` or set `timeout_seconds=60` in the INI for slow or high-latency connections. Valid values are 5–60 seconds.

**The collection is already installed. How do I avoid re-downloading everything?**  
whdfetch handles this automatically if the `.archive_index` files and `ArchiveName.txt` markers are in place from previous runs. If they are not — for example, you are adopting a collection extracted by another tool — you can run `EXTRACTONLY` to process what is already on disk and generate the markers.

**I want to try different filter combinations without affecting my main collection.**  
Use `EXTRACTTO=` to point extraction at a separate volume or partition. Everything under `GameFiles/` remains untouched.

**Disk space is running low and I want to remove the downloaded archives.**  
Run `whdfetch PURGEARCHIVES`. This deletes `.lha` and `.lzx` files under `GameFiles/` recursively while leaving extracted game folders in place.

**I want to start fresh from a clean DAT download.**  
Run `whdfetch PURGETEMP`. This removes everything in `PROGDIR:temp` including downloaded index and ZIP files. The next run will fetch fresh copies.

**The program crashes or behaves oddly at startup.**  
Check that `bsdsocket.library` is available (Roadshow must be running). Check `PROGDIR:logs/` for error log files. The `errors` log category collects all errors regardless of which subsystem produced them.

---

### Command Summary

| Command             | What it does                                                       |
| ------------------- | ------------------------------------------------------------------ |
| `DOWNLOADGAMES`     | Select the Games pack                                              |
| `DOWNLOADBETAGAMES` | Select the Games Beta pack                                         |
| `DOWNLOADDEMOS`     | Select the Demos pack                                              |
| `DOWNLOADBETADEMOS` | Select the Demos Beta pack                                         |
| `DOWNLOADMAGS`      | Select the Magazines pack                                          |
| `DOWNLOADALL`       | Select all five packs                                              |
| `HELP`              | Show help and exit                                                 |
| `EXTRACTONLY`       | Extract already-downloaded archives; do not fetch anything new     |
| `NOEXTRACT`         | Download archives but do not extract them                          |
| `EXTRACTTO=<path>`  | Extract to an alternate path                                       |
| `KEEPARCHIVES`      | Keep archive files after extraction                                |
| `DELETEARCHIVES`    | Delete archive files after extraction (compiled default)           |
| `FORCEEXTRACT`      | Re-extract even when `ArchiveName.txt` already matches             |
| `NODOWNLOADSKIP`    | Download even when an extraction marker exists (level 2 bypass)    |
| `FORCEDOWNLOAD`     | Re-download even when the file already exists (both levels bypass) |
| `SKIPAGA`           | Skip AGA-only archives                                             |
| `SKIPCD`            | Skip CD32 / CDTV / CDRom archives                                  |
| `SKIPNTSC`          | Skip NTSC-only archives                                            |
| `SKIPNONENGLISH`    | Skip non-English archives                                          |
| `NOICONS`           | Disable custom icon replacement                                    |
| `NOSKIPREPORT`      | Suppress on-screen skip messages                                   |
| `VERBOSE`           | Show detailed DAT extraction output                                |
| `DISABLECOUNTERS`   | Disable download counter and progress pre-count                    |
| `CRCCHECK`          | Enable CRC verification for downloaded archives                    |
| `TIMEOUT=<seconds>` | Set HTTP timeout (5–60 seconds, default 30)                        |
| `PURGETEMP`         | Delete `PROGDIR:temp` (prompts for confirmation)                   |
| `PURGEARCHIVES`     | Delete downloaded archives under `GameFiles/` (prompts first)      |
