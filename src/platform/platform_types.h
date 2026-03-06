#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

/* Platform-specific type definitions.
 * Provides type abstractions for file handles and other platform-dependent
 * types used throughout the project.
 */

#include "platform.h"

/*---------------------------------------------------------------------------*/
/* Platform-Specific Type Definitions                                        */
/*---------------------------------------------------------------------------*/

#if PLATFORM_AMIGA
    #include <dos/dos.h>
    typedef BPTR platform_bptr;
#else
    typedef void* platform_bptr;
#endif

#endif /* PLATFORM_TYPES_H */
