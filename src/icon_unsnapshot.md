# icon_unsnapshot - Standalone Module

## Purpose
`strip_icon_position()` clears an icon's saved Workbench position by setting:
- `do_CurrentX = NO_ICON_POSITION`
- `do_CurrentY = NO_ICON_POSITION`

Workbench will then treat the icon as unsnapshotted.

## Files
- `support files/icon_unsnapshot.h`
- `support files/icon_unsnapshot.c`

## Public API
```c
BOOL strip_icon_position(const char *icon_path);
```

## Input
Pass either format:
- Object path: `"Work:Projects/MyDrawer"`
- Icon path: `"Work:Projects/MyDrawer.info"`

The function normalizes the path internally.

## Return Value
- `TRUE`: Icon was loaded and saved with both coordinates set to `NO_ICON_POSITION`.
- `FALSE`: Invalid path, icon missing, read-only change failed, or save failed.

## Read-only / Write-protected Handling
If the icon file is write-protected:
1. The function reads current protection bits.
2. Temporarily clears write protection.
3. Writes the updated icon.
4. Restores original protection bits.

If any step fails, the function returns `FALSE`.

## Minimal Example
```c
#include "icon_unsnapshot.h"

BOOL ok;

ok = strip_icon_position("Work:Projects/MyDrawer");
if (!ok)
{
    /* handle error */
}

ok = strip_icon_position("Work:Projects/MyTool.info");
if (!ok)
{
    /* handle error */
}
```

## Notes
- Includes a fallback definition of `NO_ICON_POSITION` as `0x80000000` if not provided by your SDK.
- Uses only standard AmigaOS APIs (`dos.library`, `icon.library`) and no iTidy-specific dependencies.
