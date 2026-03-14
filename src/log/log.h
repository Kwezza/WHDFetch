#ifndef LOG_H
#define LOG_H

/* Multi-category logging system for amiga-base.
 *
 * Features:
 *   - Four generic categories with separate timestamped log files
 *   - Four log levels: DEBUG, INFO, WARNING, ERROR
 *   - Errors automatically duplicated to errors_*.log
 *   - Runtime enable/disable per category
 *   - All logs written to PROGDIR:logs/
 *
 * Adding your own categories:
 *   Replace LOG_APP with project-specific names and update the
 *   g_categoryNames[] array in log.c to match.
 */

#include <proto/dos.h>
#include <dos/dos.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <exec/types.h>

/*---------------------------------------------------------------------------*/
/* Log Categories                                                            */
/*---------------------------------------------------------------------------*/

typedef enum {
    LOG_GENERAL,    /* General program flow                                  */
    LOG_MEMORY,     /* Memory allocations/frees (high-frequency)             */
    LOG_APP,        /* Application-specific - rename/extend as needed        */
    LOG_DOWNLOAD,   /* HTTP download operations                              */
    LOG_PARSER,     /* DAT file / HTML parsing                               */
    LOG_ERRORS,     /* Errors only (also auto-copied from other categories)   */
    LOG_CATEGORY_COUNT  /* Keep last - used for array sizing                 */
} LogCategory;

/*---------------------------------------------------------------------------*/
/* Log Levels                                                                */
/*---------------------------------------------------------------------------*/

typedef enum {
    LOG_LEVEL_DEBUG,    /* Verbose debugging                                 */
    LOG_LEVEL_INFO,     /* Normal informational messages                     */
    LOG_LEVEL_WARNING,  /* Warning conditions                                */
    LOG_LEVEL_ERROR,    /* Error conditions                                  */
    LOG_LEVEL_DISABLED  /* Category fully silenced                           */
} LogLevel;

/*---------------------------------------------------------------------------*/
/* Core Functions                                                            */
/*---------------------------------------------------------------------------*/

/* Initialize the logging system (creates PROGDIR:logs/, generates timestamp) */
void initialize_log_system(BOOL cleanOldLogs);

/* Flush and close all log files */
void shutdown_log_system(void);

/* Return the timestamp string generated at initialize_log_system() time */
const char* get_log_timestamp(void);

/* Main logging function (use macros below in preference to calling directly) */
void log_message(LogCategory category, LogLevel level, const char *format, ...);

/* Convenience macros */
#define log_debug(cat, ...)   log_message(cat, LOG_LEVEL_DEBUG,   __VA_ARGS__)
#define log_info(cat, ...)    log_message(cat, LOG_LEVEL_INFO,     __VA_ARGS__)
#define log_warning(cat, ...) log_message(cat, LOG_LEVEL_WARNING,  __VA_ARGS__)
#define log_error(cat, ...)   log_message(cat, LOG_LEVEL_ERROR,    __VA_ARGS__)

/*---------------------------------------------------------------------------*/
/* Category Control                                                          */
/*---------------------------------------------------------------------------*/

void  enable_log_category(LogCategory category, BOOL enable);
BOOL  is_log_category_enabled(LogCategory category);
void  set_log_level(LogCategory category, LogLevel minLevel);

/*---------------------------------------------------------------------------*/
/* Global Level Control                                                      */
/*---------------------------------------------------------------------------*/

void     set_global_log_level(LogLevel minLevel);
LogLevel get_global_log_level(void);

/* Convenience helpers for the memory category */
void set_memory_logging_enabled(BOOL enabled);
BOOL is_memory_logging_enabled(void);

/*---------------------------------------------------------------------------*/
/* Utility                                                                   */
/*---------------------------------------------------------------------------*/

const char* get_log_category_name(LogCategory category);
const char* get_log_level_name(LogLevel level);

#endif /* LOG_H */
