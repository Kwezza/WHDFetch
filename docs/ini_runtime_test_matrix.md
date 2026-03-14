# INI Runtime Test Matrix

Use this checklist to verify each INI key with one targeted runtime test.

## How to run
1. Copy one test key into `PROGDIR:WHDDownloader.ini` (and keep all other keys at defaults from `docs/WHDDownloader.ini.sample`).
2. Run `WHDDownloader LATESTDATONLY QUIET`.
3. Check `PROGDIR:logs/general_*.log` for the expected `config[after-ini]` or pipeline line.

## Global keys
- `download_website`
  - Test: set to known alternate mirror path.
  - Expected: `config[after-ini]: website='...'` matches override.
- `file_part_to_remove`
  - Test: set to default value and then a deliberately wrong value.
  - Expected: good value -> pack matches found; wrong value -> `html: no requested pack links matched` warning.
- `html_link_prefix_filter`
  - Test: set to `Commodore%20Amiga`.
  - Expected: `html: active filters prefix='Commodore%20Amiga'` and nonzero `prefix_match`.
- `html_link_content_filter`
  - Test: set to `WHDLoad`.
  - Expected: nonzero `content_match` and pack matches found.
- `link_cleanup_removals`
  - Test: set to `amp;,%20,CommodoreAmiga-,&amp;,&Unofficial,&Unreleased`.
  - Expected: `config[after-ini]: cleanup[n]='...'` lines for each value.

## Pack keys (use `[pack.games]` as target)
- `enabled`
  - Test: `enabled=false` and run with no `DOWNLOAD...` flags.
  - Expected: `config[after-ini]` shows `requested=0` for that pack.
- `display_name`
  - Test: set to `Games Test Name`.
  - Expected: `config[after-ini]: pack[...] name='Games Test Name'`.
- `download_url`
  - Test: set to an alternate valid URL path.
  - Expected: `config[after-ini]: pack[...] url='...'`.
- `local_dir`
  - Test: set to `Games Test/`.
  - Expected: `config[after-ini]: pack[...] dir='Games Test/'`.
- `filter_dat`
  - Test: set to `Games(`.
  - Expected: `config[after-ini]: pack[...] filter_dat='Games('` and normal matches.
- `filter_zip`
  - Test: set to `Games`.
  - Expected: `config[after-ini]: pack[...] filter_zip='Games'`.

## Filters keys
- `skip_aga`
  - Test: set `true`.
  - Expected: `config[after-ini]: ... skip_aga=1 ...`.
- `skip_cd`
  - Test: set `true`.
  - Expected: `config[after-ini]: ... skip_cd=1 ...`.
- `skip_ntsc`
  - Test: set `true`.
  - Expected: `config[after-ini]: ... skip_ntsc=1 ...`.
- `skip_non_english`
  - Test: set `true`.
  - Expected: `config[after-ini]: ... skip_non_english=1 ...`.

## CLI precedence check
- Test: set `skip_aga=false` in INI, then run `WHDDownloader LATESTDATONLY SKIPAGA QUIET`.
- Expected: `config[after-cli]: ... skip_aga=1 ...`.
