# whdfetch — Retroplay WHDLoad Downloader for AmigaOS

`whdfetch` is a CLI tool for AmigaOS 3.0+ that automates downloading, extracting, and organising
[WHDLoad](http://www.whdload.de/) game, demo, and magazine packs from the
[Retroplay](https://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/) collection on the Turran FTP server.

After rebuilding my Amiga, one of the most time-consuming jobs was manually browsing the
Retroplay site, downloading every pack, extracting archives, and keeping everything
organised. `whdfetch` does all of that from a single Shell command. Select the packs you
want, apply filters for your machine's chipset and region, and let it run. On a subsequent
run it compares what is already installed against the latest pack listings and downloads
only what is new or updated.

> **Note:** This is a pure CLI tool. There is no GUI, no Workbench application, and no
> Intuition windows.

---

## Requirements

| Requirement | Notes |
|-------------|-------|
| AmigaOS 3.0+ | 3.1+ recommended |
| TCP/IP stack with `bsdsocket.library` | Roadshow recommended; WinUAE emulated stack also works |
| `c:lha` | Required for `.lha` archive extraction |
| `c:unzip` | Required for extracting the DAT ZIP files |
| `c:unlzx` | Optional — `.lzx` archives are skipped with a warning if not installed |
| Enough free hard drive space | The full collection is many gigabytes; a modern filesystem such as PFS is recommended |

Fast RAM is recommended. 

---

## How it works

A run follows a fixed sequence for each selected pack:

1. Downloads `index.html` from the Turran FTP site and scans it for pack links.
2. Downloads each matching ZIP file and extracts the XML DAT file inside it, which lists every archive in that pack with filenames, sizes, and CRC checksums.
3. Parses every `<rom name="...">` entry in the DAT and applies any active skip filters.
4. For each archive that passes the filters, checks whether it is already installed using a fast per-letter `.archive_index` cache. Archives not yet installed are downloaded directly from the FTP server into `GameFiles/<pack>/<letter>/`.
5. Extracts each downloaded archive with `c:lha` or `c:unlzx`, writes an `ArchiveName.txt` marker inside the extracted game folder, updates the index, and optionally replaces drawer icons.
6. At the end of the run, prints a per-pack summary to the Shell and saves a full session report to `PROGDIR:updates/`.

Running the same command again later checks only for what is new — already-extracted titles are skipped automatically.

---

## Quick start

Before committing to a large download, check how much disk space you will need:

```text
whdfetch ESTIMATESPACE
```

This downloads the latest DAT files, reads the archive sizes, and prints an estimate without downloading any game archives. It also accepts filter flags so the estimate reflects your actual setup:

```text
whdfetch ESTIMATESPACE SKIPAGA SKIPCD SKIPNTSC SKIPNONENGLISH
```

When you are ready to start, try the smallest pack first to verify that downloading, extracting, and folder creation all work correctly on your system:

```text
whdfetch DOWNLOADBETADEMOS EXTRACTTO=Work:WHDLoad/
```

`EXTRACTTO` redirects extracted files to a separate volume while keeping the downloaded archives in the `GameFiles` folder alongside `whdfetch`. Once you are satisfied, move on to the larger packs:

```text
whdfetch DOWNLOADGAMES EXTRACTTO=Work:WHDLoad/
```

The Games pack is the largest and can take many hours on real hardware — it is best run overnight.

---

## Available packs

| Command | Pack | Extracted to |
|---------|------|-------------|
| `DOWNLOADGAMES` | Games | `GameFiles/Games/` |
| `DOWNLOADBETAGAMES` | Games Beta & Unofficial | `GameFiles/Games Beta/` |
| `DOWNLOADDEMOS` | Demos | `GameFiles/Demos/` |
| `DOWNLOADBETADEMOS` | Demos Beta & Unofficial | `GameFiles/Demos Beta/` |
| `DOWNLOADMAGS` | Magazines | `GameFiles/Magazines/` |
| `DOWNLOADALL` | All five packs | — |

---

## Filters

If your machine cannot run certain types of title, apply filters to skip them at the DAT-parsing stage:

| Flag | Skips |
|------|-------|
| `SKIPAGA` | AGA-only titles (require A1200 or A4000) |
| `SKIPCD` | CD32, CDTV, and CDRom titles |
| `SKIPNTSC` | NTSC-only releases |
| `SKIPNONENGLISH` | Non-English or English-absent releases |

Filters can also be set permanently in `whdfetch.ini` so you do not need to type them every run.

---

## Key options

A few options that are good to know from the start:

| Option | Effect |
|--------|--------|
| `KEEPARCHIVES` | Keep `.lha`/`.lzx` files after extraction (default is to delete them) |
| `NOEXTRACT` | Download archives but do not extract them |
| `EXTRACTONLY` | Extract already-downloaded archives without downloading anything new |
| `FORCEEXTRACT` | Re-extract even if the title is already marked as installed |
| `FORCEDOWNLOAD` | Re-download even if the title is already extracted |
| `CRCCHECK` | Verify each downloaded archive against the CRC in the DAT file |
| `NOICONS` | Skip drawer icon replacement after extraction |

For the complete reference see [docs/CLI_Reference.md](docs/CLI_Reference.md).

---

## INI configuration

`whdfetch.ini`, placed alongside the binary, lets you set default packs, filters, paths,
website URLs, and other options so you do not need to repeat them on every run. CLI flags
always override INI values for the current run.

A fully annotated sample is provided at [docs/whdfetch.ini.sample](docs/whdfetch.ini.sample).

---

## Documentation

| File | Contents |
|------|----------|
| [Project page](https://madeforworkbench.com/products/whdfetch.html) | Lightweight project page for WHDFetch, written to stay usable on Amiga browsers including IBrowse setups |
| [HTML manual](https://madeforworkbench.com/docs/whdfetch-manual.html) | Lightweight online manual version for browser reading, including older Amiga browsing setups |
| [docs/Manual/manual.md](docs/Manual/manual.md) | Full user manual in Markdown form — getting started, behaviour, and options explained in prose |
| [docs/CLI_Reference.md](docs/CLI_Reference.md) | Complete command-line argument reference |
| [docs/whdfetch.ini.sample](docs/whdfetch.ini.sample) | Fully annotated sample INI configuration |
| [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md) | Internal architecture and data-flow overview |

---

## Building from source

The project targets AmigaOS 3.0+ / 68000+ and is compiled with
[VBCC](http://www.compilers.de/vbcc.html) on a Windows host using the NDK 3.2 and
Roadshow SDK.

```powershell
make                       # standard release build
make CONSOLE=1             # with console output (useful when debugging)
make CONSOLE=1 MEMTRACK=1  # with console output and allocation leak tracking
make release               # clean release build with refreshed build date
make refresh-version       # update the build date header without a full rebuild
make clean
```

Output binary: `Bin/Amiga/whdfetch`

---

## License

This project is for personal use. See repository settings for licence details.
