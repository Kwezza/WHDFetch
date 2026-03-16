# WHDDownloader — CLI Reference

This document provides an in-depth description of every command-line argument accepted by
WHDDownloader. It is intended as source material for the user-facing instruction manual and
README file.

Running WHDDownloader without any arguments displays the built-in help screen and exits.

**General syntax:**

```
WHDDownloader [COMMAND...] [OPTION...]
```

Arguments are case-insensitive and can be given in any order. At least one pack-selection
command (or `EXTRACTONLY`) is required for the program to do useful work.

---

## Pack Selection Commands

These commands tell WHDDownloader which WHDLoad packs to process. You may specify one or
more on the same command line. If none are given (and no special mode like `EXTRACTONLY`
is active), the program shows the help screen and exits.

### DOWNLOADGAMES

Selects the **Games** pack for processing.

The program will download the latest Games DAT archive from the Retroplay site, extract the
XML file listing every game ROM, and then download each `.lha` archive into
`GameFiles/Games/<letter>/`. After downloading, each archive is extracted (unless
`NOEXTRACT` is specified).

**Example:**

```
WHDDownloader DOWNLOADGAMES
```

Downloads and processes the full Games pack. Already-downloaded archives are skipped
automatically unless `FORCEDOWNLOAD` is used.

### DOWNLOADBETAGAMES

Selects the **Games (Beta & Unofficial)** pack. These are WHDLoad game conversions that
are still in development or were produced by unofficial sources. Archives are stored under
`GameFiles/Games Beta/<letter>/`.

**Example:**

```
WHDDownloader DOWNLOADBETAGAMES
```

### DOWNLOADDEMOS

Selects the **Demos** pack. Archives are stored under `GameFiles/Demos/<letter>/`.

**Example:**

```
WHDDownloader DOWNLOADDEMOS
```

### DOWNLOADBETADEMOS

Selects the **Demos (Beta & Unofficial)** pack. Archives are stored under
`GameFiles/Demos Beta/<letter>/`.

**Example:**

```
WHDDownloader DOWNLOADBETADEMOS
```

### DOWNLOADMAGS

Selects the **Magazines** pack. Archives are stored under
`GameFiles/Magazines/<letter>/`.

**Example:**

```
WHDDownloader DOWNLOADMAGS
```

### DOWNLOADALL

Convenience shorthand that selects **all five packs** at once: Games, Games Beta, Demos,
Demos Beta, and Magazines. Equivalent to specifying every individual download command on
the same line.

**Example:**

```
WHDDownloader DOWNLOADALL
```

Is functionally identical to:

```
WHDDownloader DOWNLOADGAMES DOWNLOADBETAGAMES DOWNLOADDEMOS DOWNLOADBETADEMOS DOWNLOADMAGS
```

---

## Special Operation Modes

These options change the fundamental mode of operation. They can be combined with pack
selection commands.

### EXTRACTONLY

Processes only the `.lha` archives that are **already present** on disk in
`GameFiles/<pack>/<letter>/`. No HTTP downloads are performed — the network is not used
at all beyond the initial index and DAT fetch.

This mode is designed for situations where you have previously downloaded archives (perhaps
using `NOEXTRACT`) and now want to extract them without re-downloading. The program reads
the local DAT file for each selected pack, iterates over every ROM entry, checks whether
the corresponding `.lha` exists locally, and if so, runs the extraction pipeline on it.

When `EXTRACTONLY` is active, the `extract_archives` flag is forced to `TRUE` regardless of
any other setting (including `NOEXTRACT`).

Archives that are not found on disk are silently counted as "missing" and skipped — no
error is reported for individual missing files.

**Example — extract previously downloaded Games archives:**

```
WHDDownloader DOWNLOADGAMES EXTRACTONLY
```

**Example — extract all packs to an external volume:**

```
WHDDownloader DOWNLOADALL EXTRACTONLY EXTRACTTO=Games:
```

### HELP

Prints the built-in usage screen (the same one shown when no arguments are given) and
exits immediately. No network activity occurs.

**Example:**

```
WHDDownloader HELP
```

---

## Extraction Options

By default, WHDDownloader extracts every `.lha` archive immediately after downloading it.
The extraction pipeline performs these steps:

1. Runs `c:lha` to extract the archive contents into the target directory
2. Writes an `ArchiveName.txt` marker file inside the extracted game folder
3. Applies a custom WHDLoad drawer icon (if available in `PROGDIR:Icons/`)
4. Updates the `.archive_index` cache so future runs can skip this archive instantly
5. Optionally deletes the `.lha` archive after successful extraction

The options below control various aspects of this pipeline.

### NOEXTRACT

Completely disables the extraction pipeline. Archives are downloaded and left as `.lha`
files in `GameFiles/<pack>/<letter>/`. No `c:lha` is invoked, no `ArchiveName.txt` is
written, no icons are applied, and no archives are deleted.

This is useful if you want to batch-download archives now and extract them later (perhaps
with `EXTRACTONLY`), or if you prefer to use an external extraction tool such as
WHDArchiveExtractor from Aminet.

When `NOEXTRACT` is active, the program also skips the startup validation check for
`c:lha` — meaning the program will run even if `lha` is not installed.

**Note:** Because extraction is skipped, the pre-download skip check
(`skip_download_if_extracted`) will still function based on any *previous* extractions,
but no new `ArchiveName.txt` markers will be created during this run.

**Example — download without extracting:**

```
WHDDownloader DOWNLOADGAMES NOEXTRACT
```

### EXTRACTTO=\<path\>

Redirects extraction output to a different location instead of extracting in-place next to
the `.lha` files. The target path must already exist and be writable.

When specified, the extraction pipeline creates the pack and letter subdirectory structure
under the given path:

```
<path>/<pack>/<letter>/<game_folder>/
```

For example, `EXTRACTTO=Games:` with the Games pack would extract to
`Games:Games/A/AcademyFolder/`, `Games:Games/B/BatmanFolder/`, etc.

The program validates the target path at startup: if it does not exist or is not writable,
the program reports an error and exits before any downloads begin.

If `EXTRACTTO` is given with an empty value (`EXTRACTTO=`), the extraction path is cleared
and in-place extraction is used (the default).

Drawer icons (letter folders, pack folders) are created in the EXTRACTTO directory tree
as well, using custom icons from `PROGDIR:Icons/` if available.

**Example — extract to an external drive:**

```
WHDDownloader DOWNLOADGAMES EXTRACTTO=Games:
```

**Example — extract all packs to a separate partition:**

```
WHDDownloader DOWNLOADALL EXTRACTTO=Work:WHDLoad/
```

### KEEPARCHIVES

Prevents the `.lha` archive files from being deleted after successful extraction. This
overrides the compiled default and any INI setting.

By default, `delete_archives_after_extract` is `TRUE`, meaning archives are removed once
they have been successfully extracted. Using `KEEPARCHIVES` sets this to `FALSE`, leaving
the `.lha` files in `GameFiles/<pack>/<letter>/` even after extraction completes.

This is useful if you want to maintain a local archive cache alongside the extracted games,
perhaps for backup purposes or to re-extract later with different options.

**Example — download, extract, but keep the `.lha` files:**

```
WHDDownloader DOWNLOADGAMES KEEPARCHIVES
```

### DELETEARCHIVES

Ensures that `.lha` archive files are deleted after successful extraction. This overrides
the INI setting and is the compiled default behaviour.

Since the default is already to delete archives, this option is primarily useful to
explicitly override a `delete_archives_after_extract=false` setting in the INI file
without having to edit the file.

If extraction fails for a particular archive (e.g., `c:lha` returns an error), the archive
is **not** deleted — only successfully extracted archives are removed.

**Example — force archive deletion even if INI says keep them:**

```
WHDDownloader DOWNLOADGAMES DELETEARCHIVES
```

**Interaction with NOEXTRACT:** If both `NOEXTRACT` and `DELETEARCHIVES` are specified,
no extraction occurs and therefore no deletion occurs — `NOEXTRACT` takes priority because
archives are only deleted as a post-extraction step.

**Interaction with KEEPARCHIVES:** If both `KEEPARCHIVES` and `DELETEARCHIVES` are given
on the same command line, the last one processed wins (they are parsed in order). In
practice, avoid using both together.

### FORCEEXTRACT

Bypasses the `ArchiveName.txt` skip check and forces re-extraction of every archive, even
if a matching `ArchiveName.txt` marker already exists in the target game folder.

Normally, when the extraction pipeline encounters a game folder that already contains an
`ArchiveName.txt` whose second line matches the current archive filename, it skips
extraction — the game is considered up to date. `FORCEEXTRACT` disables this check,
causing every archive to be extracted regardless.

This is useful when:

- You suspect a previous extraction was incomplete or corrupted
- You want to re-apply updated drawer icons to all game folders
- You have changed the `EXTRACTTO` path and want to re-extract to the new location
- The archive contents have been updated (same filename but new data)

**Note:** `FORCEEXTRACT` does *not* affect the pre-download skip check. If the archive
has not yet been downloaded and an extraction marker exists, the *download* itself may
still be skipped unless `FORCEDOWNLOAD` or `NODOWNLOADSKIP` is also used. To fully
re-process everything, combine `FORCEEXTRACT` with `FORCEDOWNLOAD`.

**Example — re-extract all Games archives:**

```
WHDDownloader DOWNLOADGAMES FORCEEXTRACT
```

**Example — re-extract everything from scratch:**

```
WHDDownloader DOWNLOADALL FORCEEXTRACT FORCEDOWNLOAD
```

---

## Download Skip Options

WHDDownloader has two layers of skip logic to avoid redundant work:

1. **Local archive skip:** If the `.lha` file already exists on disk, the download is
   skipped (the existing file is still passed to the extraction pipeline).
2. **Extraction marker skip:** If no local `.lha` exists but an `ArchiveName.txt` marker
   in the extracted game folder matches the archive filename, both the download *and*
   extraction are skipped entirely. This uses the `.archive_index` cache for speed.

These options control the second layer (extraction marker skip).

### NODOWNLOADSKIP

Disables the extraction-marker-based pre-download skip. When active, the program will
download every archive that does not already exist as a `.lha` file on disk, regardless of
whether an `ArchiveName.txt` marker exists in the extraction target.

This is equivalent to setting `skip_download_if_extracted=false`. The local archive file
skip (layer 1) still applies — if the `.lha` already exists, the download is skipped.

**Use case:** You have deleted some extracted game folders but want to re-download and
re-extract only the archives that are missing from disk.

**Example:**

```
WHDDownloader DOWNLOADGAMES NODOWNLOADSKIP
```

### FORCEDOWNLOAD

Forces the program to download every archive, bypassing **both** the local-file-exists
check and the extraction-marker check. Even if the `.lha` file is already present on disk,
it will be re-downloaded from the Retroplay server.

This is more aggressive than `NODOWNLOADSKIP` — it re-downloads everything unconditionally.

**Use case:** You suspect local archives are corrupted or truncated and want a clean
re-download of the entire pack.

**Warning:** This will re-download every archive in the selected packs, which may involve
thousands of files and many gigabytes of data. Use with care on slow or metered connections.

**Example — force full re-download of Games:**

```
WHDDownloader DOWNLOADGAMES FORCEDOWNLOAD
```

**Example — full clean re-download and re-extract:**

```
WHDDownloader DOWNLOADGAMES FORCEDOWNLOAD FORCEEXTRACT
```

---

## Filter Options

Filter options allow you to exclude certain categories of WHDLoad archives from being
downloaded. When a filter is active, archives matching the filter criteria are silently
skipped during the ROM download phase. The filtering is based on metadata encoded in the
archive filename (e.g., `_AGA`, `_NTSC`, `_CD32`, language codes like `_De`, `_Fr`).

Multiple filters can be combined and they stack — an archive is downloaded only if it
passes **all** active filters.

Filters apply to all selected packs equally.

### SKIPAGA

Skips all archives whose filename indicates they require the AGA chipset (e.g., filenames
containing `_AGA`). AGA games require an Amiga 1200 or Amiga 4000 — use this filter if
you are targeting an OCS/ECS Amiga (A500, A600, A2000, etc.).

**Example — download games but skip AGA titles:**

```
WHDDownloader DOWNLOADGAMES SKIPAGA
```

### SKIPCD

Skips all archives for CD-based media formats, including CD32, CDTV, and CDRom titles.
These typically require a CD drive or CD32 hardware that may not be available in a standard
WHDLoad setup.

**Example:**

```
WHDDownloader DOWNLOADGAMES SKIPCD
```

### SKIPNTSC

Skips all archives flagged as NTSC-only. NTSC games were designed for the 60 Hz video
standard used in North America and Japan. If your Amiga is a PAL model (50 Hz, common in
Europe), some NTSC games may have timing or display issues.

**Example:**

```
WHDDownloader DOWNLOADGAMES SKIPNTSC
```

### SKIPNONENGLISH

Skips all archives that are identified as non-English language versions. The parser
recognises language codes such as `De` (German), `Fr` (French), `It` (Italian), `Es`
(Spanish), `NL` (Dutch), `Fi` (Finnish), `Dk` (Danish), `Pl` (Polish), `Gr` (Greek), and
`Cz` (Czech) embedded in the archive filename.

If an archive is identified as English (the default when no language code is present), it
is always included. If an archive has a non-English language code, it is excluded when
this filter is active.

**Example — download only English-language games:**

```
WHDDownloader DOWNLOADGAMES SKIPNONENGLISH
```

### Combining Filters

Filters are cumulative. An archive must pass every enabled filter to be downloaded.

**Example — download only English PAL OCS/ECS games (no AGA, no CD, no NTSC, no
non-English):**

```
WHDDownloader DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

---

## Output and Reporting Options

### NOSKIPREPORT

Suppresses the on-screen messages that are printed each time an existing archive is
skipped. During a large update run where most archives already exist, the console can fill
rapidly with "File already exists, skipping: ..." messages. `NOSKIPREPORT` silences these,
giving a cleaner output.

Skipped files are still counted in the final summary statistics. Log files are unaffected —
skip events are always logged regardless of this option.

**Example:**

```
WHDDownloader DOWNLOADALL NOSKIPREPORT
```

### QUIET

Suppresses verbose output from the UnZip utility during DAT file extraction. This makes
the console output cleaner during the index-download and DAT-extraction phases.

Archive downloads still show progress information. This option only affects the UnZip
output produced when extracting the `.zip` DAT archives.

**Example:**

```
WHDDownloader DOWNLOADGAMES QUIET
```

---

## Icon Options

### NOICONS

Disables custom icon replacement for extracted game folders. When active, the extraction
pipeline will not apply the WHDLoad drawer icon template from `PROGDIR:Icons/WHD folder.info`
to newly extracted game folders, and will not create custom letter or pack drawer icons in
the extraction target.

System default drawer icons will still be created if a folder has no icon at all, but
custom icons from the `PROGDIR:Icons/` directory will not be used.

This also disables the icon unsnapshotting step, since there is no custom icon to
unsnapshot.

**Use case:** You have your own icon set and do not want WHDDownloader overwriting your
drawer icons, or you are running in a headless/script configuration where icons are
irrelevant.

**Example:**

```
WHDDownloader DOWNLOADGAMES NOICONS
```

---

## INI File Interaction

Most options have corresponding keys in `PROGDIR:WHDDownloader.ini`. The precedence order
is:

1. **Compiled defaults** — built into the program (e.g., `delete_archives_after_extract = TRUE`)
2. **INI file** — loaded at startup, overrides compiled defaults
3. **CLI arguments** — parsed after the INI, override everything

This means CLI arguments always have the final say. For example, if the INI file contains
`delete_archives_after_extract=false` but you pass `DELETEARCHIVES` on the command line,
archives **will** be deleted.

### Pack selection via INI

Pack selection can be defined in INI using each pack section's `enabled` key:

- `[pack.games] enabled=true|false`
- `[pack.games_beta] enabled=true|false`
- `[pack.demos] enabled=true|false`
- `[pack.demos_beta] enabled=true|false`
- `[pack.magazines] enabled=true|false`

`enabled=true` selects that pack; `enabled=false` deselects it.

### Pack-selection precedence rule

Pack selection follows this runtime rule:

1. Start from defaults (no packs selected)
2. Apply INI `[pack.*].enabled` values
3. If **any** CLI pack-selection command is present (`DOWNLOADGAMES`, `DOWNLOADBETAGAMES`,
   `DOWNLOADDEMOS`, `DOWNLOADBETADEMOS`, `DOWNLOADMAGS`, `DOWNLOADALL`), all INI pack
   selections are first cleared for this run, then CLI pack selections are applied

In short: CLI pack-selection commands are authoritative whenever at least one is provided.

### Mapping of CLI arguments to INI keys

| CLI Argument      | INI Key                          | INI Section  |
|-------------------|----------------------------------|-------------|
| DOWNLOADGAMES     | `enabled=true`                   | `[pack.games]` |
| DOWNLOADBETAGAMES | `enabled=true`                   | `[pack.games_beta]` |
| DOWNLOADDEMOS     | `enabled=true`                   | `[pack.demos]` |
| DOWNLOADBETADEMOS | `enabled=true`                   | `[pack.demos_beta]` |
| DOWNLOADMAGS      | `enabled=true`                   | `[pack.magazines]` |
| DOWNLOADALL       | all five `enabled=true` values   | `[pack.*]` |
| NOEXTRACT         | `extract_archives=false`         | `[global]`  |
| EXTRACTTO=\<path\>| `extract_path=<path>`            | `[global]`  |
| KEEPARCHIVES      | `delete_archives_after_extract=false` | `[global]` |
| DELETEARCHIVES    | `delete_archives_after_extract=true`  | `[global]` |
| FORCEEXTRACT      | (no INI equivalent)              | —           |
| NODOWNLOADSKIP    | `skip_download_if_extracted=false`| `[global]`  |
| FORCEDOWNLOAD     | `force_download=true`            | `[global]`  |
| NOICONS           | `use_custom_icons=false`         | `[global]`  |
| SKIPAGA           | `skip_aga=true`                  | `[filters]` |
| SKIPCD            | `skip_cd=true`                   | `[filters]` |
| SKIPNTSC          | `skip_ntsc=true`                 | `[filters]` |
| SKIPNONENGLISH    | `skip_non_english=true`          | `[filters]` |

---

## Compiled Defaults Summary

These are the values used when no INI file is present and no CLI arguments are given:

| Setting                        | Default Value |
|--------------------------------|---------------|
| `extract_archives`             | `TRUE` (extraction enabled) |
| `skip_existing_extractions`    | `TRUE` (skip if ArchiveName.txt matches) |
| `force_extract`                | `FALSE` |
| `skip_download_if_extracted`   | `TRUE` (skip download if already extracted) |
| `force_download`               | `FALSE` |
| `extract_path`                 | `NULL` (extract in-place) |
| `delete_archives_after_extract`| `TRUE` (delete after extraction) |
| `use_custom_icons`             | `TRUE` |
| `unsnapshot_icons`             | `TRUE` |
| `skip_aga`                     | `0` (disabled) |
| `skip_cd`                      | `0` (disabled) |
| `skip_ntsc`                    | `0` (disabled) |
| `skip_non_english`             | `0` (disabled) |

---

## Common Scenarios

### First-time full download

```
WHDDownloader DOWNLOADALL
```

Downloads all packs, extracts every archive, deletes `.lha` files after extraction, applies
custom icons, and builds the `.archive_index` cache.

### Update run (typical ongoing use)

```
WHDDownloader DOWNLOADALL NOSKIPREPORT
```

Downloads only new or updated archives (everything already extracted is skipped
automatically). Skip messages are suppressed for a cleaner output.

### Download now, extract later

```
WHDDownloader DOWNLOADGAMES NOEXTRACT KEEPARCHIVES
```

Downloads all Games `.lha` files but does not extract them. Archives are preserved on disk.
You can extract them later with:

```
WHDDownloader DOWNLOADGAMES EXTRACTONLY
```

### Extract to an external drive

```
WHDDownloader DOWNLOADGAMES EXTRACTTO=Games: KEEPARCHIVES
```

Downloads games, extracts them to the `Games:` volume, and keeps the `.lha` archives in
`GameFiles/Games/<letter>/` as a local cache.

### Re-extract everything from scratch

```
WHDDownloader DOWNLOADGAMES FORCEEXTRACT
```

Re-runs extraction on every archive, overwriting existing `ArchiveName.txt` markers and
re-applying icons. Does not re-download archives that already exist locally.

### Full clean rebuild

```
WHDDownloader DOWNLOADALL FORCEDOWNLOAD FORCEEXTRACT
```

Re-downloads and re-extracts every archive in every pack, ignoring all skip checks. This is
the "nuclear option" and will take considerable time and bandwidth.

### OCS-only English PAL setup

```
WHDDownloader DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

Downloads only games that will work on a base PAL OCS Amiga with floppy-based WHDLoad
installs in English.
