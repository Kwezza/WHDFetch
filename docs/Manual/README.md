# whdfetch

## Executive summary

whdfetch is an AmigaOS command-line tool that downloads and maintains a local library of Retroplay WHDLoad packs. It is aimed at users who want a repeatable way to fetch Games, Games Beta, Demos, Demos Beta, and Magazines from the Retroplay archive, keep them organised on disk, and avoid reprocessing titles that are already installed.

The program is not a front end and it is not a general-purpose archive manager. It is a workflow tool for one specific job: fetch Retroplay pack metadata, determine which WHDLoad archives belong to the selected packs, download the missing archives, optionally extract them, and record enough metadata to make later runs faster.

In normal use, whdfetch does four things for you:

1. Downloads the current Retroplay index page and the ZIP files that contain the DAT/XML metadata for the selected packs.
2. Extracts and parses those DAT files to build the list of WHDLoad archives to process.
3. Applies optional filters such as skipping AGA, CD, NTSC, or non-English titles.
4. Downloads archives into a structured GameFiles tree, optionally extracts them, and records what was installed so future runs can skip work.

## What the program does

At the user level, whdfetch is a library maintenance tool.

If you select one or more packs, the tool will download the current pack metadata from Retroplay, inspect the archive names listed in the DAT files, and then work through the selected list title by title. Archives are stored under pack-specific directories and then split again by first letter so the resulting tree remains usable on an Amiga filesystem.

When extraction is enabled, whdfetch also runs the external extraction tools, writes a two-line ArchiveName.txt marker file into the extracted game folder, updates a per-letter .archive_index cache, and can remove the original archive after a successful extraction. That combination is what lets later runs avoid expensive folder scans and skip titles that are already installed.

The supported content groups are:

1. Games
2. Games Beta
3. Demos
4. Demos Beta
5. Magazines

## How it works

The runtime pipeline is straightforward, but there are a few moving parts worth understanding.

### 1. Index and metadata discovery

whdfetch starts by downloading the Retroplay index page into temp/index.html. It parses that HTML looking for links that match the WHDLoad pack naming rules, then downloads the ZIP archives that contain the pack DAT files.

Those ZIP files are unpacked into the temp area. The extracted XML DAT files are then parsed for rom entries, which provide the archive filenames and, when available, metadata such as CRC and size.

### 2. Filtering and pack selection

Only the packs you select are processed. If you select DOWNLOADALL, the program enables all five packs in one run.

Before an archive is downloaded, the parsed metadata can be filtered. The current filters let you skip:

1. AGA releases
2. CDTV or CD32 releases
3. NTSC releases
4. Non-English releases

These filters operate on the parsed filename metadata, not on directory names alone.

### 3. Archive download and local cache reuse

For each remaining archive entry, whdfetch decides whether it needs to download anything at all.

If the archive is already extracted and the marker information still matches, the download is skipped by default. If the archive file itself is already present locally, the tool can also reuse that local file instead of fetching it again.

To guard against interrupted runs, whdfetch creates a zero-byte .downloading marker next to an archive while it is being fetched. If the tool sees that marker on a later run, it treats the archive as incomplete, deletes the partial file, removes the marker, and downloads the archive again.

### 4. Extraction and post-processing

If extraction is enabled, whdfetch extracts .lha archives with c:lha and .lzx archives with c:unlzx when UnLZX is installed. If UnLZX is not present, .lzx archives are skipped with a warning and the run continues.

After a successful extraction, the program writes ArchiveName.txt into the extracted folder. The file contains:

1. The pack display name on line 1
2. The exact archive filename on line 2

That second line is important. It is used later to decide whether the extracted copy already matches the incoming archive.

The program also updates a .archive_index file in each letter directory. That file maps archive filename to extracted folder name and avoids repeated directory scans on later runs.

### 5. Session reporting and logs

At the end of a run, whdfetch writes log files to PROGDIR:logs and, when there is reportable activity, writes a session report to PROGDIR:updates. The session report distinguishes between new downloads, updated downloads, local cache reuse, and extraction skips.

## Requirements and runtime assumptions

whdfetch expects a working Amiga environment. It is not self-contained.

The practical requirements are:

1. AmigaOS 3.0 or later
2. A working TCP/IP stack with bsdsocket.library available
3. c:unzip to extract the downloaded DAT ZIP files
4. c:lha to extract .lha archives when extraction is enabled
5. c:unlzx if you want .lzx archives to be extracted instead of skipped

The code and existing documentation both assume a reasonably fast Amiga setup. It will run on slower hardware, but the full-library workflow is much more comfortable on accelerated hardware or in WinUAE.

## Directory layout

The default working layout under PROGDIR looks like this:

```text
PROGDIR:
  whdfetch
  whdfetch.ini
  GameFiles/
    Games/
    Games Beta/
    Demos/
    Demos Beta/
    Magazines/
  temp/
    index.html
    Zip files/
    Dat files/
    holding/
  logs/
  updates/
```

Downloaded archives are stored under GameFiles/<pack>/<letter>/. If extraction happens in place, the extracted folders, ArchiveName.txt files, and .archive_index files live under the same pack and letter hierarchy.

If you use EXTRACTTO=<path>, the extracted content is written under the alternate path instead, but the same pack and letter structure is preserved.

## Configuration

whdfetch can be configured in two ways:

1. Command-line arguments for the current run
2. An optional INI file at PROGDIR:whdfetch.ini

If the legacy file PROGDIR:WHDDownloader.ini exists, it is also supported as a fallback.

The precedence order is:

1. Built-in defaults
2. INI overrides
3. Command-line arguments

This means the CLI always wins for the current run.

The INI file covers the main operating flags: pack enablement, extraction control, skip behavior, icons, CRC checking, timeout, and the filter flags.

## Command-line usage

General syntax:

```text
whdfetch [COMMAND...] [OPTION...]
```

Arguments are case-insensitive and can be supplied in any order.

The main pack-selection commands currently implemented in the code are:

1. HELP
2. DOWNLOADGAMES
3. DOWNLOADBETAGAMES
4. DOWNLOADDEMOS
5. DOWNLOADBETADEMOS
6. DOWNLOADMAGS
7. DOWNLOADALL
8. PURGEARCHIVES

The most useful operational options are:

1. SKIPAGA
2. SKIPCD
3. SKIPNTSC
4. SKIPNONENGLISH
5. VERBOSE
6. NOEXTRACT
7. EXTRACTTO=<path>
8. KEEPARCHIVES
9. DELETEARCHIVES
10. EXTRACTONLY
11. FORCEEXTRACT
12. NODOWNLOADSKIP
13. FORCEDOWNLOAD
14. NOICONS
15. DISABLECOUNTERS
16. CRCCHECK

## What the main options change

### NOEXTRACT

The program still downloads archives, but it leaves them as .lha or .lzx files in GameFiles and does not run the extraction pipeline. That means no ArchiveName.txt is written for newly downloaded titles, no icon work is done, and no archive is deleted after download.

### EXTRACTONLY

The program does not download game archives in this mode. Instead, it reads the selected pack DAT lists and tries to extract only the archives that already exist on disk. This is useful if you previously downloaded with NOEXTRACT or copied archive files into GameFiles manually.

### FORCEEXTRACT

By default, extraction is skipped when ArchiveName.txt already shows that the installed folder came from the same archive filename. FORCEEXTRACT bypasses that check and makes the tool extract again.

### NODOWNLOADSKIP and FORCEDOWNLOAD

By default, whdfetch skips downloads when it can prove the extracted version already matches. NODOWNLOADSKIP disables that skip logic. FORCEDOWNLOAD goes further and forces a new network download even if the tool could have reused an existing local archive or matching extraction.

### KEEPARCHIVES and DELETEARCHIVES

After a successful extraction, the default behavior is to delete the archive file. KEEPARCHIVES changes that for the current run and leaves the .lha or .lzx file in place. DELETEARCHIVES explicitly restores the default behavior.

### EXTRACTTO=<path>

Extraction is redirected to another writable location, but the pack and first-letter folder structure is still created under that target.

## Example command lines

The examples below focus on what actually happens during the run, not just on the syntax.

### Example 1

```text
whdfetch DOWNLOADGAMES
```

What happens:

1. The Games pack is selected.
2. The Retroplay index page is downloaded and scanned.
3. The current Games DAT ZIP is downloaded and unpacked.
4. The Games DAT XML is parsed into a list of archive names.
5. Each game archive is checked against the existing extracted markers and local archive cache.
6. Missing archives are downloaded into GameFiles/Games/<letter>/.
7. By default, downloaded archives are extracted immediately.
8. ArchiveName.txt and .archive_index are updated, and the original archive is deleted after successful extraction.

### Example 2

```text
whdfetch DOWNLOADALL SKIPAGA SKIPCD
```

What happens:

1. All five packs are selected.
2. The program downloads and processes the current metadata for each selected pack.
3. Before downloading archives, it filters out titles identified as AGA or CD-based releases.
4. Only the remaining titles are considered for download and extraction.
5. Existing installations are skipped where the marker and index checks show they already match.

This is a useful maintenance run if you want the broadest possible update pass but do not want AGA or CD32/CDTV content mixed into the result.

### Example 3

```text
whdfetch DOWNLOADGAMES NOEXTRACT
```

What happens:

1. The Games metadata is refreshed and parsed as usual.
2. The matching game archives are downloaded into GameFiles/Games/<letter>/.
3. Extraction is not attempted.
4. The archives remain on disk as .lha or .lzx files.
5. You can come back later and extract them with EXTRACTONLY or an external tool.

This mode is useful when you want to build an archive cache first and deal with extraction separately.

### Example 4

```text
whdfetch DOWNLOADGAMES EXTRACTTO=Games: KEEPARCHIVES
```

What happens:

1. Game archives are still downloaded under GameFiles/Games/<letter>/.
2. Extracted folders are written under Games:Games/<letter>/ instead of next to the archives.
3. Because KEEPARCHIVES is set, the downloaded archive files are not deleted after extraction.
4. The extraction markers and .archive_index cache are created under the extraction target, not under the archive storage tree.

This is useful if you want one location for your archive cache and another location for the extracted WHDLoad folders.

### Example 5

```text
whdfetch DOWNLOADBETAGAMES EXTRACTONLY EXTRACTTO=Games:
```

What happens:

1. The Games Beta pack is selected.
2. The program reads the pack metadata so it knows which archive names to look for.
3. It does not download game archives in this mode.
4. For each DAT entry, it checks whether the corresponding archive already exists locally.
5. If the archive exists, it is extracted to Games:Games Beta/<letter>/.
6. If the archive is missing locally, it is skipped without a per-file error.

This mode is useful after a previous NOEXTRACT run, or when you already have a populated archive library and want whdfetch to handle only the extraction pass.

### Example 6

```text
whdfetch DOWNLOADGAMES FORCEDOWNLOAD FORCEEXTRACT CRCCHECK
```

What happens:

1. The Games pack is processed.
2. Existing skip markers are ignored for both download and extraction.
3. Archives are fetched again from the network even if matching local state already exists.
4. CRC verification is enabled for archives where DAT CRC information is available.
5. Successfully downloaded archives are extracted again even if the installed folder already matches the previous archive name.

This is the aggressive maintenance mode to use when you want to rebuild or verify a pack rather than perform a normal incremental update.

### Example 7

```text
whdfetch PURGEARCHIVES
```

What happens:

1. The program asks for confirmation.
2. If you answer with Y, it recursively deletes downloaded .lha and .lzx files under GameFiles/.
3. Extracted folders are preserved.
4. The tool exits after the purge.

This is a cleanup command, not a download run.

## Notes on current behavior

There are a few implementation details worth knowing before you rely on the tool for large runs.

1. The user-facing command names in the current code are the DOWNLOAD... forms shown above. Some older documents still mention shorter aliases such as GAME or DEMO, but those are not the current help-text names.
2. whdfetch always needs the metadata pass for normal downloads because that is how it learns what archives belong to the selected packs.
3. EXTRACTONLY avoids archive downloads, but it still depends on the local DAT text lists created by earlier metadata processing.
4. .lzx extraction requires c:unlzx. Without it, those archives are skipped rather than treated as fatal errors.
5. The default timeout is 30 seconds, and timeout values are clamped in code to the supported range.

## Further reading

If you want the lower-level reference material that this manual was built from, start with:

1. docs/CLI_Reference.md
2. docs/ProjectInfo.md
3. docs/whdfetch.ini.sample
4. PROJECT_OVERVIEW.md
