# whdfetch

**A Retroplay WHDLoad Archive Downloader and Installer for AmigaOS 3.0+**

This program helps you set up an internet-connected Amiga with the latest WHDLoad games, demos, and magazines from Retroplay’s collection on the Turran server. You can select packs, apply basic filters, and run it again later to update your collection by automatically finding new or updated releases.

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
10. [Disk Space Estimation](#disk-space-estimation)
11. [Maintenance Commands](#maintenance-commands)
12. [Connection Settings](#connection-settings)
13. [Icon Handling](#icon-handling)
14. [INI File Configuration](#ini-file-configuration)
15. [Session Reports](#session-reports)
16. [Skip Detection and Caching](#skip-detection-and-caching)
17. [Tips & Troubleshooting](#tips--troubleshooting)

---

## Requirements

- AmigaOS 3.0 or newer (3.1 or later recommended)
- TCP/IP stack (`bsdsocket.library v4` or higher) — required for all network operations
- `c:unzip` — required to extract the DAT files. Available on Aminet: util/arc/UnZip.lha
- `c:lha` — required for `.lha` archive extraction. Available on Aminet: util/arc/lha
- `c:unlzx` — optional; required for `.lzx` archive extraction. If not installed, `.lzx` archives are skipped with a warning and the run continues. Aminet: TBC
- Fast RAM recommended — Should work with at least 2 MB of memory, but more is better.
- Enough hard drive space to store all the files. A more modern file system, such as PFS, is recommended, as the extracted archives can easily exceed the 4GB limit.

---

## Introduction

After rebuilding my Amiga, I found downloading and extracting all the latest WHDLoad packs took a lot of time. This program simplifies that by downloading the newest packs from RetroPlay and the archives from the Turran server. You can choose one or more packs, such as Games or Demos, and apply simple filters by chipset, language, CD, and more. It extracts archives directly to a folder you pick, creating a clear file path (for example, “Games/A/A10TankKiller3Disk”) with custom or default icons. You can also choose to keep or delete the downloaded archive after extraction. Running the program again on the same folder will check for new or updated items and download only what’s needed.

If you like, the program can verify the downloaded archive’s CRC against the server’s to ensure the download was successful. Extracting archives directly on the Amiga, real or emulated, is the only way I know to preserve special file metadata and attributes correctly. That’s why I created this tool.

Right now, this program works only through the command line. You can use an optional .ini file to set default options and update filters or URLs if they change later.

---

## Getting Started

`whdfetch` lets you download and extract WHDLoad packs right on your Amiga. Before you start a big download, it’s a good idea to check how much disk space you’ll need. This is especially important on real Amiga systems, where storage needs can be bigger than you might think.

### 1. Copy the program files

Copy `whdfetch` to a folder on your Amiga. If you want to use a custom setup, copy `whdfetch.ini` to the same folder. If you don’t include `whdfetch.ini`, the program will use its default settings.

### 2. Ensure internet access is working

Your Amiga must already be connected to the internet before running whdfetch.

I use Roadshow for my TCP/IP stack, but `whdfetch` has also been tested well with the emulated `bsdsocket.library` under WinUAE. Other TCP/IP stacks should work too, as long as your Amiga can access the internet.

### 3. Open a Shell

Open a Shell and change to the directory where you placed `whdfetch`.

### 4. Check the required disk space first

Before you download anything, run this command:

```text
whdfetch ESTIMATESPACE
```

This command downloads the latest DAT files, checks the archive sizes, and shows you an estimate of the disk space each pack will need.

It shows:

* the total archive size
* a rough estimate of extracted size
* the combined space needed if you keep both archives and extracted files

This is the safest first command for new users, as it lets you see the storage requirements before committing to a long download.

This is the safest first step for new users because it lets you check storage needs before starting a long download.

Note that downloaded archives are stored in a GameFiles directory created in the same location from which `whdfetch` is run. Extracted files can be redirected elsewhere with `EXTRACTTO`, but the downloaded archives still need space on the drive where `whdfetch` is being run. For this reason, checking free space before starting is very important.

Usually, the full collection needs many gigabytes of free space, especially if you choose to keep the archives with `KEEPARCHIVES`.

### 5. Decide where extracted files should go

Before you start a real download, decide where you want the extracted files to go.

Use `EXTRACTTO` to pick the destination folder. `whdfetch` will automatically create the right folder structure inside it.

For example:

```text
whdfetch DOWNLOADBETADEMOS EXTRACTTO Work:WHDLoad/
```

Here, the extracted files go under Work:WHDLoad/ instead of the program folder.

whdfetch creates the needed subfolders automatically. This keeps your downloaded pack organised without you having to set up the folder tree yourself.

### 6. Apply filters if needed

`ESTIMATESPACE` also works with the usual filters, so you can get a space estimate that fits your setup better.

For example:

```text
whdfetch ESTIMATESPACE SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

This gives a more realistic estimate if you’re using a non-AGA, PAL, English-language setup.

### 7. Try a small real download

Once you’re sure you have enough disk space, try a small test download to your chosen extraction folder:

```text
whdfetch DOWNLOADBETADEMOS EXTRACTTO Work:WHDLoad/
```

This is a good first test since it’s a very small pack. It helps you check that downloading, extracting, folder creation, and reporting all work on your system without needing a long overnight run.

### 8. Check the results

When the run finishes, `whdfetch` shows a summary with totals for:

* new downloads
* skipped titles
* extraction results

It also saves a session report to:

```text
PROGDIR:updates/
```

### 9. Move on to the main packs

Once everything works as expected, you can try one of the bigger packs like:

```text
whdfetch DOWNLOADGAMES EXTRACTTO Work:WHDLoad/
```

`DOWNLOADGAMES` is the biggest pack and can take many hours on a real Amiga. It’s usually best to let it run overnight once you’re sure everything is working right.

### Tips

**Always estimate first.**
Run `ESTIMATESPACE` before any big download, especially if your hard disk space is limited.

**Keep in mind where archives are saved.**
Downloaded archives go into the GameFiles folder created where you run `whdfetch`. Even if you use `EXTRACTTO`, the drive with `whdfetch` still needs enough free space for the archives.

**Pick your extraction destination from the start.**
Using `EXTRACTTO` right away helps keep extracted files on the right drive and shows you where new folders will be created.

**Use filters.**
If your machine can’t run some titles (like AGA), applying filters first can save download time and disk space.

**By default, archives are deleted after extraction.**
If you want to keep the downloaded archives as a backup, use this:

```text
whdfetch DOWNLOADGAMES KEEPARCHIVES EXTRACTTO Work:WHDLoad/
```

Kept archives also act as a local cache. If a later command needs the same archive again, `whdfetch` can reuse the copy already stored in `GameFiles` instead of downloading it again, provided it is still present and up to date.

**You don’t need to keep archives for future updates.**
If you run the same pack again on the same extraction folder, `whdfetc` checks tracking files inside the folders and skips titles that are already up to date. This lets you refresh a Retroplay WHDLoad collection over time without redownloading everything.

This works even if you deleted archives after extraction, because metadata like .archive_index is saved inside the extraction folders.

Be aware that keeping archives requires significantly more disk space, as both the archives and the extracted contents must be stored.
---

## How It Works

A whdfetch run follows a set sequence of steps for each selected pack:

1. **Fetch the index.** The program downloads `index.html` from the Retroplay FTP site and scans it for links that match the expected pack filename pattern.
2. **Download the pack ZIP.** Each matching pack link is a ZIP file containing a DAT (XML) file listing every archive in that pack, including filenames, sizes, and CRC values. The ZIP is downloaded to `temp/Zip files/` and extracted to `temp/Dat files/`.
3. **Parse the DAT file.** Every `<rom name="...">` entry in the XML is parsed. The filename is decoded to extract metadata: title, version, chipset requirement, video format, language codes, media type, and number of disks. Active filters are applied at this stage.
4. **Process each entry.** For every archive that passes the filters, whdfetch checks if it's already present and up to date using the `.archive_index` cache. If enabled, it can also verify `ArchiveName.txt` markers before skipping a download. If the archive needs downloading, it fetches it from the Retroplay FTP server directly to `GameFiles/<pack>/<letter>/`.
5. **Extract.** If extraction is enabled (the default), the archive is processed with `c:lha` or `c:unlzx`. An `ArchiveName.txt` marker is written inside the extracted folder. Optional icon replacement happens afterward.
6. **Report.** At the end of the run, per-pack statistics are printed to the console, and a full session report is saved to `PROGDIR:updates/updates_YYYY-MM-DD_HH-MM-SS.txt`.

The `.archive_index` cache is updated in memory during the run and saved to disk on clean exit. Stale entries pointing to folders that no longer exist are automatically removed each time it's saved.

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

Disables the extraction pipeline entirely. Archives are downloaded and left as `.lha` or `.lzx` files. No extractor is called, no `ArchiveName.txt` is written, and no icons are applied.

This is useful for downloading a large batch now and extracting later, or when you want to use an external extraction tool. Because extraction is skipped, startup validation of extraction tools is also skipped, so the program runs even if `c:lha` is not installed.

Pre-download skip logic still works normally. By default, this uses `.archive_index` and folder existence checks. If `VERIFYMARKER` is enabled, `ArchiveName.txt` is also verified before pre-download skip.

```
whdfetch DOWNLOADGAMES NOEXTRACT
```

---

### EXTRACTTO=\<path\>

Redirects the extraction output to a different location instead of extracting alongside the downloaded archives. The pack and letter directory structure is preserved under the given path:

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

Keeps archive files after successful extraction. By default, archives are deleted after extraction. This overrides that default and any INI setting.

Useful if you want to maintain a local archive cache alongside the extracted games for backup or to re-extract later with different options.

`KEEPARCHIVES` and `DELETEARCHIVES` oppose each other. Only one should be used per run.

```
whdfetch DOWNLOADGAMES KEEPARCHIVES
```

---

### DELETEARCHIVES

Explicitly enables post-extraction archive deletion. This is the compiled default, so this command is mainly useful for overriding an INI setting that controls whether archives are kept.

If extraction fails for a particular archive, that archive is not deleted. Only successfully extracted archives are removed.

`DELETEARCHIVES` has no effect when `NOEXTRACT` is active, since deletion occurs only after a successful extraction pass.

```
whdfetch DOWNLOADGAMES DELETEARCHIVES
```

---

### FORCEEXTRACT

Bypasses the `ArchiveName.txt` skip check and re-extracts every archive, even when the marker already matches.

Normally, if a game folder contains an `ArchiveName.txt` whose second line matches the current archive filename, extraction is skipped. `FORCEEXTRACT` disables this check for the current run.

This does not affect the pre-download skip. If the archive has not been downloaded and an extracted match is found, the download itself may still be skipped unless `FORCEDOWNLOAD` or `NODOWNLOADSKIP` is also used.

Use `FORCEEXTRACT` to reapply updated icons to all game folders, repair a suspected incomplete extraction, or re-extract to a new `EXTRACTTO` path.

```
whdfetch DOWNLOADGAMES FORCEEXTRACT
```

Full rebuild — re-download and re-extract everything:

```
whdfetch DOWNLOADALL FORCEEXTRACT FORCEDOWNLOAD
```

---

## Download Skip Options

whdfetch has two primary layers of skip logic, plus an optional verification layer:

1. **Local archive skip** — if the `.lha` or `.lzx` file already exists on disk, the download is skipped, and the existing archive is passed to the extraction pipeline.
2. **Extracted-folder skip** — if no local archive exists, but `.archive_index` contains the archive and the indexed extracted folder still exists, download is skipped.
3. **Optional marker verification** — with `VERIFYMARKER`, whdfetch also verifies `ArchiveName.txt` metadata before pre-download skip.

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

### VERIFYMARKER

Enables an extra pre-download `ArchiveName.txt` verification pass.

By default, pre-download skip uses `.archive_index` plus folder-exists checks for speed. On slower real Amiga hardware, this is significantly faster for very large lists. With `VERIFYMARKER`, whdfetch also reads marker metadata before deciding to skip.

Use this when you want stricter marker validation and are willing to trade speed for it.

```
whdfetch DOWNLOADGAMES VERIFYMARKER
```

---

### FORCEDOWNLOAD

Forces every archive to be redownloaded, bypassing both the local-file-exists check and the extraction-marker check. Even if the `.lha` is already present on disk, it is re-fetched from the Retroplay server.

This is more aggressive than `NODOWNLOADSKIP`. It bypasses both skip layers unconditionally.

**Warning:** this re-downloads every archive in the selected packs. On a large pack, this may involve thousands of files and many gigabytes. Use with care on slow or metered connections.

```
whdfetch DOWNLOADGAMES FORCEDOWNLOAD
```

Full clean re-download and re-extract:

```
whdfetch DOWNLOADGAMES FORCEDOWNLOAD FORCEEXTRACT
```

---

## Filter Options

Filter options exclude certain archive categories from processing. Filtering is based on metadata encoded in the archive filename: chipset type (`_AGA`), video format (`_NTSC`), media type (`_CD32`, `_CDTV`), and language codes (`_De`, `_Fr`, `_It`, and others).

Multiple filters can be combined. An archive is processed only if it passes all active filters. Filters apply equally to all selected packs.

---

### SKIPAGA

Excludes archives whose filename indicates they require the AGA chipset (filenames containing `_AGA`). AGA hardware requires an Amiga 1200 or Amiga 4000. Use this if you are running on an OCS or ECS machine such as an A500, A600, or A2000.


---

### SKIPCD

Excludes archives for CD-based formats, including CD32, CDTV, and CDRom titles. These require a CD drive or CD32 hardware that may not be available in a standard WHDLoad setup.


---

### SKIPNTSC

Excludes archives flagged as NTSC-only. NTSC games were designed for the 60 Hz video standard used in North America and Japan. On a PAL Amiga (50 Hz), some NTSC titles may have timing or display issues.


---

### SKIPNONENGLISH

Excludes archives identified as non-English language versions. The parser recognises language codes embedded in the archive filename: `De`, `Fr`, `It`, `Es`, `NL`, `Fi`, `Dk`, `Pl`, `Gr`, `Cz`, and several others.

Archives with no language code are treated as English and are always included.

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

Skipped archives are still counted in the final summary statistics. If `ENABLELOGGING` is active, they are also written to log files.

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

### ENABLELOGGING

Enables file-based logging for the current run. By default, logging is disabled to reduce file I/O overhead on slower original Amiga hardware.

When enabled, whdfetch writes category logs under `PROGDIR:logs/` (for example `general_*.log`, `download_*.log`, and `errors_*.log`).

```
whdfetch DOWNLOADGAMES ENABLELOGGING
```

---

### DISABLECOUNTERS

Disables the pre-count pass and suppresses `Download X of Y` style status lines. This also disables the `MB left` running total during large runs.

Use this on slower systems for the leanest possible output path, or when running in scripted configurations that do not require progress feedback.

```
whdfetch DOWNLOADALL DISABLECOUNTERS
```

---

### CRCCHECK

Enables CRC verification for downloaded archives using CRC values from the DAT metadata. When a mismatch is detected, the download is retried within the configured retry budget.

CRC checking is off by default for the fastest operation. Enable it when you suspect a connection is introducing corruption, or after a run with many unexplained extraction failures.


---

## Disk Space Estimation

### ESTIMATESPACE

Calculates how much disk space the selected packs will require, then prints a per-pack and combined total without downloading any ROM archives. This is a read-only planning command: it downloads and parses the DAT files (or uses cached ones if they are up to date), then totals up the archive sizes and exits. No game archives are downloaded, and no extraction takes place.

Because the DAT metadata contains the compressed archive size for each entry, the figures shown are the space needed for the `.lha` files themselves. The extracted size cannot be known precisely in advance, so whdfetch estimates it at **1.5× the archive size** as a rough guide.

For each selected pack, the output shows:

- **Archive size** — total size of all archives in the pack that pass your current filters.
- **Extracted (estimate)** — rough estimate of the space needed once all archives are extracted (1.5× archive size).

At the bottom, a combined total is shown for both figures. If you intend to use `KEEPARCHIVES`, a third figure is shown: the total if you keep the archives on disk alongside the extracted games.

All active filter flags (`SKIPAGA`, `SKIPCD`, `SKIPNTSC`, `SKIPNONENGLISH`) apply normally. Only archives that would pass those filters are counted in the totals.

If no pack-selection command is given alongside `ESTIMATESPACE`, all five packs are estimated automatically (equivalent to `DOWNLOADALL`).

**Basic usage — estimate all packs:**

```
whdfetch ESTIMATESPACE
```

**Estimate only the Games pack, excluding AGA titles:**

```
whdfetch DOWNLOADGAMES ESTIMATESPACE SKIPAGA
```

**Estimate all packs with full OCS/ECS filtering:**

```
whdfetch ESTIMATESPACE SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

> **Note:** The 1.5× extracted size is a rough guide only. Actual extracted sizes vary by title. The true extracted size is typically in this range, but individual packs may differ.

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

Permanently deletes all downloaded `.lha` and `.lzx` archive files under `GameFiles/` recursively. Extracted game folders and their contents are not touched. The program prints a brief warning and requires explicit confirmation with `Y`.

Use this to reclaim disk space while keeping extracted game installations intact. On the next run, any archive that is missing and not already covered by an `ArchiveName.txt` marker will be re-downloaded.

```
whdfetch PURGEARCHIVES
```

---

## Connection Settings

### TIMEOUT=\<seconds\>

Sets the maximum number of seconds whdfetch waits for any HTTP activity before aborting the current transfer. The timeout applies to every HTTP operation during a run: the index fetch, ZIP pack downloads, and individual `.lha` archive downloads. A timeout is triggered when a socket remains idle for longer than the configured timeout.

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

If no matching custom icon is found, `GetDefDiskObject(WBDRAWER)` is used as a fallback. If the `Icons` folder does not exist, the custom lookup is silently skipped — the feature is enabled simply by placing the folder there.

An existing icon is never overwritten.

Note: Check that the icon is set to type "Drawer"; otherwise, you may not be able to open the new drawer.

### WHDLoad game folder icons

WHDLoad archives typically include their own `.info` sidecar next to the game folder. After successful extraction, whdfetch can replace this icon with a consistent template from `PROGDIR:Icons/WHD folder.info`. The replacement handles write-protected icons gracefully.

Replaced icons are automatically unsnapshotted, so saved Workbench X/Y positions are cleared. This prevents icons from stacking at the same screen coordinates when viewed in Workbench.

### NOICONS

Disables all custom icon handling. The extraction pipeline will not apply icons from `PROGDIR:Icons/`. System default drawer icons may still be created by AmigaOS if a folder has no icon at all, but nothing from `PROGDIR:Icons/` is used. Icon unsnapshotting is also skipped.

Use this when running in a headless configuration, when icons are irrelevant.

```
whdfetch DOWNLOADGAMES NOICONS
```

---

## INI File Configuration

`PROGDIR:whdfetch.ini` is optional. When present, it overrides compiled defaults.

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
verify_archive_marker_before_download=false
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

`extract_path` is empty by default, meaning extraction happens in place alongside downloaded archives. Set it to a path, such as `Games:` to redirect extraction to a separate volume.

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

1. Start with no packs selected.
2. Apply INI `[pack.*].enabled` values
3. If any CLI pack-selection command is present (`DOWNLOADGAMES`, `DOWNLOADALL`, etc.), all INI pack selections are cleared, and only the CLI selections apply.

In short: if you name a pack on the command line, INI pack settings are set aside entirely for that run.

---

### CLI to INI key mapping

| CLI Argument        | INI Key                                      | INI Section         |
| ------------------- | -------------------------------------------- | ------------------- |
| `DOWNLOADGAMES`     | `enabled=true`                               | `[pack.games]`      |
| `DOWNLOADBETAGAMES` | `enabled=true`                               | `[pack.games_beta]` |
| `DOWNLOADDEMOS`     | `enabled=true`                               | `[pack.demos]`      |
| `DOWNLOADBETADEMOS` | `enabled=true`                               | `[pack.demos_beta]` |
| `DOWNLOADMAGS`      | `enabled=true`                               | `[pack.magazines]`  |
| `DOWNLOADALL`       | all five `enabled=true`                      | `[pack.*]`          |
| `NOEXTRACT`         | `extract_archives=false`                     | `[global]`          |
| `EXTRACTTO=<path>`  | `extract_path=<path>`                        | `[global]`          |
| `KEEPARCHIVES`      | `delete_archives_after_extract=false`        | `[global]`          |
| `DELETEARCHIVES`    | `delete_archives_after_extract=true`         | `[global]`          |
| `NODOWNLOADSKIP`    | `skip_download_if_extracted=false`           | `[global]`          |
| `VERIFYMARKER`      | `verify_archive_marker_before_download=true` | `[global]`          |
| `FORCEDOWNLOAD`     | `force_download=true`                        | `[global]`          |
| `NOICONS`           | `use_custom_icons=false`                     | `[global]`          |
| `DISABLECOUNTERS`   | `disable_counters=true`                      | `[global]`          |
| `CRCCHECK`          | `crccheck=true`                              | `[global]`          |
| `ENABLELOGGING`     | _none (CLI only)_                            | n/a                 |
| `TIMEOUT=<seconds>` | `timeout_seconds=<seconds>`                  | `[global]`          |
| `SKIPAGA`           | `skip_aga=true`                              | `[filters]`         |
| `SKIPCD`            | `skip_cd=true`                               | `[filters]`         |
| `SKIPNTSC`          | `skip_ntsc=true`                             | `[filters]`         |
| `SKIPNONENGLISH`    | `skip_non_english=true`                      | `[filters]`         |

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

Update classification uses the `.archive_index` cache at download time, before the cache entry is updated for the new archive. An archive is classified as updated only when:

- It shares the same strict identity as a cached entry: matching title, group, chipset, video format, language, media type, and disk count.
- The new archive's version number is strictly greater than the cached version.

If the new archive has no numeric version token, it is treated as New. If a cached candidate lacks a version token, it is ignored during update matching. This prevents false positives where many different releases share the same primary title prefix.

---

## Skip Detection and Caching

### Archive index (`.archive_index`)

Each letter folder under `GameFiles/<pack>/` contains a file called `.archive_index`. This file is a TAB-separated text file with one entry per line:

```
Academy_v1.2.lha	Academy
AcademyAGA_v1.0.lha	AcademyAGA
```

Column one is the exact archive filename. Column two is the name of the extracted game folder on disk.

The index is loaded into memory at startup, queried during download skip checks, updated after each successful extraction, and saved to disk during shutdown. Old entries that point to folders which no longer exist are pruned automatically on each shutdown.

The index exists because the extracted folder name does not always match the archive filename in a predictable way. Without it, confirming that a title is already installed would require scanning every subfolder and reading its `ArchiveName.txt` — a slow operation on classic Amiga storage.

### Marker file (`ArchiveName.txt`)

Inside each extracted game folder, whdfetch writes a two-line text file:

```
Games
Academy_v1.2.lha
```

Line one is the pack display name (for example, Games or Demos (Beta & Unofficial)). Line two is the exact archive filename.

If line two of this file matches the archive filename being processed, extraction is skipped unless `FORCEEXTRACT` is active. For download pre-skip, this marker check is only used when `VERIFYMARKER` (or `verify_archive_marker_before_download=true`) is enabled.

### Incomplete download recovery (`.downloading`)

Before each download begins, an empty marker file is created alongside the archive:

```
GameFiles/Games/A/Academy_v1.2.lha.downloading
```

On successful completion, the marker is deleted. If the program is interrupted mid-download, both the marker and the partial archive remain on disk.

On the next run, whdfetch detects the marker, deletes both the marker and the partial archive, and starts the download cleanly. Non-retryable server errors (400, 401, 403, 404) also clean up the marker immediately so failed entries do not generate futile retries on every subsequent run.


## Tips & Troubleshooting

**Nothing downloads — the program shows the help screen.**
You need to select at least one pack, either by using a CLI pack command or by setting `enabled=true` in a `[pack.*]` INI section. If you run whdfetch without selecting a pack or providing other arguments, it will show the help screen.

**The run downloads nothing new, even though new titles should be available.**
Check if `skip_download_if_extracted` is set to `true` in your INI file. By default, whdfetch skips titles that are already extracted by checking `.archive_index` and folders. If `VERIFYMARKER` is enabled, it also checks `ArchiveName.txt` metadata. Use `NODOWNLOADSKIP` to skip this logic but still check local files, or `FORCEDOWNLOAD` to download everything again.

**A previous run was interrupted, and archives are not being retried.**
Check for `.downloading` marker files next to the partial archives in `GameFiles/<pack>/<letter>/`. If you find them, the next run will detect and retry those downloads. If the markers are missing but partial archives are still there, delete the partial archives manually and try again.

**Extraction completes, but game folders have the wrong icon.**
Make sure `PROGDIR:Icons/WHD folder.info` exists and that the icon is named exactly `WHD folder.info`. If you used `NOICONS` or set `use_custom_icons=false` in the INI, custom icons won't be shown during that run.

**.lzx archives are being skipped.**
`c:unlzx` is either not installed or not located in `C:`. You can install it from Aminet. whdfetch won't fail if unlzx is missing; it will skip `.lzx` archives with a warning and continue processing the rest as usual.

**Downloads time out repeatedly.**
Try increasing the timeout. The default is 30 seconds. Use `TIMEOUT=60` or set `timeout_seconds=60` in the INI if your connection is slow or has high latency. Valid values range from 5 to 60 seconds.

**The collection is already installed. How do I avoid re-downloading everything?**
whdfetch handles this automatically if the `.archive_index` files and `ArchiveName.txt` markers are in place from previous runs. If markers are missing but the original `.lha`/`.lzx` archives are still present in `GameFiles/<pack>/<letter>/`, run `EXTRACTONLY` to process those archives and regenerate marker data. If you only have already-extracted folders (no archives), `EXTRACTONLY` cannot rebuild markers for them.

**I want to try different filter combinations without affecting my main collection.**
Use `EXTRACTTO=` to extract files to a different volume or partition. This way, everything under `GameFiles/` stays unchanged.

**Disk space is running low, and I want to remove the downloaded archives.**
Run whdfetch `PURGEARCHIVES` to delete all `.lha` and `.lzx` files under `GameFiles/` without removing the extracted game folders.

**I want to start fresh from a clean DAT download.**
Run whdfetch `PURGETEMP` to clear everything in `PROGDIR:temp`, including downloaded index and ZIP files. The next time you run it, fresh copies will be downloaded.

**The program crashes or behaves oddly at startup.**
Make sure `bsdsocket.library` is available and that Roadshow is running. Also, check `PROGDIR:logs/` for error log files. The errors log category includes all errors, no matter which subsystem caused them.

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
| `VERIFYMARKER`      | Enable extra `ArchiveName.txt` verification for pre-download skip   |
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