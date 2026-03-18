# whdfetch — CLI Reference

This document provides an in-depth description of every command-line argument accepted by
whdfetch. It is intended as source material for the user-facing instruction manual and
README file.

Running whdfetch without any arguments displays the built-in help screen and exits.

**General syntax:**

```text
whdfetch [COMMAND...] [OPTION...]
```

Arguments are case-insensitive and can be given in any order.

For the program to do useful work, at least one pack must be selected, either via CLI
pack-selection commands or via the INI file. `EXTRACTONLY` is an operation mode, not a
pack selector, so it still depends on pack selection coming from the CLI or INI.

CLI arguments override INI settings for the current run.

---

## Mutually Exclusive and Precedence Rules

The following command interactions are important:

* `KEEPARCHIVES` and `DELETEARCHIVES` are opposing commands. Only one final effective
  value can apply.
* `EXTRACTONLY` forces extraction on, so it overrides `NOEXTRACT`.
* `NOEXTRACT` disables the extraction pipeline and therefore disables post-extraction
  actions such as archive deletion, marker creation, icon application, and icon
  unsnapshotting for that run.
* `FORCEEXTRACT` affects extraction only. It does not force re-downloads.
* `FORCEDOWNLOAD` is stronger than `NODOWNLOADSKIP`. It bypasses both skip layers,
  whereas `NODOWNLOADSKIP` bypasses only the extraction-marker-based pre-download skip.

**Recommended runtime behaviour:** where contradictory commands are supplied, the program
should ideally warn the user instead of relying silently on parse order.

---

## Pack Selection Commands

These commands explicitly select which WHDLoad packs to process for the current run. If any
CLI pack-selection command is present, it overrides all pack-selection choices from the INI
file for that run.

You may specify one or more on the same command line. If no packs are selected via either
CLI or INI, the program shows the help screen and exits.

### DOWNLOADGAMES

Selects the **Games** pack for processing. Archives are stored under
`GameFiles/Games/<letter>/`.

**Example:**

```text
whdfetch DOWNLOADGAMES
```

Downloads and processes the full Games pack. Already-downloaded archives are skipped
automatically unless `FORCEDOWNLOAD` is used.

### DOWNLOADBETAGAMES

Selects the **Games (Beta & Unofficial)** pack. These are WHDLoad game conversions that
are still in development or were produced by unofficial sources. Archives are stored under
`GameFiles/Games Beta/<letter>/`.

**Example:**

```text
whdfetch DOWNLOADBETAGAMES
```

### DOWNLOADDEMOS

Selects the **Demos** pack. Archives are stored under `GameFiles/Demos/<letter>/`.

**Example:**

```text
whdfetch DOWNLOADDEMOS
```

### DOWNLOADBETADEMOS

Selects the **Demos (Beta & Unofficial)** pack. Archives are stored under
`GameFiles/Demos Beta/<letter>/`.

**Example:**

```text
whdfetch DOWNLOADBETADEMOS
```

### DOWNLOADMAGS

Selects the **Magazines** pack. Archives are stored under
`GameFiles/Magazines/<letter>/`.

**Example:**

```text
whdfetch DOWNLOADMAGS
```

### DOWNLOADALL

Convenience shorthand that selects **all five packs** at once: Games, Games Beta, Demos,
Demos Beta, and Magazines. It is functionally identical to specifying every individual
pack-selection command on the same line.

**Example:**

```text
whdfetch DOWNLOADALL
```

Equivalent to:

```text
whdfetch DOWNLOADGAMES DOWNLOADBETAGAMES DOWNLOADDEMOS DOWNLOADBETADEMOS DOWNLOADMAGS
```

---

## Special Operation Modes

These options change the fundamental mode of operation. They can be combined with pack
selection commands.

### EXTRACTONLY

Processes only the archives (`.lha` and `.lzx`) that are already present on disk in
`GameFiles/<pack>/<letter>/`. No game or demo archives are downloaded. Network activity may
still occur for index and DAT metadata retrieval.

This mode is designed for situations where you have previously downloaded archives,
perhaps using `NOEXTRACT`, and now want to extract them without re-downloading. The
program reads the DAT file for each selected pack, iterates over each ROM entry, checks
whether the corresponding archive exists locally, and if so, runs the extraction pipeline
on it.

When `EXTRACTONLY` is active, the `extract_archives` flag is forced to `TRUE` regardless of
any other setting, including `NOEXTRACT`.

Archives that are not found on disk are silently counted as missing and skipped. No error
is reported for individual missing files.

**Interaction with NOEXTRACT:** if both are supplied, `NOEXTRACT` is ignored because
`EXTRACTONLY` requires extraction.

**Example — extract previously downloaded Games archives:**

```text
whdfetch DOWNLOADGAMES EXTRACTONLY
```

**Example — extract all packs to an external volume:**

```text
whdfetch DOWNLOADALL EXTRACTONLY EXTRACTTO=Games:
```

### HELP

Prints the built-in usage screen, the same one shown when no arguments are given, and exits
immediately before any normal pack-selection, download, or extraction logic runs.

**Example:**

```text
whdfetch HELP
```

---

## Extraction Options

By default, whdfetch extracts archives immediately after downloading them (`.lha` and `.lzx`).
The extraction pipeline performs these steps:

1. Runs `c:lha` for `.lha` archives or `c:unlzx` for `.lzx` archives
2. Writes an `ArchiveName.txt` marker file inside the extracted game folder
3. Applies a custom WHDLoad drawer icon, if available in `PROGDIR:Icons/`
4. Updates the `.archive_index` cache so future runs can skip this archive instantly
5. Optionally deletes the archive file after successful extraction

If `c:unlzx` is not installed, `.lzx` archives are skipped with a warning and the run continues.

The options below control various aspects of this pipeline.

### NOEXTRACT

Completely disables the extraction pipeline. Archives are downloaded and left as archive files
(`.lha`/`.lzx`) in `GameFiles/<pack>/<letter>/`. No extractor command is invoked, no `ArchiveName.txt` is
written, no icons are applied, no icons are unsnapshotted, no `.archive_index` entry is
created from a fresh extraction, and no archives are deleted.

This is useful if you want to batch-download archives now and extract them later, perhaps
with `EXTRACTONLY`, or if you prefer to use an external extraction tool such as
WHDArchiveExtractor from Aminet.

When `NOEXTRACT` is active, the program also skips extraction-tool startup validation,
meaning the program will run even if extraction tools are not installed.

Because extraction is skipped, the pre-download skip check
(`skip_download_if_extracted`) still works based on any previous extractions, but no new
`ArchiveName.txt` markers are created during this run.

`NOEXTRACT` is ignored if `EXTRACTONLY` is also specified, because `EXTRACTONLY` forces
extraction on.

**Example — download without extracting:**

```text
whdfetch DOWNLOADGAMES NOEXTRACT
```

### EXTRACTTO=<path>

Redirects extraction output to a different location instead of extracting in place next to
the `.lha` files. The target path must already exist and be writable.

When specified, the extraction pipeline creates the pack and letter subdirectory structure
under the given path:

```text
<path>/<pack>/<letter>/<game_folder>/
```

For example, `EXTRACTTO=Games:` with the Games pack would extract to
`Games:Games/A/AcademyFolder/`, `Games:Games/B/BatmanFolder/`, and so on.

The program validates the target path at startup. If it does not exist or is not writable,
the program reports an error and exits before any downloads begin.

If `EXTRACTTO` is given with an empty value (`EXTRACTTO=`), the extraction path is cleared
and in-place extraction is used, which is the default.

When `EXTRACTTO` is active, all extracted content, extraction markers, and any generated
letter or pack drawer icons are created under the target path rather than alongside the
downloaded archives.

**Example — extract to an external drive:**

```text
whdfetch DOWNLOADGAMES EXTRACTTO=Games:
```

**Example — extract all packs to a separate partition:**

```text
whdfetch DOWNLOADALL EXTRACTTO=Work:WHDLoad/
```

### KEEPARCHIVES

Prevents archive files from being deleted after successful extraction. This
overrides the compiled default and any INI setting.

By default, `delete_archives_after_extract` is `TRUE`, meaning archives are removed once
they have been successfully extracted. Using `KEEPARCHIVES` sets this to `FALSE`, leaving
the archive files in `GameFiles/<pack>/<letter>/` even after extraction completes.

This is useful if you want to maintain a local archive cache alongside the extracted games,
perhaps for backup purposes or to re-extract later with different options.

`KEEPARCHIVES` opposes `DELETEARCHIVES`. If both are supplied, they conflict and only one
final effective setting can apply.

**Example — download, extract, but keep the archive files:**

```text
whdfetch DOWNLOADGAMES KEEPARCHIVES
```

### DELETEARCHIVES

Explicitly enables post-extraction archive deletion for the current run. This is the
compiled default, so the command is mainly useful when overriding an INI setting that keeps
archives.

If extraction fails for a particular archive, for example if `c:lha` returns an error, the
archive is not deleted. Only successfully extracted archives are removed.

If `NOEXTRACT` is active, `DELETEARCHIVES` has no effect because archive deletion only
happens after a successful extraction pass.

`DELETEARCHIVES` conflicts with `KEEPARCHIVES`. Only one final effective setting should be
used.

**Example — force archive deletion even if INI says keep them:**

```text
whdfetch DOWNLOADGAMES DELETEARCHIVES
```

### FORCEEXTRACT

Bypasses the `ArchiveName.txt` skip check and forces re-extraction of every archive, even
if a matching `ArchiveName.txt` marker already exists in the target game folder.

Normally, when the extraction pipeline encounters a game folder that already contains an
`ArchiveName.txt` whose second line matches the current archive filename, it skips
extraction because the game is considered up to date. `FORCEEXTRACT` disables this check,
causing every archive to be extracted regardless.

This is useful when:

* You suspect a previous extraction was incomplete or corrupted
* You want to re-apply updated drawer icons to all game folders
* You have changed the `EXTRACTTO` path and want to re-extract to the new location
* The archive contents have been updated under the same filename

`FORCEEXTRACT` does not affect the pre-download skip check. If the archive has not yet been
downloaded and an extraction marker exists, the download itself may still be skipped unless
`FORCEDOWNLOAD` or `NODOWNLOADSKIP` is also used.

Think of `FORCEEXTRACT` as: re-extract what I already have access to.
Think of `FORCEDOWNLOAD` as: re-download the archives again.
Use both together for a full rebuild.

**Example — re-extract all Games archives:**

```text
whdfetch DOWNLOADGAMES FORCEEXTRACT
```

**Example — re-extract everything from scratch:**

```text
whdfetch DOWNLOADALL FORCEEXTRACT FORCEDOWNLOAD
```

---

## Download Skip Options

whdfetch has two layers of skip logic to avoid redundant work:

1. **Local archive skip:** if the `.lha` file already exists on disk, the download is
   skipped. The existing archive is still passed to the extraction pipeline.
2. **Extraction marker skip:** if no local `.lha` exists but an `ArchiveName.txt` marker
   in the extracted game folder matches the archive filename, both the download and
   extraction are skipped entirely. This uses the `.archive_index` cache for speed.

The options below control the second layer.

### NODOWNLOADSKIP

Advanced override option.

Disables the extraction-marker-based pre-download skip. When active, the program downloads
every archive that does not already exist as a `.lha` file on disk, regardless of whether
an `ArchiveName.txt` marker exists in the extraction target.

This is equivalent to setting `skip_download_if_extracted=false`. The local archive file
skip, layer 1, still applies. If the `.lha` already exists, the download is skipped.

`NODOWNLOADSKIP` only disables the already-extracted shortcut. It does not bypass the
archive-file-already-exists check.

**Use case:** you have deleted some extracted game folders but want to re-download and
re-extract only the archives that are missing from disk.

**Example:**

```text
whdfetch DOWNLOADGAMES NODOWNLOADSKIP
```

### FORCEDOWNLOAD

Forces the program to download every archive, bypassing both the local-file-exists check
and the extraction-marker check. Even if the `.lha` file is already present on disk, it is
re-downloaded from the Retroplay server.

This is more aggressive than `NODOWNLOADSKIP`. It re-downloads everything unconditionally.

`FORCEDOWNLOAD` is the stronger override. It bypasses both skip layers, whereas
`NODOWNLOADSKIP` bypasses only the extraction-marker layer.

**Use case:** you suspect local archives are corrupted or truncated and want a clean
re-download of the entire pack.

**Warning:** this re-downloads every archive in the selected packs, which may involve
thousands of files and many gigabytes of data. Use with care on slow or metered
connections.

**Example — force full re-download of Games:**

```text
whdfetch DOWNLOADGAMES FORCEDOWNLOAD
```

**Example — full clean re-download and re-extract:**

```text
whdfetch DOWNLOADGAMES FORCEDOWNLOAD FORCEEXTRACT
```

---

## Filter Options

Filter options allow you to exclude certain categories of WHDLoad archives from being
processed. Filtering is based on metadata encoded in the archive filename, for example
`_AGA`, `_NTSC`, `_CD32`, and language codes such as `_De` or `_Fr`.

Multiple filters can be combined and they stack. An archive is processed only if it passes
all active filters.

Filters apply to all selected packs equally.

### SKIPAGA

Skips all archives whose filename indicates they require the AGA chipset, for example
filenames containing `_AGA`. AGA games require an Amiga 1200 or Amiga 4000, so use this
filter if you are targeting an OCS or ECS Amiga such as an A500, A600, or A2000.

**Example — download games but skip AGA titles:**

```text
whdfetch DOWNLOADGAMES SKIPAGA
```




### INI: timeout_seconds

Set `timeout_seconds` in the `[global]` section of `PROGDIR:whdfetch.ini` to adjust the
default timeout for every run. The CLI `TIMEOUT=<seconds>` argument still overrides this value
for the current invocation, but the INI provides a persistent fallback.

Values outside the 5-60 second range are clamped, and invalid values are ignored.


### SKIPCD

Skips all archives for CD-based media formats, including CD32, CDTV, and CDRom titles.

These typically require a CD drive or CD32 hardware that may not be available in a standard
WHDLoad setup.

**Example:**

```text
whdfetch DOWNLOADGAMES SKIPCD
```

### SKIPNTSC

Skips all archives flagged as NTSC-only. NTSC games were designed for the 60 Hz video
standard used in North America and Japan. If your Amiga is a PAL model, 50 Hz and common
in Europe, some NTSC games may have timing or display issues.

**Example:**

```text
whdfetch DOWNLOADGAMES SKIPNTSC
```

### SKIPNONENGLISH

Skips all archives identified as non-English language versions. The parser recognises
language codes such as `De`, `Fr`, `It`, `Es`, `NL`, `Fi`, `Dk`, `Pl`, `Gr`, and `Cz`
embedded in the archive filename.

If an archive is identified as English, which is the default when no language code is
present, it is included. If an archive has a non-English language code, it is excluded when
this filter is active.

**Example — download only English-language games:**

```text
whdfetch DOWNLOADGAMES SKIPNONENGLISH
```

### Combining Filters

Filters are cumulative. An archive must pass every enabled filter to be processed.

**Example — download only English PAL OCS/ECS games, no AGA, no CD, no NTSC, no
non-English:**

```text
whdfetch DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

---

## Connection Timeout

### TIMEOUT=<seconds>

Controls the maximum number of seconds whdfetch waits for any HTTP activity before
aborting the current transfer. The timeout applies to every HTTP download the program performs
during a run: the index.html fetch, the ZIP/DAT downloads, and the individual `.lha` pack
downloads. A timeout is triggered from the shared download library whenever a socket stays idle
for longer than the configured value.

Valid values are 5 through 60 seconds. The default is 30 seconds when no other source overrides
it. CLI arguments are evaluated after the INI, so `TIMEOUT=45` will supersede an
`timeout_seconds` setting in the INI for that run.

**Example — increase the timeout for a slow connection:**

```text
whdfetch DOWNLOADALL TIMEOUT=45
```

## Output and Reporting Options

### NOSKIPREPORT

Suppresses the on-screen messages printed each time an existing archive is skipped. During
a large update run where most archives already exist, the console can fill rapidly with
messages such as `File already exists, skipping: ...`. `NOSKIPREPORT` silences these,
giving cleaner output.

Skip events are still counted in the final summary statistics and are still written to log
files.

**Example:**

```text
whdfetch DOWNLOADALL NOSKIPREPORT
```

### QUIET

Suppresses verbose output from DAT archive extraction. This makes the console output
cleaner during the index-download and DAT-extraction phases.

Archive downloads still show progress information. This option affects only the DAT archive
extraction output.

**Example:**

```text
whdfetch DOWNLOADGAMES QUIET
```

---

## Icon Options

### NOICONS

Disables custom icon replacement for extracted game folders. When active, the extraction
pipeline does not apply the WHDLoad drawer icon template from
`PROGDIR:Icons/WHD folder.info` to newly extracted game folders, and does not create custom
letter or pack drawer icons in the extraction target.

System default drawer icons may still be created if a folder has no icon at all, but
custom icons from `PROGDIR:Icons/` are not used.

This also disables the icon unsnapshotting step, because there is no custom icon to
unsnapshot.

There is no separate CLI switch for icon unsnapshotting. It is an internal part of the
custom-icon workflow, so disabling custom icons also disables unsnapshotting.

**Use case:** you have your own icon set and do not want whdfetch overwriting your
drawer icons, or you are running in a headless or scripted configuration where icons are
irrelevant.

**Example:**

```text
whdfetch DOWNLOADGAMES NOICONS
```

---

## INI File Interaction

Most options have corresponding keys in `PROGDIR:whdfetch.ini`. The precedence order
is:

1. **Compiled defaults** — built into the program
2. **INI file** — loaded at startup, overrides compiled defaults
3. **CLI arguments** — parsed after the INI, override everything

This means CLI arguments always have the final say. For example, if the INI file contains
`delete_archives_after_extract=false` but you pass `DELETEARCHIVES` on the command line,
archives will be deleted.

In practice, the CLI is the per-run override layer, while the INI provides persistent
defaults.

### Pack selection via INI

Pack selection can be defined in the INI file using each pack section's `enabled` key:

* `[pack.games] enabled=true|false`
* `[pack.games_beta] enabled=true|false`
* `[pack.demos] enabled=true|false`
* `[pack.demos_beta] enabled=true|false`
* `[pack.magazines] enabled=true|false`

`enabled=true` selects that pack and `enabled=false` deselects it.

### Pack-selection precedence rule

Pack selection follows this runtime rule:

1. Start from defaults, with no packs selected
2. Apply INI `[pack.*].enabled` values
3. If any CLI pack-selection command is present, `DOWNLOADGAMES`, `DOWNLOADBETAGAMES`,
   `DOWNLOADDEMOS`, `DOWNLOADBETADEMOS`, `DOWNLOADMAGS`, or `DOWNLOADALL`, all INI pack
   selections are first cleared for that run and then CLI pack selections are applied

In short, CLI pack-selection commands are authoritative whenever at least one is provided.

### Mapping of CLI arguments to INI keys

| CLI Argument      | INI Key                               | INI Section         | Notes                          |
| ----------------- | ------------------------------------- | ------------------- | ------------------------------ |
| DOWNLOADGAMES     | `enabled=true`                        | `[pack.games]`      | CLI pack selector              |
| DOWNLOADBETAGAMES | `enabled=true`                        | `[pack.games_beta]` | CLI pack selector              |
| DOWNLOADDEMOS     | `enabled=true`                        | `[pack.demos]`      | CLI pack selector              |
| DOWNLOADBETADEMOS | `enabled=true`                        | `[pack.demos_beta]` | CLI pack selector              |
| DOWNLOADMAGS      | `enabled=true`                        | `[pack.magazines]`  | CLI pack selector              |
| DOWNLOADALL       | all five `enabled=true` values        | `[pack.*]`          | Shorthand for all packs        |
| NOEXTRACT         | `extract_archives=false`              | `[global]`          | Overridden by `EXTRACTONLY`    |
| EXTRACTTO=<path>  | `extract_path=<path>`                 | `[global]`          | Per-run extraction destination |
| KEEPARCHIVES      | `delete_archives_after_extract=false` | `[global]`          | Opposes `DELETEARCHIVES`       |
| DELETEARCHIVES    | `delete_archives_after_extract=true`  | `[global]`          | Default already true           |
| FORCEEXTRACT      | no INI equivalent                     | —                   | Per-run only                   |
| NODOWNLOADSKIP    | `skip_download_if_extracted=false`    | `[global]`          | Advanced override              |
| FORCEDOWNLOAD     | `force_download=true`                 | `[global]`          | Stronger than `NODOWNLOADSKIP` |
| NOICONS           | `use_custom_icons=false`              | `[global]`          | Also disables unsnapshotting   |
| TIMEOUT=<seconds> | `timeout_seconds=<seconds>`           | `[global]`          | Range 5-60 seconds, CLI overrides INI |
| SKIPAGA           | `skip_aga=true`                       | `[filters]`         | Filter                         |
| SKIPCD            | `skip_cd=true`                        | `[filters]`         | Filter                         |
| SKIPNTSC          | `skip_ntsc=true`                      | `[filters]`         | Filter                         |
| SKIPNONENGLISH    | `skip_non_english=true`               | `[filters]`         | Filter                         |

---

## Compiled Defaults Summary

These are the values used when no INI file is present and no CLI arguments are given.

Some defaults below correspond directly to user-facing CLI or INI settings, while others
are internal behaviour flags shown for completeness.

| Setting                         | Default Value                               |
| ------------------------------- | ------------------------------------------- |
| `extract_archives`              | `TRUE` (extraction enabled)                 |
| `skip_existing_extractions`     | `TRUE` (skip if `ArchiveName.txt` matches)  |
| `force_extract`                 | `FALSE`                                     |
| `skip_download_if_extracted`    | `TRUE` (skip download if already extracted) |
| `force_download`                | `FALSE`                                     |
| `extract_path`                  | `NULL` (extract in place)                   |
| `delete_archives_after_extract` | `TRUE` (delete after extraction)            |
| `use_custom_icons`              | `TRUE`                                      |
| `unsnapshot_icons`              | `TRUE`                                      |
| `skip_aga`                      | `0` (disabled)                              |
| `skip_cd`                       | `0` (disabled)                              |
| `skip_ntsc`                     | `0` (disabled)                              |
| `skip_non_english`              | `0` (disabled)                              |
| `timeout_seconds`               | `30` seconds (clamped 5–60)                |

---

## Common Scenarios

### First-time full download

```text
whdfetch DOWNLOADALL
```

Downloads all packs, extracts every archive, deletes `.lha` files after extraction,
applies custom icons, and builds the `.archive_index` cache.

### Update run, typical ongoing use

```text
whdfetch DOWNLOADALL NOSKIPREPORT
```

Downloads only new or updated archives. Everything already extracted is skipped
automatically. Skip messages are suppressed for cleaner output.

### Download now, extract later

```text
whdfetch DOWNLOADGAMES NOEXTRACT KEEPARCHIVES
```

Downloads all Games `.lha` files but does not extract them. Archives are preserved on disk.
You can extract them later with:

```text
whdfetch DOWNLOADGAMES EXTRACTONLY
```

### Extract to an external drive

```text
whdfetch DOWNLOADGAMES EXTRACTTO=Games: KEEPARCHIVES
```

Downloads games, extracts them to the `Games:` volume, and keeps the `.lha` archives in
`GameFiles/Games/<letter>/` as a local cache.

### Re-extract everything from scratch

```text
whdfetch DOWNLOADGAMES FORCEEXTRACT
```

Use this when the archives are already present locally and you want to rebuild the
extracted folders without re-downloading the `.lha` files.

### Full clean rebuild

```text
whdfetch DOWNLOADALL FORCEDOWNLOAD FORCEEXTRACT
```

Use this when you want to ignore both local archive presence and prior extraction markers.
This is the nuclear option and will take considerable time and bandwidth.

### OCS-only English PAL setup

```text
whdfetch DOWNLOADGAMES SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

Downloads only games that should suit a base PAL OCS Amiga setup in English.

---

## Command Combinations to Avoid

### Conflicting archive-retention commands

```text
whdfetch DOWNLOADGAMES KEEPARCHIVES DELETEARCHIVES
```

These commands oppose each other. Only one should be used.

### Contradictory extraction mode commands

```text
whdfetch DOWNLOADGAMES EXTRACTONLY NOEXTRACT
```

`EXTRACTONLY` forces extraction, so `NOEXTRACT` is ignored.

### Semantically contradictory, though safe

```text
whdfetch DOWNLOADGAMES NOEXTRACT DELETEARCHIVES
```

`DELETEARCHIVES` has no effect because archive deletion only happens after extraction.
