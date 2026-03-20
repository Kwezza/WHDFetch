#ifndef LIFECYCLE_H
#define LIFECYCLE_H

#include "platform/platform.h"

BOOL lifecycle_confirm_purge_archives(void);
BOOL lifecycle_purge_downloaded_archives(void);
void lifecycle_do_shutdown(BOOL *download_lib_initialized);

#endif /* LIFECYCLE_H */
