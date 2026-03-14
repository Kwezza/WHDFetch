/* log.c - Multi-category logging system for amiga-base
 *
 * Each category writes to its own timestamped file under PROGDIR:logs/.
 * Errors are automatically duplicated to errors_*.log.
 * Uses AmigaDOS file I/O (BPTR, Open/Write/Close) exclusively.
 *
 * Adapted from iTidy writeLog.c:
 *   - Removed dependency on file_directory_handling.c (dir creation inlined)
 *   - Removed TimerBase / performance timing (timer.device not in base skeleton)
 *   - Categories simplified to GENERAL, MEMORY, APP, ERRORS
 */

#include "platform/platform.h"
#include "platform/amiga_headers.h"  /* AllocVec, FreeVec, MEMF_CLEAR, AnchorPath */
#include "console_output.h"
#include "log/log.h"

#include <proto/dos.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/*---------------------------------------------------------------------------*/
/* Log System State                                                          */
/*---------------------------------------------------------------------------*/

typedef struct {
    char    filename[256];
    BPTR    fileHandle;
    BOOL    enabled;
    LogLevel minLevel;
    ULONG   messageCount;
    ULONG   bytesWritten;
} LogCategoryState;

static LogCategoryState g_logCategories[LOG_CATEGORY_COUNT];
static char  g_logTimestamp[32]    = {0};
static char  g_logsDirectory[256]  = {0};
static BOOL  g_logSystemInitialized = FALSE;

static LogLevel g_globalLogLevel        = LOG_LEVEL_DISABLED;
static BOOL     g_memoryLoggingEnabled  = FALSE;

/*---------------------------------------------------------------------------*/
/* Category / Level Name Tables                                              */
/* IMPORTANT: order must match the LogCategory enum in log.h                */
/*---------------------------------------------------------------------------*/

static const char* g_categoryNames[] = {
    "general",
    "memory",
    "app",
    "download",
    "parser",
    "errors"
};

static const char* g_levelNames[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "DISABLED"
};

const char* get_log_category_name(LogCategory category) {
    if (category >= 0 && category < LOG_CATEGORY_COUNT)
        return g_categoryNames[category];
    return "unknown";
}

const char* get_log_level_name(LogLevel level) {
    if (level >= 0 && level <= LOG_LEVEL_DISABLED)
        return g_levelNames[level];
    return "UNKNOWN";
}

/*---------------------------------------------------------------------------*/
/* Timestamp Helpers                                                         */
/*---------------------------------------------------------------------------*/

static void generate_timestamp_string(char *buf, int bufSize) {
    time_t rawtime;
    struct tm *t;

    time(&rawtime);
    t = localtime(&rawtime);
    if (t)
        snprintf(buf, bufSize, "%04d-%02d-%02d_%02d-%02d-%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
    else {
        strncpy(buf, "unknown_time", bufSize - 1);
        buf[bufSize - 1] = '\0';
    }
}

static void get_entry_timestamp(char *buf, int bufSize) {
    time_t rawtime;
    struct tm *t;

    time(&rawtime);
    t = localtime(&rawtime);
    if (t)
        snprintf(buf, bufSize, "[%02d:%02d:%02d]",
                 t->tm_hour, t->tm_min, t->tm_sec);
    else {
        strncpy(buf, "[00:00:00]", bufSize - 1);
        buf[bufSize - 1] = '\0';
    }
}

const char* get_log_timestamp(void) {
    return g_logTimestamp;
}

/*---------------------------------------------------------------------------*/
/* Directory Management (inlined - no external dependency)                  */
/*---------------------------------------------------------------------------*/

static BOOL ensure_logs_directory(void) {
    BPTR lock;
    BPTR parentLock;
    char pathWithoutSlash[256];
    size_t len;

    /* Check if it already exists */
    lock = Lock(g_logsDirectory, ACCESS_READ);
    if (lock) { UnLock(lock); return TRUE; }

    /* Strip trailing slash before CreateDir() */
    strncpy(pathWithoutSlash, g_logsDirectory, sizeof(pathWithoutSlash) - 1);
    pathWithoutSlash[sizeof(pathWithoutSlash) - 1] = '\0';
    len = strlen(pathWithoutSlash);
    if (len > 0 && pathWithoutSlash[len - 1] == '/')
        pathWithoutSlash[len - 1] = '\0';

    /* Method 1: direct CreateDir */
    lock = CreateDir((CONST_STRPTR)pathWithoutSlash);
    if (lock) { UnLock(lock); return TRUE; }

    /* Method 2: switch to PROGDIR: then create "logs" */
    parentLock = Lock("PROGDIR:", ACCESS_READ);
    if (parentLock) {
        BPTR oldDir = CurrentDir(parentLock);
        lock = CreateDir("logs");
        if (lock) {
            UnLock(lock);
            CurrentDir(oldDir);
            UnLock(parentLock);
            return TRUE;
        }
        CurrentDir(oldDir);
        UnLock(parentLock);
    }

    CONSOLE_WARNING("WARNING: Could not create logs directory '%s' - "
                    "falling back to PROGDIR:\n", g_logsDirectory);
    return FALSE;
}

/* Delete all files under the logs directory (used when cleanOldLogs == TRUE) */
static void clean_logs_directory(void) {
    struct AnchorPath *anchor;
    LONG result;
    char pattern[300];

    snprintf(pattern, sizeof(pattern), "%s#?", g_logsDirectory);

    anchor = (struct AnchorPath *)AllocVec(sizeof(struct AnchorPath) + 512, MEMF_CLEAR);
    if (!anchor) return;

    anchor->ap_BreakBits = 0;
    anchor->ap_Strlen    = 0;

    result = MatchFirst(pattern, anchor);
    while (result == 0) {
        if (anchor->ap_Info.fib_DirEntryType < 0) {
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s%s",
                     g_logsDirectory, anchor->ap_Info.fib_FileName);
            DeleteFile(fullPath);
        }
        result = MatchNext(anchor);
    }
    MatchEnd(anchor);
    FreeVec(anchor);
}

/*---------------------------------------------------------------------------*/
/* Log System Initialization / Shutdown                                     */
/*---------------------------------------------------------------------------*/

void initialize_log_system(BOOL cleanOldLogs) {
    int i;

    generate_timestamp_string(g_logTimestamp, sizeof(g_logTimestamp));
    snprintf(g_logsDirectory, sizeof(g_logsDirectory), "PROGDIR:logs/");

    CONSOLE_STATUS("Log system init: '%s'\n", g_logsDirectory);

    if (!ensure_logs_directory()) {
        strncpy(g_logsDirectory, "PROGDIR:", sizeof(g_logsDirectory));
        g_logsDirectory[sizeof(g_logsDirectory) - 1] = '\0';
        CONSOLE_WARNING("Log system: falling back to PROGDIR:\n");
    }

    if (cleanOldLogs)
        clean_logs_directory();

    for (i = 0; i < LOG_CATEGORY_COUNT; i++) {
        snprintf(g_logCategories[i].filename,
                 sizeof(g_logCategories[i].filename),
                 "%s%s_%s.log",
                 g_logsDirectory, g_categoryNames[i], g_logTimestamp);

        g_logCategories[i].fileHandle   = 0;
        g_logCategories[i].enabled      = TRUE;
        g_logCategories[i].minLevel     = LOG_LEVEL_DEBUG;
        g_logCategories[i].messageCount = 0;
        g_logCategories[i].bytesWritten = 0;
    }

    g_logSystemInitialized = TRUE;

    log_info(LOG_GENERAL, "Log system initialized - timestamp: %s\n", g_logTimestamp);
    log_info(LOG_GENERAL, "Logs directory: %s\n", g_logsDirectory);
}

void shutdown_log_system(void) {
    int i;

    if (!g_logSystemInitialized) return;

    log_info(LOG_GENERAL, "========== LOG SYSTEM SHUTDOWN ==========\n");
    for (i = 0; i < LOG_CATEGORY_COUNT; i++) {
        if (g_logCategories[i].messageCount > 0) {
            log_info(LOG_GENERAL, "  %s: %lu messages, %lu bytes\n",
                     g_categoryNames[i],
                     (unsigned long)g_logCategories[i].messageCount,
                     (unsigned long)g_logCategories[i].bytesWritten);
        }
    }
    log_info(LOG_GENERAL, "=========================================\n");

    for (i = 0; i < LOG_CATEGORY_COUNT; i++) {
        if (g_logCategories[i].fileHandle) {
            Close(g_logCategories[i].fileHandle);
            g_logCategories[i].fileHandle = 0;
        }
    }

    g_logSystemInitialized = FALSE;
}

/*---------------------------------------------------------------------------*/
/* Category / Level Control                                                  */
/*---------------------------------------------------------------------------*/

void enable_log_category(LogCategory category, BOOL enable) {
    if (category >= 0 && category < LOG_CATEGORY_COUNT)
        g_logCategories[category].enabled = enable;
}

BOOL is_log_category_enabled(LogCategory category) {
    if (category >= 0 && category < LOG_CATEGORY_COUNT)
        return g_logCategories[category].enabled;
    return FALSE;
}

void set_log_level(LogCategory category, LogLevel minLevel) {
    if (category >= 0 && category < LOG_CATEGORY_COUNT)
        g_logCategories[category].minLevel = minLevel;
}

void set_global_log_level(LogLevel minLevel) {
    int i;
    g_globalLogLevel = minLevel;
    for (i = 0; i < LOG_CATEGORY_COUNT; i++)
        g_logCategories[i].minLevel = minLevel;
}

LogLevel get_global_log_level(void) {
    return g_globalLogLevel;
}

void set_memory_logging_enabled(BOOL enabled) {
    g_memoryLoggingEnabled = enabled;
    enable_log_category(LOG_MEMORY, enabled);
    if (enabled)
        set_log_level(LOG_MEMORY, LOG_LEVEL_DEBUG);  /* allocs use log_debug */
}

BOOL is_memory_logging_enabled(void) {
    return g_memoryLoggingEnabled;
}

/*---------------------------------------------------------------------------*/
/* Core Logging Function                                                     */
/*---------------------------------------------------------------------------*/

void log_message(LogCategory category, LogLevel level, const char *format, ...) {
    char     buffer[1024];
    char     timestamp[16];
    char     levelPrefix[48];
    va_list  args;
    LONG     written;
    BPTR     logFile;
    char    *src, *dst;

    if (!g_logSystemInitialized)          return;
    if (category < 0 || category >= LOG_CATEGORY_COUNT) return;
    if (!g_logCategories[category].enabled) return;
    if (level < g_logCategories[category].minLevel) return;

    get_entry_timestamp(timestamp, sizeof(timestamp));
    snprintf(levelPrefix, sizeof(levelPrefix), "%s[%s] ",
             timestamp, get_log_level_name(level));

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* Collapse newlines to spaces for single-line log entries */
    src = dst = buffer;
    while (*src) {
        if (*src == '\n' || *src == '\r') {
            if (dst > buffer && *(dst - 1) != ' ')
                *dst++ = ' ';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    ensure_logs_directory();

    logFile = Open(g_logCategories[category].filename, MODE_READWRITE);
    if (!logFile)
        logFile = Open(g_logCategories[category].filename, MODE_NEWFILE);

    if (logFile) {
        Seek(logFile, 0, OFFSET_END);

        written = Write(logFile, levelPrefix, strlen(levelPrefix));
        if (written > 0) g_logCategories[category].bytesWritten += written;

        written = Write(logFile, buffer, strlen(buffer));
        if (written > 0) g_logCategories[category].bytesWritten += written;

        Write(logFile, "\n", 1);
        Flush(logFile);
        Close(logFile);

        g_logCategories[category].messageCount++;
    }

    /* Duplicate errors to the errors log (avoid self-recursion) */
    if (level == LOG_LEVEL_ERROR && category != LOG_ERRORS) {
        char errorPrefix[64];

        logFile = Open(g_logCategories[LOG_ERRORS].filename, MODE_READWRITE);
        if (!logFile)
            logFile = Open(g_logCategories[LOG_ERRORS].filename, MODE_NEWFILE);

        if (logFile) {
            snprintf(errorPrefix, sizeof(errorPrefix), "%s[ERROR][%s] ",
                     timestamp, get_log_category_name(category));
            Seek(logFile, 0, OFFSET_END);
            Write(logFile, errorPrefix, strlen(errorPrefix));
            Write(logFile, buffer, strlen(buffer));
            Write(logFile, "\n", 1);
            Flush(logFile);
            Close(logFile);
            g_logCategories[LOG_ERRORS].messageCount++;
        }
    }
}
