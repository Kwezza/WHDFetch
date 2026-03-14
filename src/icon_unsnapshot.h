/*
 * icon_unsnapshot.h - Standalone helper to clear Workbench icon snapshot position
 *
 * Usage:
 *   - Call strip_icon_position() with a file path or .info path.
 *   - The function sets icon X/Y to NO_ICON_POSITION and writes the icon back.
 */

#ifndef ICON_UNSNAPSHOT_H
#define ICON_UNSNAPSHOT_H

#include <exec/types.h>

#if defined(__AMIGA__) || defined(PLATFORM_AMIGA)
	#include <workbench/workbench.h>
#endif

/*
 * Workbench uses NO_ICON_POSITION to mean "no saved icon position".
 * Some SDK setups expose it already; keep a fallback for portability.
 */
#ifndef NO_ICON_POSITION
#define NO_ICON_POSITION ((LONG)0x80000000)
#endif

/*
 * Clear an icon's saved position (unsnapshot it).
 *
 * Parameters:
 *   icon_path - Path to the object ("Work:Foo") or icon file ("Work:Foo.info").
 *
 * Returns:
 *   TRUE  - icon loaded and saved with X/Y set to NO_ICON_POSITION.
 *   FALSE - invalid path, icon missing, or write/save failure.
 */
BOOL strip_icon_position(const char *icon_path);

#endif /* ICON_UNSNAPSHOT_H */
