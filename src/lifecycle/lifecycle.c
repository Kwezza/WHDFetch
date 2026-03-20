#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>

#include "download/amiga_download.h"
#include "extract/extract.h"
#include "ini_parser.h"
#include "lifecycle/lifecycle.h"
#include "log/log.h"
#include "report/report.h"
#include "utilities.h"

static BOOL purge_archive_files_recursive(const char *directory, ULONG *deleted_count)
{
    BPTR directory_lock;
    struct FileInfoBlock *fib;
    char entry_path[512] = {0};
    BOOL success = TRUE;

    if (directory == NULL)
    {
        return FALSE;
    }

    directory_lock = Lock(directory, ACCESS_READ);
    if (directory_lock == 0)
    {
        return TRUE;
    }

    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib == NULL)
    {
        UnLock(directory_lock);
        return FALSE;
    }

    if (!Examine(directory_lock, fib))
    {
        FreeDosObject(DOS_FIB, fib);
        UnLock(directory_lock);
        return FALSE;
    }

    while (ExNext(directory_lock, fib))
    {
        if (fib->fib_DirEntryType > 0)
        {
            if (snprintf(entry_path, sizeof(entry_path), "%s/%s", directory, fib->fib_FileName) >= (int)sizeof(entry_path))
            {
                log_warning(LOG_GENERAL,
                            "purge: skipped directory with path too long under '%s' ('%s')\n",
                            directory,
                            fib->fib_FileName);
                success = FALSE;
                continue;
            }

            sanitize_amiga_file_path(entry_path);
            if (!purge_archive_files_recursive(entry_path, deleted_count))
            {
                success = FALSE;
            }
        }
        else if (fib->fib_DirEntryType < 0 && detect_archive_type_from_filename(fib->fib_FileName) != ARCHIVE_TYPE_UNKNOWN)
        {
            if (snprintf(entry_path, sizeof(entry_path), "%s/%s", directory, fib->fib_FileName) >= (int)sizeof(entry_path))
            {
                log_warning(LOG_GENERAL,
                            "purge: skipped archive with path too long under '%s' ('%s')\n",
                            directory,
                            fib->fib_FileName);
                success = FALSE;
                continue;
            }

            sanitize_amiga_file_path(entry_path);
            if (DeleteFile(entry_path))
            {
                if (deleted_count != NULL)
                {
                    *deleted_count = *deleted_count + 1;
                }
                log_info(LOG_GENERAL, "purge: deleted archive '%s'\n", entry_path);
            }
            else if (IoErr() != ERROR_OBJECT_NOT_FOUND)
            {
                log_warning(LOG_GENERAL,
                            "purge: failed to delete archive '%s' (IoErr=%ld)\n",
                            entry_path,
                            (long)IoErr());
                success = FALSE;
            }
        }
    }

    if (IoErr() != ERROR_NO_MORE_ENTRIES)
    {
        log_warning(LOG_GENERAL,
                    "purge: directory scan ended with IoErr=%ld for '%s'\n",
                    (long)IoErr(),
                    directory);
        success = FALSE;
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(directory_lock);
    return success;
}

BOOL lifecycle_confirm_purge_archives(void)
{
    char response[16] = {0};

    fprintf(stderr, "\nPURGEARCHIVES will permanently delete downloaded .lha/.lzx archives under GameFiles/.\n");
    fprintf(stderr, "Extracted game folders will be preserved.\n");
    fprintf(stderr, "Type Y to continue: ");
    fflush(stderr);

    if (fgets(response, sizeof(response), stdin) == NULL)
    {
        fprintf(stderr, "\nPurge cancelled.\n");
        return FALSE;
    }

    if (toupper((unsigned char)response[0]) != 'Y')
    {
        fprintf(stderr, "\nPurge cancelled.\n");
        return FALSE;
    }

    return TRUE;
}

BOOL lifecycle_purge_downloaded_archives(void)
{
    ULONG deleted_count = 0;
    const char *archive_root_directory = "GameFiles";

    if (!does_file_or_folder_exist(archive_root_directory, 0))
    {
        fprintf(stderr, "GameFiles/ does not exist; nothing to purge.\n");
        log_info(LOG_GENERAL, "purge: GameFiles directory missing; nothing to delete\n");
        return TRUE;
    }

    if (!purge_archive_files_recursive(archive_root_directory, &deleted_count))
    {
        fprintf(stderr, "Purge completed with errors. Check the log for details.\n");
        log_error(LOG_GENERAL, "purge: archive purge completed with errors\n");
        return FALSE;
    }

    fprintf(stderr, "Purged archive files under %s/.\n", archive_root_directory);
    log_info(LOG_GENERAL,
             "purge: purged archive files under '%s'\n",
             archive_root_directory);
    return TRUE;
}

void lifecycle_do_shutdown(BOOL *download_lib_initialized)
{
    log_info(LOG_GENERAL, "do_shutdown: entered\n");

    if (download_lib_initialized != NULL && *download_lib_initialized)
    {
        log_info(LOG_GENERAL, "do_shutdown: calling ad_cleanup_download_lib...\n");
        ad_cleanup_download_lib();
        *download_lib_initialized = FALSE;
        log_info(LOG_GENERAL, "do_shutdown: ad_cleanup_download_lib complete\n");
    }
    else
    {
        log_info(LOG_GENERAL, "do_shutdown: download lib was not initialized, skipping cleanup\n");
    }

    log_info(LOG_GENERAL, "do_shutdown: clearing session report state...\n");
    report_cleanup();
    log_info(LOG_GENERAL, "do_shutdown: session report state cleared\n");

    log_info(LOG_GENERAL, "do_shutdown: flushing extract index cache...\n");
    extract_index_flush();
    log_info(LOG_GENERAL, "do_shutdown: extract index cache flush complete\n");

    log_info(LOG_GENERAL, "do_shutdown: freeing INI override allocations...\n");
    ini_parser_cleanup_overrides();

    log_info(LOG_GENERAL, "do_shutdown: calling amiga_memory_report...\n");
    amiga_memory_report();
    log_info(LOG_GENERAL, "do_shutdown: amiga_memory_report complete\n");

    log_info(LOG_GENERAL, "do_shutdown: calling shutdown_log_system - this is the last log entry\n");
    shutdown_log_system();
}
