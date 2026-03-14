/* platform.c - Memory tracking system for amiga-base
 *
 * Provides tracked versions of malloc/free that:
 *   - Use AmigaOS AllocVec/FreeVec (prefers Fast RAM for 15x perf on expanded systems)
 *   - Record every allocation with file + line info
 *   - Report leaks at program exit via amiga_memory_report()
 *   - Handle OOM via a pre-allocated emergency buffer + EasyRequest dialog
 *
 * Only compiled when MEMTRACK=1 is passed to make (sets -DDEBUG_MEMORY_TRACKING).
 * In release builds this entire file contributes zero code.
 */

#include "platform.h"

#ifdef DEBUG_MEMORY_TRACKING

#include "../log/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(__AMIGA__)
    #include <exec/types.h>
    #include <exec/memory.h>
    #include <proto/exec.h>
    #include <proto/dos.h>
    #include <intuition/intuition.h>
    #include <proto/intuition.h>
#endif

/*---------------------------------------------------------------------------*/
/* Memory Tracking Data Structures                                           */
/*---------------------------------------------------------------------------*/

typedef struct MemoryBlock {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct MemoryBlock *next;
} MemoryBlock;

static MemoryBlock *g_memoryList       = NULL;
static size_t g_totalAllocated    = 0;
static size_t g_totalFreed        = 0;
static size_t g_currentAllocated  = 0;
static size_t g_peakAllocated     = 0;
static size_t g_allocationCount   = 0;
static size_t g_freeCount         = 0;
static int    g_loggingSuspended  = 0;

/* Emergency shutdown resources (pre-allocated at init, never freed) */
static char  *g_emergencyBuffer     = NULL;
static BPTR   g_emergencyLogFile    = 0;
static int    g_emergencyInitialized = 0;

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

void amiga_memory_init(void) {
    g_memoryList       = NULL;
    g_totalAllocated   = 0;
    g_totalFreed       = 0;
    g_currentAllocated = 0;
    g_peakAllocated    = 0;
    g_allocationCount  = 0;
    g_freeCount        = 0;
    g_loggingSuspended = 0;

    log_info(LOG_MEMORY, "Memory tracking initialized\n");

    if (!g_emergencyInitialized) {
#if defined(__AMIGA__)
        /* 1 KB in Chip RAM - always available even when Fast RAM is exhausted */
        g_emergencyBuffer  = (char *)AllocVec(1024, MEMF_CHIP);
        g_emergencyLogFile = Open("RAM:CRITICAL_FAILURE.log", MODE_NEWFILE);

        if (g_emergencyBuffer && g_emergencyLogFile) {
            g_emergencyInitialized = 1;
            log_info(LOG_MEMORY, "Emergency shutdown system ready (1KB Chip RAM + RAM: log)\n");
        } else {
            log_warning(LOG_MEMORY, "Emergency shutdown system FAILED to initialize\n");
            if (g_emergencyBuffer)  { FreeVec(g_emergencyBuffer); g_emergencyBuffer = NULL; }
            if (g_emergencyLogFile) { Close(g_emergencyLogFile);  g_emergencyLogFile = 0; }
        }
#else
        g_emergencyBuffer = (char *)malloc(1024);
        if (g_emergencyBuffer) g_emergencyInitialized = 1;
#endif
    }
}

void amiga_memory_suspend_logging(void) { g_loggingSuspended = 1; }
void amiga_memory_resume_logging(void)  { g_loggingSuspended = 0; }

/*---------------------------------------------------------------------------*/
/* Allocation                                                                */
/*---------------------------------------------------------------------------*/

void* amiga_malloc_debug(size_t size, const char *file, int line) {
    void *ptr;
    MemoryBlock *block;

#if defined(__AMIGA__)
    ptr = AllocVec(size, MEMF_ANY | MEMF_CLEAR);
#else
    ptr = malloc(size);
#endif

    if (!ptr) {
        if (g_emergencyInitialized) {
            char msg[256];
            sprintf(msg, "OUT OF MEMORY at %s:%d (requested %lu bytes)",
                    file, line, (unsigned long)size);
            amiga_memory_emergency_shutdown(msg);
            /* does not return */
        }
        log_error(LOG_MEMORY, "malloc(%lu) FAILED at %s:%d\n",
                  (unsigned long)size, file, line);
        return NULL;
    }

#if defined(__AMIGA__)
    block = (MemoryBlock *)AllocVec(sizeof(MemoryBlock), MEMF_ANY | MEMF_CLEAR);
#else
    block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
#endif

    if (!block) {
        log_warning(LOG_MEMORY,
                    "Failed to allocate tracking block for %lu bytes at %s:%d\n",
                    (unsigned long)size, file, line);
        return ptr;
    }

    block->ptr  = ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->next = g_memoryList;
    g_memoryList = block;

    g_totalAllocated   += size;
    g_currentAllocated += size;
    g_allocationCount++;

    if (g_currentAllocated > g_peakAllocated)
        g_peakAllocated = g_currentAllocated;

    if (!g_loggingSuspended) {
        log_debug(LOG_MEMORY,
                  "ALLOC: %lu bytes at 0x%08lx (%s:%d) [cur:%lu peak:%lu]\n",
                  (unsigned long)size, (unsigned long)ptr, file, line,
                  (unsigned long)g_currentAllocated, (unsigned long)g_peakAllocated);
    }

    return ptr;
}

/*---------------------------------------------------------------------------*/
/* Deallocation                                                              */
/*---------------------------------------------------------------------------*/

void amiga_free_debug(void *ptr, const char *file, int line) {
    MemoryBlock *block, *prev;

    if (!ptr) {
        if (!g_loggingSuspended)
            log_debug(LOG_MEMORY, "FREE: NULL at %s:%d (ignored)\n", file, line);
        return;
    }

    prev  = NULL;
    block = g_memoryList;
    while (block) {
        if (block->ptr == ptr) {
            if (prev)
                prev->next = block->next;
            else
                g_memoryList = block->next;

            g_totalFreed       += block->size;
            g_currentAllocated -= block->size;
            g_freeCount++;

            if (!g_loggingSuspended) {
                log_debug(LOG_MEMORY,
                          "FREE: %lu bytes at 0x%08lx (alloc'd %s:%d, freed %s:%d) [cur:%lu]\n",
                          (unsigned long)block->size, (unsigned long)ptr,
                          block->file, block->line, file, line,
                          (unsigned long)g_currentAllocated);
            }

#if defined(__AMIGA__)
            FreeVec(block);
            FreeVec(ptr);
#else
            free(block);
            free(ptr);
#endif
            return;
        }
        prev  = block;
        block = block->next;
    }

    /* Pointer was not in the tracking list */
    if (!g_loggingSuspended) {
        log_error(LOG_MEMORY, "FREE: UNTRACKED pointer 0x%08lx at %s:%d\n",
                  (unsigned long)ptr, file, line);
    }

#if defined(__AMIGA__)
    FreeVec(ptr);  /* AllocVec/FreeVec must stay paired on Amiga */
#else
    free(ptr);     /* still free to avoid leak */
#endif
}

/*---------------------------------------------------------------------------*/
/* String duplication                                                        */
/*---------------------------------------------------------------------------*/

char* amiga_strdup_debug(const char *str, const char *file, int line) {
    char *copy;
    size_t len;

    if (!str) {
        log_debug(LOG_MEMORY, "STRDUP: NULL at %s:%d\n", file, line);
        return NULL;
    }

    len  = strlen(str) + 1;
    copy = (char *)amiga_malloc_debug(len, file, line);
    if (copy) strcpy(copy, str);
    return copy;
}

/*---------------------------------------------------------------------------*/
/* Leak Report                                                               */
/*---------------------------------------------------------------------------*/

void amiga_memory_report(void) {
    MemoryBlock *block;
    size_t leakCount = 0, leakBytes = 0;

    log_info(LOG_MEMORY, "========== MEMORY TRACKING REPORT ==========\n");
    log_info(LOG_MEMORY, "Allocations : %lu  (%lu bytes)\n",
             (unsigned long)g_allocationCount, (unsigned long)g_totalAllocated);
    log_info(LOG_MEMORY, "Frees       : %lu  (%lu bytes)\n",
             (unsigned long)g_freeCount, (unsigned long)g_totalFreed);
    log_info(LOG_MEMORY, "Peak usage  : %lu bytes (%lu KB)\n",
             (unsigned long)g_peakAllocated, (unsigned long)(g_peakAllocated / 1024));
    log_info(LOG_MEMORY, "Current     : %lu bytes\n", (unsigned long)g_currentAllocated);

    block = g_memoryList;
    while (block) { leakCount++; leakBytes += block->size; block = block->next; }

    if (leakCount > 0) {
        log_error(LOG_MEMORY, "*** LEAKS: %lu blocks, %lu bytes ***\n",
                  (unsigned long)leakCount, (unsigned long)leakBytes);
        block = g_memoryList;
        while (block) {
            log_error(LOG_MEMORY, "  - %lu bytes at 0x%08lx  (%s:%d)\n",
                      (unsigned long)block->size, (unsigned long)block->ptr,
                      block->file, block->line);
            block = block->next;
        }
    } else {
        log_info(LOG_MEMORY, "*** NO LEAKS - all allocations freed ***\n");
    }

    log_info(LOG_MEMORY, "=============================================\n");
}

/*---------------------------------------------------------------------------*/
/* Emergency OOM Handler                                                     */
/*---------------------------------------------------------------------------*/

void amiga_memory_emergency_shutdown(const char *reason) {
#if defined(__AMIGA__)
    struct Library *IntuitionBase = NULL;

    struct EasyStruct criticalRequest = {
        sizeof(struct EasyStruct),
        0,
        "CRITICAL MEMORY FAILURE",
        AMIGA_APP_NAME " has run out of memory!\n\n"
        "%s\n\n"
        "The program will now exit.\n\n"
        "Check RAM:CRITICAL_FAILURE.log for details.",
        "Exit"
    };

    if (g_emergencyBuffer && g_emergencyLogFile) {
        sprintf(g_emergencyBuffer,
                "========== CRITICAL MEMORY FAILURE ==========\n"
                "%s\n"
                "Allocated: %lu  Freed: %lu  Current: %lu  Peak: %lu\n"
                "Alloc count: %lu  Free count: %lu\n"
                "=============================================\n",
                reason,
                (unsigned long)g_totalAllocated,
                (unsigned long)g_totalFreed,
                (unsigned long)g_currentAllocated,
                (unsigned long)g_peakAllocated,
                (unsigned long)g_allocationCount,
                (unsigned long)g_freeCount);

        Write(g_emergencyLogFile, g_emergencyBuffer, strlen(g_emergencyBuffer));
        Flush(g_emergencyLogFile);
        Close(g_emergencyLogFile);
        g_emergencyLogFile = 0;
    }

    IntuitionBase = OpenLibrary("intuition.library", 0);
    if (IntuitionBase) {
        EasyRequest(NULL, &criticalRequest, NULL, (APTR)reason);
        CloseLibrary(IntuitionBase);
    }

    exit(20);  /* RETURN_FAIL */

#else
    if (g_emergencyBuffer) {
        fprintf(stderr, "\n*** CRITICAL MEMORY FAILURE ***\n%s\n", reason);
        fprintf(stderr, "Allocated:%lu  Freed:%lu  Current:%lu\n",
                (unsigned long)g_totalAllocated,
                (unsigned long)g_totalFreed,
                (unsigned long)g_currentAllocated);
    }
    exit(1);
#endif
}

#endif /* DEBUG_MEMORY_TRACKING */
