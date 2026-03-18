#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Platform Selection                                                        */
/*---------------------------------------------------------------------------*/

#define PLATFORM_AMIGA 1

/*---------------------------------------------------------------------------*/
/* Application Name (used in OOM error dialogs)                             */
/* Override in Makefile: -DAMIGA_APP_NAME=\"MyApp\"                         */
/*---------------------------------------------------------------------------*/

/* App name used in window titles and OOM dialogs.
 * VBCC on Windows cannot receive string literals via -D flags, so
 * change this define directly when starting a new project.          */
#ifndef AMIGA_APP_NAME
    #define AMIGA_APP_NAME "Retroplay WHD Downloader"
#endif

/*---------------------------------------------------------------------------*/
/* Memory Allocation Abstraction with Optional Tracking                     */
/*                                                                           */
/* Enable memory debugging by passing MEMTRACK=1 to make:                   */
/*   make MEMTRACK=1                                                         */
/* Do NOT hardcode DEBUG_MEMORY_TRACKING here - it creates                  */
/* RAM:CRITICAL_FAILURE.log on every run.                                   */
/*---------------------------------------------------------------------------*/

#ifdef DEBUG_MEMORY_TRACKING
    /* Memory tracking functions - implemented in platform.c */
    void* amiga_malloc_debug(size_t size, const char *file, int line);
    void  amiga_free_debug(void *ptr, const char *file, int line);
    char* amiga_strdup_debug(const char *str, const char *file, int line);
    void  amiga_memory_report(void);
    void  amiga_memory_init(void);
    void  amiga_memory_suspend_logging(void);
    void  amiga_memory_resume_logging(void);
    void  amiga_memory_emergency_shutdown(const char *reason);

    #define amiga_malloc(sz)  amiga_malloc_debug(sz, __FILE__, __LINE__)
    #define amiga_free(p)     amiga_free_debug(p, __FILE__, __LINE__)
    #define amiga_strdup(s)   amiga_strdup_debug(s, __FILE__, __LINE__)
#else
    /* Standard allocation without tracking */
    #define amiga_malloc(sz)  malloc(sz)
    #define amiga_free(p)     free(p)

    static inline char* amiga_strdup(const char *str) {
        char *copy;
        if (!str) return NULL;
        copy = (char *)malloc(strlen(str) + 1);
        if (copy) strcpy(copy, str);
        return copy;
    }

    /* No-op stubs when tracking is disabled.
     * do{}while(0) avoids VBCC warning 153 "statement has no effect" */
    #define amiga_memory_report()  do {} while (0)
    #define amiga_memory_init()    do {} while (0)
#endif

/*---------------------------------------------------------------------------*/
/* Platform-Specific Includes                                                */
/*---------------------------------------------------------------------------*/

#if PLATFORM_AMIGA
    /* Amiga-specific headers are included by modules that need them.        */
    /* See amiga_headers.h for the full set.                                 */
#endif

#endif /* PLATFORM_H */
