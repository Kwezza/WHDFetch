/*

This program is designed to run on a Commodore Amiga with at least Workbench 2.04 or later.
Code compiles under SAS/C.

The utility automates the process of downloading WHDLoad packs from Retroplay on the
Turran download site (www.grandis.nu). It initiates by downloading the index.html file
from the site, parsing it to identify links to WHDLoad packs. The utility then proceeds
to download these packs, handling zip and dat files by extracting them appropriately. Each
downloaded file is organized into directories based on the WHDLoad pack type (e.g., Games,
Demos, Magazines) and further categorized by the initial letter of the file name for ease
of navigation.

This utility is designed for execution on Amiga systems equipped with a very fast
processor and a high-speed internet connection to ensure optimal performance and
efficiency in downloading WHDLoad packs. It has been developed with PiStorm-accelerated
machines in mind, taking full advantage of the significant speed enhancements it offers.
Additionally, the utility performs exceptionally well when emulated in WinUAE when
running in high speed. Please be advised that running this program on a standard Amiga
without acceleration may result in significantly slower download speeds, potentially
leading to an unbearable experience.

Key Requirements:

* A functional TCP/IP stack is mandatory for operation. For users utilizing WinUAE,
  enabling the 'bsdsocket.library' within the hardware->Expansion settings is sufficient.
* The utility uses its built-in direct HTTP downloader for fetching index pages,
    DAT ZIP archives, and individual WHDLoad `.lha` archives.
* An unzip utility is required for file extraction, tested with the Aminet version of
  UnZip (https://aminet.net/package/util/arc/UnZip).
* Compilation and development were conducted using SAS/C on the Amiga.

Compatibility Note:

While developed primarily on the RoadShow TCP/IP stack, the utility should be adaptable
to other TCP/IP stacks, though these configurations have not undergone thorough testing.

Future plans:

* Implement a feature to check for updates or new WHDLoad files. The system will store
  the downloaded DAT files and refer to them during subsequent checks against this database
  to identify any new or updated files available for download. This approach reduces the
  need to download entire WHDLoad game sets each time. Potentially, this could allow the
  software to run on original hardware with only minimal downloading needed (though it
  would still require a TCP/IP stack running).
* Allow users to filter out software they are not interested in (e.g., AGA versions, other
  language files). (Semi-done)
* Develop a new GUI to make running the software more user-friendly.
* Enable certain settings to be stored in a separate configuration file. This allows users
  to easily update settings such as web addresses or DAT file names or formatting changes
  in the future without needing a recompile.
* I'm open to other ideas - please drop me a note on my GitHub page (github.com/Kwezza)


*/
#include <exec/memory.h>
#include <exec/types.h>
#include <ctype.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/libraries.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "utilities.h"
#include "gamefile_parser.h"
#include "ini_parser.h"
#include "tag_text.h"
#include "linecounter.h"
#include "report/report.h"
#include "download/amiga_download.h"
#include "extract/extract.h"
#include "platform/platform.h"
#include "log/log.h"
#include "build_version.h"

#if defined(__AMIGA__)
/* VBCC stack override: helps avoid late-exit crashes from stack exhaustion. */
ULONG __stack = 131072;
#endif

/* Track whether the download library was successfully initialized so that
 * do_shutdown() only calls ad_cleanup_download_lib() when appropriate. */
static BOOL g_download_lib_initialized = FALSE;

typedef struct html_scan_stats {
    ULONG lines_scanned;
    ULONG href_lines_found;
    ULONG links_parsed;
    ULONG prefix_matches;
    ULONG content_matches;
    ULONG pack_filter_matches;
    ULONG pack_requested_matches;
    ULONG downloads_triggered;
} html_scan_stats;

static html_scan_stats g_html_scan_stats;

static BOOL can_write_to_extract_path(const char *extract_path)
{
    FILE *probe_file;
    char probe_file_path[512] = {0};
    size_t path_len;

    if (extract_path == NULL || extract_path[0] == '\0')
    {
        return TRUE;
    }

    path_len = strlen(extract_path);
    if (path_len + strlen("/.whddl_write_test.tmp") + 1 > sizeof(probe_file_path))
    {
        log_error(LOG_GENERAL, "extract: extract path too long for probe: '%s'\n", extract_path);
        return FALSE;
    }

    sprintf(probe_file_path, "%s/.whddl_write_test.tmp", extract_path);
    sanitize_amiga_file_path(probe_file_path);

    DeleteFile(probe_file_path);
    probe_file = fopen(probe_file_path, "w");
    if (probe_file == NULL)
    {
        log_error(LOG_GENERAL, "extract: failed write probe for extract path '%s'\n", extract_path);
        return FALSE;
    }

    fputs("ok", probe_file);
    fclose(probe_file);
    DeleteFile(probe_file_path);
    return TRUE;
}

static BOOL is_nonfatal_extract_skip(LONG extract_code)
{
    return (extract_code == EXTRACT_RESULT_SKIPPED_TOOL_MISSING ||
            extract_code == EXTRACT_RESULT_SKIPPED_UNSUPPORTED_ARCHIVE) ? TRUE : FALSE;
}

static BOOL is_retryable_archive_download_error(LONG error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
        case AD_SOCKET_ERROR:
        case AD_CONNECT_ERROR:
        case AD_CONNECTION_ERROR:
            return TRUE;
        default:
            return FALSE;
    }
}

static const char *download_retry_reason_text(LONG error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
            return "timeout";
        case AD_SOCKET_ERROR:
            return "socket error";
        case AD_CONNECT_ERROR:
            return "connect error";
        case AD_CONNECTION_ERROR:
            return "connection closed";
        case AD_CRC_ERROR:
            return "CRC mismatch";
        default:
            return "download error";
    }
}

static const char *download_failure_reason_text(LONG error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
            return "timeout";
        case AD_CANCELLED:
            return "cancelled";
        case AD_SOCKET_ERROR:
            return "socket error";
        case AD_CONNECT_ERROR:
            return "connect error";
        case AD_SEND_ERROR:
            return "send error";
        case AD_RECEIVE_ERROR:
            return "receive error";
        case AD_CONNECTION_ERROR:
            return "connection closed";
        case AD_BAD_REQUEST:
            return "HTTP 400 bad request";
        case AD_UNAUTHORIZED:
            return "HTTP 401 unauthorized";
        case AD_FORBIDDEN:
            return "HTTP 403 forbidden";
        case AD_NOT_FOUND:
            return "HTTP 404 not found";
        case AD_SERVER_ERROR:
            return "HTTP 500 server error";
        case AD_SERVICE_UNAVAILABLE:
            return "HTTP 503 service unavailable";
        case AD_REQUEST_FAILED:
            return "HTTP request failed";
        case AD_FILE_ERROR:
            return "file write error";
        case AD_CRC_ERROR:
            return "CRC mismatch";
        case AD_CRC_FORMAT_ERROR:
            return "invalid DAT CRC";
        case AD_MEMORY_ERROR:
            return "memory error";
        case AD_NOT_INITIALIZED:
            return "download library not initialized";
        case AD_INVALID_URL:
            return "invalid URL";
        case AD_HOST_NOT_FOUND:
            return "host not found";
        default:
            return "download error";
    }
}

static BOOL confirm_purge_archives(void)
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

static BOOL purge_downloaded_archives(void)
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

static const char *extract_skip_reason_from_code(LONG extract_code)
{
    if (extract_code == EXTRACT_RESULT_SKIPPED_TOOL_MISSING)
    {
        return "c:unlzx missing - install UnLZX2 from Aminet and place unlzx in C:";
    }

    if (extract_code == EXTRACT_RESULT_SKIPPED_UNSUPPORTED_ARCHIVE)
    {
        return "unsupported archive extension";
    }

    return NULL;
}

static BOOL validate_extraction_startup_configuration(const download_option *download_options)
{
    if (download_options == NULL)
    {
        return FALSE;
    }

    if (download_options->extract_archives == FALSE)
    {
        log_info(LOG_GENERAL, "extract: startup validation skipped because extraction is disabled\n");
        return TRUE;
    }

    if (!does_file_or_folder_exist("c:lha", 0))
    {
        printf("Extraction is enabled, but c:lha is missing. Install lha or use NOEXTRACT.\n");
        log_error(LOG_GENERAL, "extract: startup validation failed because c:lha is missing\n");
        return FALSE;
    }

    if (!does_file_or_folder_exist("c:unlzx", 0))
    {
        printf("Note: c:unlzx is missing. LZX archives will be skipped during extraction.\n");
        log_warning(LOG_GENERAL,
                    "extract: startup validation note - c:unlzx missing, LZX archives will be skipped\n");
    }

    if (download_options->extract_path != NULL && download_options->extract_path[0] != '\0')
    {
        if (!does_file_or_folder_exist(download_options->extract_path, 0))
        {
            printf("Extraction target '%s' does not exist.\n", download_options->extract_path);
            log_error(LOG_GENERAL,
                      "extract: startup validation failed because extract target does not exist: '%s'\n",
                      download_options->extract_path);
            return FALSE;
        }

        if (!can_write_to_extract_path(download_options->extract_path))
        {
            printf("Extraction target '%s' is not writable.\n", download_options->extract_path);
            log_error(LOG_GENERAL,
                      "extract: startup validation failed because extract target is not writable: '%s'\n",
                      download_options->extract_path);
            return FALSE;
        }
    }

    log_info(LOG_GENERAL, "extract: startup validation complete\n");
    return TRUE;
}

static void log_effective_configuration(const whdload_pack_def *pack_defs,
                                        const download_option *download_options,
                                        const char *stage)
{
    int i;

    if (pack_defs == NULL || download_options == NULL)
    {
        return;
    }

    log_info(LOG_GENERAL,
             "config[%s]: website='%s' strip_prefix='%s' prefix_filter='%s' content_filter='%s' cleanup_count=%ld\n",
             stage,
             DOWNLOAD_WEBSITE,
             FILE_PART_TO_REMOVE,
             HTML_LINK_PREFIX_FILTER,
             HTML_LINK_CONTENT_FILTER,
             (long)LINK_CLEANUP_REMOVAL_COUNT);

    for (i = 0; i < LINK_CLEANUP_REMOVAL_COUNT; i++)
    {
        if (LINK_CLEANUP_REMOVALS[i] != NULL)
        {
            log_info(LOG_GENERAL, "config[%s]: cleanup[%ld]='%s'\n", stage, (long)i, LINK_CLEANUP_REMOVALS[i]);
        }
    }

    log_info(LOG_GENERAL,
             "config[%s]: options no_skip=%ld verbose=%ld disable_counters=%ld crc_check=%ld extract=%ld extract_only=%ld skip_existing=%ld force_extract=%ld skip_download=%ld force_download=%ld extract_path='%s' delete_archives=%ld purge_archives=%ld skip_aga=%ld skip_cd=%ld skip_ntsc=%ld skip_non_english=%ld use_custom_icons=%ld unsnapshot_icons=%ld\n",
             stage,
             (long)download_options->no_skip_messages,
             (long)download_options->verbose_output,
             (long)download_options->disable_counters,
             (long)download_options->crc_check,
             (long)download_options->extract_archives,
             (long)download_options->extract_existing_only,
             (long)download_options->skip_existing_extractions,
             (long)download_options->force_extract,
             (long)download_options->skip_download_if_extracted,
             (long)download_options->force_download,
             (download_options->extract_path != NULL) ? download_options->extract_path : "",
             (long)download_options->delete_archives_after_extract,
             (long)download_options->purge_archives,
             (long)skip_AGA,
             (long)skip_CD,
             (long)skip_NTSC,
             (long)skip_NonEnglish,
             (long)download_options->use_custom_icons,
             (long)download_options->unsnapshot_icons);

    for (i = 0; i < 5; i++)
    {
        log_info(LOG_GENERAL,
                 "config[%s]: pack[%ld] name='%s' requested=%ld url='%s' dir='%s' filter_dat='%s' filter_zip='%s'\n",
                 stage,
                 (long)i,
                 pack_defs[i].full_text_name_of_pack,
                 (long)pack_defs[i].user_requested_download,
                 pack_defs[i].download_url,
                 pack_defs[i].extracted_pack_dir,
                 pack_defs[i].filter_dat_files,
                 pack_defs[i].filter_zip_files);
    }
}

static void apply_timeout_argument(download_option *options, const char *value)
{
    if (options == NULL || value == NULL)
    {
        return;
    }

    ULONG parsed_value;
    ULONG raw_value;

    if (!parse_timeout_seconds(value, &parsed_value, &raw_value))
    {
        log_warning(LOG_GENERAL, "main: invalid TIMEOUT value '%s' ignored (keeping %ld seconds)",
                    value, (long)options->timeout_seconds);
        return;
    }

    if (parsed_value != raw_value)
    {
        log_warning(LOG_GENERAL,
                    "main: TIMEOUT=%ld out of range, clamped to %ld seconds (%ld-%ld)",
                    (long)raw_value,
                    (long)parsed_value,
                    (long)TIMEOUT_SECONDS_MIN,
                    (long)TIMEOUT_SECONDS_MAX);
    }

    options->timeout_seconds = parsed_value;
}

/*
 * do_shutdown() - called before every return from main().
 * Logs each step so we can see exactly how far the shutdown gets before
 * any crash.  All log writes go directly to disk (immediate write mode).
 */
static void do_shutdown(void)
{
    log_info(LOG_GENERAL, "do_shutdown: entered\n");

    if (g_download_lib_initialized)
    {
        log_info(LOG_GENERAL, "do_shutdown: calling ad_cleanup_download_lib...\n");
        ad_cleanup_download_lib();
        g_download_lib_initialized = FALSE;
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
    /* No more logging possible after this point */
}

#define BUFFER_SIZE 1024
#define MAX_LINK_LENGTH 256
#define CLI_LINES_TO_PAUSE_AT 17

const char *DOWNLOAD_WEBSITE = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/";
const char *FILE_PART_TO_REMOVE = "Commodore%20Amiga%20-%20WHDLoad%20-%20";
const char *HTML_LINK_PREFIX_FILTER = "Commodore%20Amiga";
const char *HTML_LINK_CONTENT_FILTER = "WHDLoad";
const char *LINK_CLEANUP_REMOVALS[MAX_LINK_CLEANUP_REMOVALS] = {
    "amp;",
    "%20",
    "CommodoreAmiga-",
    "&amp;",
    "&Unofficial",
    "&Unreleased"
};
int LINK_CLEANUP_REMOVAL_COUNT = 6;

const char *WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_&_Unofficial/";
const char *WHDLOAD_DOWNLOAD_DEMOS = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Demos/";
const char *WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_&_Unofficial/";
const char *WHDLOAD_DOWNLOAD_GAMES = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games/";
const char *WHDLOAD_DOWNLOAD_MAGAZINES = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Magazines/";

const char *WHDLOAD_TEXT_NAME_DEMOS_BETA_AND_UNRELEASED = "Demos (Beta & Unofficial)";
const char *WHDLOAD_TEXT_NAME_DEMOS = "Demos";
const char *WHDLOAD_TEXT_NAME_GAMES_BETA_AND_UNRELEASED = "Games (Beta & Unofficial)";
const char *WHDLOAD_TEXT_NAME_GAMES = "Games";
const char *WHDLOAD_TEXT_NAME_MAGAZINES = "Magazines";

const char *WHDLOAD_DIR_DEMOS_BETA_AND_UNRELEASED = "Demos Beta/";
const char *WHDLOAD_DIR_DEMOS = "Demos/";
const char *WHDLOAD_DIR_GAMES_BETA_AND_UNRELEASED = "Games Beta/";
const char *WHDLOAD_DIR_GAMES = "Games/";
const char *WHDLOAD_DIR_MAGAZINES = "Magazines/";

const char *WHDLOAD_FILE_FILTER_DAT_BETA_AND_UNRELEASED = "Demos-Beta(";
const char *WHDLOAD_FILE_FILTER_DAT_DEMOS = "Demos(";
const char *WHDLOAD_FILE_FILTER_DAT_GAMES_BETA_AND_UNRELEASED = "Games-Beta";
const char *WHDLOAD_FILE_FILTER_DAT_GAMES = "Games(";
const char *WHDLOAD_FILE_FILTER_DAT_MAGAZINES = "Magazines(";

const char *WHDLOAD_FILE_FILTER_ZIP_BETA_AND_UNRELEASED = "Demos-Beta";
const char *WHDLOAD_FILE_FILTER_ZIP_DEMOS = "Demos(";
const char *WHDLOAD_FILE_FILTER_ZIP_GAMES_BETA_AND_UNRELEASED = "Games-Beta";
const char *WHDLOAD_FILE_FILTER_ZIP_GAMES = "Games(";
const char *WHDLOAD_FILE_FILTER_ZIP_MAGAZINES = "Magazines";

const char *GITHUB_ADDRESS = "https://github.com/Kwezza/Retroplay-WHDLoad-downloader";

const int DEMOS_BETA = 0;
const int DEMOS = 1;
const int GAMES_BETA = 2;
const int GAMES = 3;
const int MAGAZINES = 4;

int files_downloaded = 0;
int files_skipped = 0;
int no_skip_messages = 0;
int verbose_output = 0;

int skip_AGA = 0, skip_CD = 0, skip_NTSC = 0, skip_NonEnglish = 0;

long start_time;

#define PROGRAM_NAME_LITERAL "Retroplay WHD Downloader"
#define VERSION_STRING_LITERAL "0.9b"
#define TEMPLATE "HELP/S,DOWNLOADGAMES/S,DOWNLOADBETAGAMES/S,"                       \
                 "DOWNLOADDEMOS/S,DOWNLOADBETADEMOS/S,DOWNLOADMAGS/S,DOWNLOADALL/S," \
                 "NOSKIPREPORT/S,SKIPAGA/S,SKIPCD/S,"                                \
                 "SKIPNTSC/S,SKIPNONENGLISH/S,VERBOSE/S,NOEXTRACT/S,"                  \
                 "EXTRACTTO/K,KEEPARCHIVES/S,DELETEARCHIVES/S,PURGEARCHIVES/S,EXTRACTONLY/S,FORCEEXTRACT/S," \
                 "NODOWNLOADSKIP/S,FORCEDOWNLOAD/S,NOICONS/S,DISABLECOUNTERS/S,CRCCHECK/S"

#define textBlack "\x1B[31m"
#define textBlue "\x1B[33m"
#define textBold "\x1B[1m"
#define textGrey "\x1B[30m"
#define textItalic "\x1B[3m"
#define textReset "\x1B[0m"
#define textReverse "\x1B[7m"
#define textUnderline "\x1B[4m"
#define textWhite "\x1B[32m"

const char PROGRAM_NAME[] = PROGRAM_NAME_LITERAL;
const char VERSION_STRING[] = VERSION_STRING_LITERAL;
const char version[] = "$VER: " PROGRAM_NAME_LITERAL " " VERSION_STRING_LITERAL " (" APP_BUILD_DATE_DMY ")";
struct Library *RP_Socket_Base;

/* const char *DIR_GAME_DOWNLOADS = "GameFiles/"; */
const char *DIR_DAT_FILES = "temp/Dat files";
const char *DIR_GAME_DOWNLOADS = "GameFiles";
const char *DIR_HOLDING = "temp/holding";
const char *DIR_TEMP = "temp";
const char *DIR_ZIP_FILES = "temp/Zip files";

static BOOL build_pack_text_list_path(const struct whdload_pack_def *pack_def,
                                      char *out_path,
                                      size_t out_path_size)
{
    char *matched_name;

    if (pack_def == NULL || out_path == NULL || out_path_size == 0)
    {
        return FALSE;
    }

    matched_name = get_first_matching_fileName(pack_def->filter_dat_files);
    if (matched_name == NULL)
    {
        return FALSE;
    }

    if (snprintf(out_path, out_path_size, "%s/%s", DIR_DAT_FILES, matched_name) >= (int)out_path_size)
    {
        amiga_free(matched_name);
        return FALSE;
    }

    sanitize_amiga_file_path(out_path);
    amiga_free(matched_name);

    return does_file_or_folder_exist(out_path, 0) ? TRUE : FALSE;
}

static LONG count_total_queued_entries_for_selected_packs(struct whdload_pack_def whdload_pack_defs[],
                                                          int num_packs)
{
    LONG total_entries = 0;
    int i;

    for (i = 0; i < num_packs; i++)
    {
        char list_path[256] = {0};
        LONG list_count;

        if (whdload_pack_defs[i].user_requested_download != 1)
        {
            continue;
        }

        if (!build_pack_text_list_path(&whdload_pack_defs[i], list_path, sizeof(list_path)))
        {
            log_warning(LOG_GENERAL,
                        "download: unable to pre-count entries for pack '%s' (list not found)\n",
                        whdload_pack_defs[i].full_text_name_of_pack);
            continue;
        }

        list_count = CountLines(list_path);
        if (list_count < 0)
        {
            log_warning(LOG_GENERAL,
                        "download: failed pre-count for '%s' (path='%s')\n",
                        whdload_pack_defs[i].full_text_name_of_pack,
                        list_path);
            continue;
        }

        total_entries += list_count;
    }

    return total_entries;
}

static void parse_dat_list_entry(char *line,
                                 char **out_name,
                                 ULONG *out_size_bytes,
                                 char **out_crc)
{
    char *name_token;
    char *size_token;
    char *crc_token;

    if (out_name != NULL)
    {
        *out_name = NULL;
    }
    if (out_size_bytes != NULL)
    {
        *out_size_bytes = 0;
    }
    if (out_crc != NULL)
    {
        *out_crc = NULL;
    }

    if (line == NULL)
    {
        return;
    }

    name_token = strtok(line, "\t");
    size_token = strtok(NULL, "\t");
    crc_token = strtok(NULL, "\t");

    if (name_token != NULL)
    {
        trim(name_token);
        if (out_name != NULL)
        {
            *out_name = name_token;
        }
    }

    if (size_token != NULL && out_size_bytes != NULL)
    {
        char *end_ptr = NULL;
        ULONG parsed_size = 0;
        trim(size_token);

        if (size_token[0] != '\0')
        {
            parsed_size = (ULONG)strtoul(size_token, &end_ptr, 10);
            if (end_ptr != NULL && *end_ptr == '\0')
            {
                *out_size_bytes = parsed_size;
            }
        }
    }

    if (crc_token != NULL)
    {
        trim(crc_token);
        if (out_crc != NULL)
        {
            *out_crc = crc_token;
        }
    }
}

static ULONG archive_size_bytes_to_kb(ULONG size_bytes)
{
    if (size_bytes == 0)
    {
        return 0;
    }

    return (size_bytes + 1023UL) / 1024UL;
}

static ULONG sum_queued_bytes_from_list_file(const char *list_path)
{
    FILE *file_ptr;
    char buffer[1024] = {0};
    ULONG total_kb = 0;

    if (list_path == NULL)
    {
        return 0;
    }

    file_ptr = fopen(list_path, "r");
    if (file_ptr == NULL)
    {
        return 0;
    }

    while (fgets(buffer, sizeof(buffer), file_ptr) != NULL)
    {
        char parse_buffer[1024] = {0};
        char *archive_name = NULL;
        char *archive_crc = NULL;
        ULONG archive_size_bytes = 0;

        strncpy(parse_buffer, buffer, sizeof(parse_buffer) - 1);
        parse_buffer[sizeof(parse_buffer) - 1] = '\0';
        trim(parse_buffer);
        if (parse_buffer[0] == '\0')
        {
            continue;
        }

        parse_dat_list_entry(parse_buffer, &archive_name, &archive_size_bytes, &archive_crc);
        if (archive_name != NULL && archive_size_bytes > 0)
        {
            ULONG archive_size_kb = archive_size_bytes_to_kb(archive_size_bytes);

            if ((ULONG)(0xFFFFFFFFUL - total_kb) < archive_size_kb)
            {
                total_kb = 0xFFFFFFFFUL;
            }
            else
            {
                total_kb += archive_size_kb;
            }
        }
    }

    fclose(file_ptr);
    return total_kb;
}

static ULONG sum_total_queued_kb_for_selected_packs(struct whdload_pack_def whdload_pack_defs[],
                                                       int num_packs)
{
    ULONG total_kb = 0;
    int i;

    for (i = 0; i < num_packs; i++)
    {
        char list_path[256] = {0};

        if (whdload_pack_defs[i].user_requested_download != 1)
        {
            continue;
        }

        if (!build_pack_text_list_path(&whdload_pack_defs[i], list_path, sizeof(list_path)))
        {
            continue;
        }

        {
            ULONG pack_kb = sum_queued_bytes_from_list_file(list_path);
            if ((ULONG)(0xFFFFFFFFUL - total_kb) < pack_kb)
            {
                total_kb = 0xFFFFFFFFUL;
            }
            else
            {
                total_kb += pack_kb;
            }
        }
    }

    return total_kb;
}

/**
 * @brief Main entry point for the Retroplay WHD Downloader application
 *
 * Processes command line arguments, initializes directories, downloads and 
 * processes DAT files, and downloads WHDLoad archives based on user options.
 * Maintains counters for files downloaded and skipped, and provides a summary
 * upon completion.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return 0 for successful completion, 1 for errors or early termination
 */
int main(int argc, char *argv[])
{
    TextBuilder tb;

    struct whdload_pack_def whdload_pack_defs[5]; /* Array of pack definitions */
    struct download_option download_options;      /* User-selected download options */
    int i;                        /* Loop counter */
    int replace_files = 0;        /* Flag for replacing existing files */
    int longest_text_name = 0;    /* Length of longest pack name for formatting */
    int cancel_early = 0;         /* Flag for early cancellation */
    long elapsed_seconds;         /* Total execution time in seconds */
    long hours, minutes, seconds; /* Components of elapsed time */
    long response_code;           /* Return code from download operations */
    int download_result;          /* Result of download operations */
    int requested_pack_count = 0; /* Number of packs selected by CLI/defaults */
    int has_cli_pack_selection = 0;
    download_progress_state progress_state = {0};
    download_progress_state *active_progress_state = NULL;
    char temp_string[1024];       /* Buffer for building command strings */    
    /* Initialize application defaults and structures */
    setup_app_defaults(whdload_pack_defs, &download_options);

    /* Start the log system immediately so all shutdown steps are captured */
    initialize_log_system(TRUE);
    log_info(LOG_GENERAL, "main: starting up\n");
    report_init();

    if (ini_parser_load_overrides(whdload_pack_defs, &download_options))
    {
        log_info(LOG_GENERAL, "main: ini overrides loaded\n");
    }
    else
    {
        log_debug(LOG_GENERAL, "main: ini not found, defaults remain active\n");
    }

    log_effective_configuration(whdload_pack_defs, &download_options, "after-ini");

    for (i = 0; i < argc; i++)
    {
        if (strncasecmp_custom(argv[i], "PURGEARCHIVES", strlen(argv[i])) == 0)
        {
            if (!confirm_purge_archives())
            {
                do_shutdown();
                return 0;
            }

            if (!purge_downloaded_archives())
            {
                do_shutdown();
                return RETURN_FAIL;
            }

            do_shutdown();
            return 0;
        }
    }

    /* Show startup text and verify required programs */
    if (startup_text_and_needed_progs_are_installed(argc))
    {
        log_info(LOG_GENERAL, "main: startup check failed, exiting early\n");
        do_shutdown();
        return 1;
    }

    /* Set timezone to avoid errors in unzip program */
    ensure_time_zone_set();
    
    /* Start execution timer */
    start_time = time(NULL);

    /* If any CLI pack selection command is present, it overrides INI pack selection. */
    for (i = 0; i < argc; i++)
    {
        if (strncasecmp_custom(argv[i], "DOWNLOADGAMES", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADBETAGAMES", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADDEMOS", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADBETADEMOS", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADMAGS", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADALL", strlen(argv[i])) == 0)
        {
            has_cli_pack_selection = 1;
            break;
        }
    }

    if (has_cli_pack_selection)
    {
        for (i = 0; i < 5; i++)
        {
            whdload_pack_defs[i].user_requested_download = 0;
        }
    }

    /* Process command line arguments */
    for (i = 0; i < argc; i++)
    {
        /* HELP argument - show help text and exit */
        if (strncasecmp_custom(argv[i], "HELP", strlen(argv[i])) == 0)
        {
            startup_text_and_needed_progs_are_installed(0);
            log_info(LOG_GENERAL, "main: HELP requested, exiting\n");
            do_shutdown();
            return 0;
        }
        
        /* Pack selection arguments */
        if (strncasecmp_custom(argv[i], "DOWNLOADGAMES", strlen(argv[i])) == 0)
            whdload_pack_defs[GAMES].user_requested_download = 1;

        /* Filter options */
        if (strncasecmp_custom(argv[i], "SKIPAGA", strlen(argv[i])) == 0)
            skip_AGA = 1;

        if (strncasecmp_custom(argv[i], "SKIPCD", strlen(argv[i])) == 0)
            skip_CD = 1;

        if (strncasecmp_custom(argv[i], "SKIPNTSC", strlen(argv[i])) == 0)
            skip_NTSC = 1;

        if (strncasecmp_custom(argv[i], "SKIPNONENGLISH", strlen(argv[i])) == 0)
            skip_NonEnglish = 1;

        /* Additional pack selection arguments */
        if (strncasecmp_custom(argv[i], "DOWNLOADBETAGAMES", strlen(argv[i])) == 0)
            whdload_pack_defs[GAMES_BETA].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADDEMOS", strlen(argv[i])) == 0)
            whdload_pack_defs[DEMOS].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADBETADEMOS", strlen(argv[i])) == 0)
            whdload_pack_defs[DEMOS_BETA].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADMAGS", strlen(argv[i])) == 0)
            whdload_pack_defs[MAGAZINES].user_requested_download = 1;

        /* Download all packs */
        if (strncasecmp_custom(argv[i], "DOWNLOADALL", strlen(argv[i])) == 0)
        {
            whdload_pack_defs[GAMES].user_requested_download = 1;
            whdload_pack_defs[GAMES_BETA].user_requested_download = 1;
            whdload_pack_defs[DEMOS].user_requested_download = 1;
            whdload_pack_defs[DEMOS_BETA].user_requested_download = 1;
            whdload_pack_defs[MAGAZINES].user_requested_download = 1;
        }
        
        /* Mode settings */
        if (strncasecmp_custom(argv[i], "NOSKIPREPORT", strlen(argv[i])) == 0)
        {
            download_options.no_skip_messages = 1;
            no_skip_messages = 1;
        }

        if (strncasecmp_custom(argv[i], "VERBOSE", strlen(argv[i])) == 0)
        {
            download_options.verbose_output = 1;
        }

        if (strncasecmp_custom(argv[i], "NOEXTRACT", strlen(argv[i])) == 0)
        {
            download_options.extract_archives = FALSE;
        }

        if (strncasecmp_custom(argv[i], "EXTRACTTO=", 10) == 0)
        {
            const char *extract_target = argv[i] + 10;
            if (extract_target[0] == '\0')
            {
                download_options.extract_path = NULL;
            }
            else
            {
                download_options.extract_path = extract_target;
            }
        }

        if (strncasecmp_custom(argv[i], "KEEPARCHIVES", strlen(argv[i])) == 0)
        {
            download_options.delete_archives_after_extract = FALSE;
        }

        if (strncasecmp_custom(argv[i], "DELETEARCHIVES", strlen(argv[i])) == 0)
        {
            download_options.delete_archives_after_extract = TRUE;
        }

        if (strncasecmp_custom(argv[i], "EXTRACTONLY", strlen(argv[i])) == 0)
        {
            download_options.extract_existing_only = 1;
            download_options.extract_archives = TRUE;
        }

        if (strncasecmp_custom(argv[i], "FORCEEXTRACT", strlen(argv[i])) == 0)
        {
            download_options.force_extract = TRUE;
        }

        if (strncasecmp_custom(argv[i], "NODOWNLOADSKIP", strlen(argv[i])) == 0)
        {
            download_options.skip_download_if_extracted = FALSE;
        }

        if (strncasecmp_custom(argv[i], "FORCEDOWNLOAD", strlen(argv[i])) == 0)
        {
            download_options.force_download = TRUE;
        }

        if (strncasecmp_custom(argv[i], "PURGEARCHIVES", strlen(argv[i])) == 0)
        {
            download_options.purge_archives = TRUE;
        }

        if (strncasecmp_custom(argv[i], "NOICONS", strlen(argv[i])) == 0)
        {
            download_options.use_custom_icons = FALSE;
        }

        if (strncasecmp_custom(argv[i], "DISABLECOUNTERS", strlen(argv[i])) == 0)
        {
            download_options.disable_counters = TRUE;
        }

        if (strncasecmp_custom(argv[i], "CRCCHECK", strlen(argv[i])) == 0)
        {
            download_options.crc_check = TRUE;
        }

        if (strncasecmp_custom(argv[i], "TIMEOUT=", 8) == 0)
        {
            apply_timeout_argument(&download_options, argv[i] + 8);
        }
    }

    if (!validate_extraction_startup_configuration(&download_options))
    {
        log_error(LOG_GENERAL, "main: extraction startup validation failed, exiting\n");
        do_shutdown();
        return RETURN_FAIL;
    }

    for (i = 0; i < 5; i++)
    {
        if (whdload_pack_defs[i].user_requested_download == 1)
        {
            requested_pack_count++;
        }
    }

    if (requested_pack_count == 0)
    {
        log_warning(LOG_GENERAL, "main: no packs selected; links may be detected but none will be downloaded\n");
    }
    else
    {
        log_info(LOG_GENERAL, "main: selected packs=%ld\n", (long)requested_pack_count);
    }

    log_effective_configuration(whdload_pack_defs, &download_options, "after-cli");

    /* Create required directories */
    if (!create_Directory_and_unlock(DIR_TEMP) ||
        !create_Directory_and_unlock(DIR_ZIP_FILES) ||
        !create_Directory_and_unlock(DIR_DAT_FILES) ||
        !create_Directory_and_unlock(DIR_HOLDING) ||
        !create_Directory_and_unlock(DIR_GAME_DOWNLOADS))
    {
        printf("Failed to create one or more required directories. See general log for details.\n");
        log_error(LOG_GENERAL, "main: required directory creation failed, exiting\n");
        do_shutdown();
        return RETURN_FAIL;
    }

    /* Clean holding directory if program is run again */
    delete_all_files_in_dir(DIR_HOLDING);

    if (download_options.extract_existing_only == 1)
    {
        int extract_errors = 0;

        log_info(LOG_GENERAL, "main: EXTRACTONLY enabled; processing existing archives only\n");
        printf("\nEXTRACTONLY mode: processing existing archives from local DAT files...\n");

        for (i = 0; i < 5; i++)
        {
            if (whdload_pack_defs[i].user_requested_download == 1)
            {
                response_code = extract_existing_archives_if_file_exists(&whdload_pack_defs[i], &download_options);
                if (response_code != 0)
                {
                    extract_errors++;
                }
            }
        }

        if (extract_errors > 0)
        {
            printf("\nEXTRACTONLY completed with errors. Check PROGDIR:logs for details.\n\n");
            log_warning(LOG_GENERAL, "main: EXTRACTONLY completed with %ld pack-level errors\n", (long)extract_errors);
            do_shutdown();
            return RETURN_WARN;
        }

        printf("\nEXTRACTONLY complete.\n\n");
        log_info(LOG_GENERAL, "main: EXTRACTONLY completed successfully\n");
        do_shutdown();
        return 0;
    }

        /* Initialize with tag values */
        log_info(LOG_GENERAL, "main: calling ad_init_download_lib_taglist...\n");
        if (!ad_init_download_lib_taglist(
                ADTAG_Verbose, FALSE,             
            ADTAG_Timeout, download_options.timeout_seconds,               
                ADTAG_BufferSize, 16384,         
                ADTAG_UserAgent, "whdfetch/1.0", 
                ADTAG_MaxRetries, 2,             
                TAG_DONE))
        {
            printf("Failed to initialize download library\n");
            log_error(LOG_GENERAL, "main: ad_init_download_lib_taglist failed\n");
            do_shutdown();
            return RETURN_FAIL;
        }
        g_download_lib_initialized = TRUE;
        log_info(LOG_GENERAL, "main: download library initialized\n");

    if (download_options.crc_check == TRUE)
    {
        printf(textReset textBold "CRC verification:" textReset " ON\n");
        log_info(LOG_GENERAL, "main: CRC verification enabled (CRCCHECK)\n");
    }
    else
    {
        printf(textReset textBold "CRC verification:" textReset " OFF (use CRCCHECK to enable)\n");
        log_info(LOG_GENERAL, "main: CRC verification disabled (default)\n");
    }

    /* Download the Retroplay index page */
    printf(textReset textBold "\nGetting latest Retroplay web page from the TURRAN website site...\n" textReset);


    sprintf(temp_string, "%s/index.html", DIR_TEMP);
    download_result = ad_download_http_file_with_retries(DOWNLOAD_WEBSITE, temp_string, TRUE);

    if (download_result != AD_SUCCESS)
    {
        /* Use the helper function to print a descriptive error message */
        ad_print_download_error(download_result);
        printf("\nFailed to download the index page. Please check your internet connection, and the address is still valid.\n");
        printf("Current address: %s\n", DOWNLOAD_WEBSITE);
        log_error(LOG_GENERAL, "main: index page download failed (result=%ld), exiting\n", (long)download_result);
        do_shutdown();
        return -1;
    }
    else
    {
        printf("File successfully downloaded.             \n");
    }



    /* Process the downloaded HTML file */
    printf("\n" textReset textBold "Processing HTML file" textReset "\n");
    printf("Hunting for download links...\n");
    extract_and_validate_HTML_links(whdload_pack_defs, 5);

    /* Extract DAT files from downloaded archives */
    printf("\n" textReset textBold "Extracting DAT files..." textReset "\n");
    extract_Zip_file_and_rename(DIR_ZIP_FILES, whdload_pack_defs, 5, download_options.verbose_output);

    /* Process the extracted DAT files */
    printf("\n%s%sProcessing DAT Files...%s\n", textReset, textBold, textReset);
    printf("Extracting file names from the XML file...\n");
    process_and_archive_WHDLoadDat_Files(whdload_pack_defs, 5);
    printf("\n" textReset textBold "Extraction results:" textReset "\n");

    /* Show extraction results using formatted text builder */
    if (init_text_builder(&tb))
    {
        /* Find the longest pack name for formatting */
        for (i = 0; i < 5; i++)
        {
            if (whdload_pack_defs[i].user_requested_download == 1)
            {
                if (strlen(whdload_pack_defs[i].full_text_name_of_pack) > longest_text_name)
                {
                    longest_text_name = strlen(whdload_pack_defs[i].full_text_name_of_pack);
                }
            }
        }
        longest_text_name += 2;

        /* Add each requested pack's results to the text builder */
        for (i = 0; i < 5; i++)
        {
            if (whdload_pack_defs[i].user_requested_download == 1)
            {
                char *padded_count = pad_number_with_dots(whdload_pack_defs[i].file_count, 4);
                addf_line(&tb, "%s<ex%dd>%s archives", 
                          whdload_pack_defs[i].full_text_name_of_pack, 
                          longest_text_name, 
                          padded_count);
                amiga_free(padded_count);
            }
        }
        
        /* Display the results */
        finalize_text_builder(&tb);
        print_tagged_text(tb.lines);
        free_text_builder(&tb);
    }

    /* Process each requested pack for downloading */
    if (download_options.disable_counters == FALSE)
    {
        progress_state.total_queued_entries = count_total_queued_entries_for_selected_packs(whdload_pack_defs, 5);
        progress_state.current_download_index = 0;
        progress_state.total_queued_kb = sum_total_queued_kb_for_selected_packs(whdload_pack_defs, 5);
        progress_state.remaining_queued_kb = progress_state.total_queued_kb;
        active_progress_state = &progress_state;

        if (progress_state.total_queued_entries > 0)
        {
            printf(textReset textBold "Total queued archives to process:" textReset " %ld",
                   (long)progress_state.total_queued_entries);

            if (progress_state.total_queued_kb > 0)
            {
                ULONG total_mb_whole = progress_state.total_queued_kb / 1024UL;
                ULONG total_mb_tenths = ((progress_state.total_queued_kb % 1024UL) * 10UL) / 1024UL;

              printf(textReset textBold " (%ld.%ld MB)" textReset,
                  (long)total_mb_whole,
                  (long)total_mb_tenths);
            }

            printf("\n");
        }
    }
    else
    {
        log_info(LOG_GENERAL, "download: counters disabled by configuration\n");
    }

    for (i = 0; i < 5; i++)
    {
        if (whdload_pack_defs[i].user_requested_download == 1)
        {
            printf("\n" textReset textBold "Downloading %s..." textReset "\n", 
                   whdload_pack_defs[i].full_text_name_of_pack);
            response_code = download_roms_if_file_exists(&whdload_pack_defs[i],
                                                         &download_options,
                                                         replace_files,
                                                         active_progress_state);
            if (response_code == 20)
            {
                break;
            }
        }
    }

    /* Calculate elapsed time */
    elapsed_seconds = time(NULL) - start_time;
    hours = elapsed_seconds / 3600;
    minutes = (elapsed_seconds % 3600) / 60;
    seconds = elapsed_seconds % 60;

    /* Handle early cancellation */
    if (cancel_early == 1)
    {
        printf("\n\n" textReset textBold "Process cancelled by user or error detected." textReset "\n");
        log_info(LOG_GENERAL, "main: cancelled early, beginning shutdown\n");
        do_shutdown();
        return 1;
    }

    /* Display completion summary */
    printf("\n\n" textReset textBold "Process complete\n");
    printf(textReset textBold "================" textReset "\n\n");

    /* Print statistics for each pack */
    for (i = 0; i < 5; i++)
    {
        if (whdload_pack_defs[i].user_requested_download == 1)
        {
            printf("Stats for %s folder:" textReset textBold " %d new,  %d skipped\n" textReset, 
                   whdload_pack_defs[i].full_text_name_of_pack, 
                   whdload_pack_defs[i].count_new_files_downloaded, 
                   whdload_pack_defs[i].count_existing_files_skipped);
        }
    }
    printf("\n");

    report_write();
    printf("\n");

    /* Show future plans message */
    printf("Work in progress... You can use WHDArchiveExtractor from Aminet to\n"
           "extract the archives. Currently planning to enhanced this tool\n"
           "with WHDArchiveExtractor, enabling it to download and extract only new\n"
           "WHDLoad archives. A new reaction GUI may also be introduced\n"
           "to add more functionality, such as filters.\n");
    
    /* Show elapsed time and version */
    printf("\nElapsed time: %ld:%02ld:%02ld\n", hours, minutes, seconds);
    printf(textReset textBold " %s " textReset ", version %s\n\n", PROGRAM_NAME, VERSION_STRING);

    log_info(LOG_GENERAL, "main: normal completion, beginning shutdown\n");
    do_shutdown();
    return 0;
}

BOOL startup_text_and_needed_progs_are_installed(int number_of_args)
{
    // BOOL exitApp = FALSE;
    int bsd_version = get_bsdSocket_version();
    char *versionInfo;

    // int wbVersion = GetWorkbenchVersion();

    TextBuilder tb;
    if (init_text_builder(&tb))
    {
        if (bsd_version == -1)
        {
            addf_line(&tb, "Failed to open bsdsocket.library or no internet connection found.  If you're using WinUAE, make sure bsdsocket.library is enabled in the hardware settings.  If you're using a real Amiga, ensure you have a working TCP/IP stack installed, such as RoadShow. The program will now exit.");
            finalize_text_builder(&tb);
            print_tagged_text(tb.lines);
            free_text_builder(&tb);
            return TRUE;
        }
        else
        {
        }

        add_line(&tb, "");

        addf_line(&tb, "<b>%s</b> %s (%s %s)<ut>", PROGRAM_NAME, VERSION_STRING, APP_BUILD_DATE_DMY, __TIME__);
        add_line(&tb, "");
        add_line(&tb, "A tool to download and maintain your WHDLoad library from RetroPlay's online repository. Supports incremental updates.");

        if (!does_file_or_folder_exist("c:unzip", 0))
        {
            add_line(&tb, "<jf><b>UnZip</b> is missing from your C: directory, and it's needed to extract the downloaded DAT files. You can find a tested and working version on Aminet: http://aminet.net/package/util/arc/UnZip. The program will now exit.");
            finalize_text_builder(&tb);
            print_tagged_text(tb.lines);
            free_text_builder(&tb);
            return TRUE;
        }

        add_line(&tb, "");
        add_line(&tb, "<b>Detected program versions<ut>");

        versionInfo = get_executable_version("c:unzip");
        addf_line(&tb, "<b>UnZip:</b><ex08>%s", versionInfo);
        amiga_free(versionInfo);

        versionInfo = get_executable_version("c:lha");
        addf_line(&tb, "<b>LHA:</b><ex08>%s", versionInfo);
        amiga_free(versionInfo);

        if (does_file_or_folder_exist("c:unlzx", 0))
        {
            versionInfo = get_executable_version("c:unlzx");
            addf_line(&tb, "<b>UnLZX:</b><ex08>%s", versionInfo);
            amiga_free(versionInfo);
        }
        else
        {
            add_line(&tb, "<b>UnLZX:</b><ex08>[Not installed - .lzx archives will be skipped]");
        }

        versionInfo = run_dos_command_and_get_first_line("version");
        addf_line(&tb, "<b>Host:</b><ex08>%s", versionInfo);
        amiga_free(versionInfo);

        if (number_of_args < 2)
        {

            add_line(&tb, "");
            add_line(&tb, "<b>Usage and examples<ut>");
            add_line(&tb, "");
            add_line(&tb, "<b>Usage:</b> whdfetch [OPTIONS] [COMMANDS]");
            add_line(&tb, "");
            add_line(&tb, "<b>Commands (choose one or more):</b>");
            add_line(&tb, "  DOWNLOADGAMES/S<ex23>Download games");
            add_line(&tb, "  DOWNLOADBETAGAMES/S<ex23>Download beta and unofficial games");
            add_line(&tb, "  DOWNLOADDEMOS/S<ex23>Download demos");
            add_line(&tb, "  DOWNLOADBETADEMOS/S<ex23>Download beta and unofficial demos");
            add_line(&tb, "  DOWNLOADMAGS/S<ex23>Download magazines");
            add_line(&tb, "  DOWNLOADALL/S<ex23>Download all packs");
            add_line(&tb, "  PURGEARCHIVES/S<ex23>Delete downloaded archives under GameFiles/ recursively");
            add_line(&tb, "");
            add_line(&tb, "<b>Options (optional, choose one or more):</b>");
            add_line(&tb, "  NOSKIPREPORT/S<ex23>Don't report skipped existing archives");
            add_line(&tb, "  SKIPAGA/S<ex23>Skip AGA packages");
            add_line(&tb, "  SKIPCD/S<ex23>Skip CDTV/CD32 packages");
            add_line(&tb, "  SKIPNTSC/S<ex23>Skip NTSC packages");
            add_line(&tb, "  SKIPNONENGLISH/S<ex23>Skip non-English packages");
            add_line(&tb, "  VERBOSE/S<ex23>Show detailed UnZip output (default is quiet)");
            add_line(&tb, "  NOEXTRACT/S<ex23>Disable post-download archive extraction");
            add_line(&tb, "  EXTRACTTO/K<ex23>Extract archives to a separate target path");
            add_line(&tb, "  KEEPARCHIVES/S<ex23>Keep downloaded archives after extraction");
            add_line(&tb, "  DELETEARCHIVES/S<ex23>Delete downloaded archives after extraction");
            add_line(&tb, "  EXTRACTONLY/S<ex23>Extract already-downloaded archives only");
            add_line(&tb, "  FORCEEXTRACT/S<ex23>Always extract even when ArchiveName.txt matches");
            add_line(&tb, "  NODOWNLOADSKIP/S<ex23>Disable marker-based pre-download skip");
            add_line(&tb, "  FORCEDOWNLOAD/S<ex23>Always download even when extracted marker matches");
            add_line(&tb, "  DISABLECOUNTERS/S<ex23>Disable queued counters and pre-count pass");
            add_line(&tb, "  CRCCHECK/S<ex23>Enable CRC verification for downloaded archives");

            add_line(&tb, "");
            add_line(&tb, "<b>Examples:</b>");
            add_line(&tb, "  'whdfetch DOWNLOADGAMES'");
            add_line(&tb, "<ex06>Download all games");
            add_line(&tb, "  whdfetch DOWNLOADALL SKIPAGA");
            add_line(&tb, "<ex06>Download all packs, skipping AGA packages");
            add_line(&tb, "  whdfetch DOWNLOADGAMES EXTRACTTO=Games: KEEPARCHIVES");
            add_line(&tb, "<ex06>Download games and extract to Games: while keeping archives");
            add_line(&tb, "  whdfetch DOWNLOADBETAGAMES EXTRACTONLY EXTRACTTO=Games: KEEPARCHIVES");
            add_line(&tb, "<ex06>Extract existing beta-game archives without redownloading");
            add_line(&tb, "  whdfetch DOWNLOADGAMES FORCEEXTRACT");
            add_line(&tb, "<ex06>Force extraction even when a matching ArchiveName.txt already exists");
            add_line(&tb, "  whdfetch DOWNLOADGAMES FORCEDOWNLOAD");
            add_line(&tb, "<ex06>Bypass pre-download marker checks and always download archives");
            add_line(&tb, "");
            add_line(&tb, "<b>Notes</b><ut>");
            add_line(&tb, "</b>  -<ex04></b>A fast Amiga setup is recommended for large downloads.");
            add_line(&tb, "</b>  -<ex04>On WinUAE, entire sets can be downloaded in under an hour.");
            add_line(&tb, "</b>  -<ex04>On real Amiga hardware, speed may vary depending on CPU and network connectivity");
            add_line(&tb, "</b>  -<ex04>Archive extraction can be configured with NOEXTRACT, EXTRACTTO, KEEPARCHIVES, and DELETEARCHIVES.");
            add_line(&tb, "</b>  -<ex04>PURGEARCHIVES removes downloaded .lha/.lzx files under GameFiles/ after confirmation.");
            add_line(&tb, "</b>  -<ex04>By default, extraction is skipped when ArchiveName.txt line 2 exactly matches the archive filename.");
            add_line(&tb, "</b>  -<ex04>By default, download is also skipped when an extracted ArchiveName.txt match is found.");
            add_line(&tb, "</b>  -<ex04>Use FORCEEXTRACT to bypass the skip check and always re-extract.");
            add_line(&tb, "</b>  -<ex04>Use FORCEDOWNLOAD to bypass pre-download skip and always fetch archives.");
            add_line(&tb, "</b>  -<ex04>Use EXTRACTONLY to process archives that are already present in GameFiles/.");
            add_line(&tb, "</b>  -<ex04>PURGEARCHIVES keeps extracted folders intact; only archive files are deleted.");
            add_line(&tb, "</b>  -<ex04>For comments and suggestions, please visit the GitHub repository at https://github.com/Kwezza/RetroPlay-WHDLoad-downloader");

            add_line(&tb, "");

            finalize_text_builder(&tb);
            print_tagged_text(tb.lines);
            free_text_builder(&tb);
            return TRUE;
        }
    }
    finalize_text_builder(&tb);
    print_tagged_text(tb.lines);
    free_text_builder(&tb);
    return FALSE;
}

void setup_app_defaults(struct whdload_pack_def WHDLoadPackDefs[], struct download_option *downloadOptions)
{
    if (downloadOptions != NULL)
    {
        downloadOptions->download_games = 0;
        downloadOptions->download_games_beta = 0;
        downloadOptions->download_demos = 0;
        downloadOptions->download_demos_beta = 0;
        downloadOptions->download_magazines = 0;
        downloadOptions->download_all = 0;
        downloadOptions->no_skip_messages = 0;
        downloadOptions->verbose_output = 0;
        downloadOptions->extract_archives = TRUE;
        downloadOptions->skip_existing_extractions = TRUE;
        downloadOptions->force_extract = FALSE;
        downloadOptions->skip_download_if_extracted = TRUE;
        downloadOptions->force_download = FALSE;
        downloadOptions->extract_path = NULL;
        downloadOptions->delete_archives_after_extract = TRUE;
        downloadOptions->purge_archives = FALSE;
        downloadOptions->extract_existing_only = 0;
        downloadOptions->use_custom_icons = TRUE;
        downloadOptions->unsnapshot_icons = TRUE;
        downloadOptions->disable_counters = FALSE;
        downloadOptions->crc_check = FALSE;
        downloadOptions->timeout_seconds = 30;
    }

    WHDLoadPackDefs[DEMOS_BETA].count_existing_files_skipped = 0;
    WHDLoadPackDefs[DEMOS_BETA].count_new_files_downloaded = 0;
    WHDLoadPackDefs[DEMOS_BETA].download_url = WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].extracted_pack_dir = WHDLOAD_DIR_DEMOS_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].full_text_name_of_pack = WHDLOAD_TEXT_NAME_DEMOS_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].updated_dat_downloaded = 0;
    WHDLoadPackDefs[DEMOS_BETA].user_requested_download = 0;
    WHDLoadPackDefs[DEMOS_BETA].file_count = 0;

    WHDLoadPackDefs[DEMOS].count_existing_files_skipped = 0;
    WHDLoadPackDefs[DEMOS].count_new_files_downloaded = 0;
    WHDLoadPackDefs[DEMOS].download_url = WHDLOAD_DOWNLOAD_DEMOS;
    WHDLoadPackDefs[DEMOS].extracted_pack_dir = WHDLOAD_DIR_DEMOS;
    WHDLoadPackDefs[DEMOS].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_DEMOS;
    WHDLoadPackDefs[DEMOS].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_DEMOS;
    WHDLoadPackDefs[DEMOS].full_text_name_of_pack = WHDLOAD_TEXT_NAME_DEMOS;
    WHDLoadPackDefs[DEMOS].updated_dat_downloaded = 0;
    WHDLoadPackDefs[DEMOS].user_requested_download = 0;
    WHDLoadPackDefs[DEMOS].file_count = 0;

    WHDLoadPackDefs[GAMES_BETA].count_existing_files_skipped = 0;
    WHDLoadPackDefs[GAMES_BETA].count_new_files_downloaded = 0;
    WHDLoadPackDefs[GAMES_BETA].download_url = WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].extracted_pack_dir = WHDLOAD_DIR_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].full_text_name_of_pack = WHDLOAD_TEXT_NAME_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].updated_dat_downloaded = 0;
    WHDLoadPackDefs[GAMES_BETA].user_requested_download = 0;
    WHDLoadPackDefs[GAMES_BETA].file_count = 0;

    WHDLoadPackDefs[GAMES].count_existing_files_skipped = 0;
    WHDLoadPackDefs[GAMES].count_new_files_downloaded = 0;
    WHDLoadPackDefs[GAMES].download_url = WHDLOAD_DOWNLOAD_GAMES;
    WHDLoadPackDefs[GAMES].extracted_pack_dir = WHDLOAD_DIR_GAMES;
    WHDLoadPackDefs[GAMES].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_GAMES;
    WHDLoadPackDefs[GAMES].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_GAMES;
    WHDLoadPackDefs[GAMES].full_text_name_of_pack = WHDLOAD_TEXT_NAME_GAMES;
    WHDLoadPackDefs[GAMES].updated_dat_downloaded = 0;
    WHDLoadPackDefs[GAMES].user_requested_download = 0;
    WHDLoadPackDefs[GAMES].file_count = 0;

    WHDLoadPackDefs[MAGAZINES].count_existing_files_skipped = 0;
    WHDLoadPackDefs[MAGAZINES].count_new_files_downloaded = 0;
    WHDLoadPackDefs[MAGAZINES].download_url = WHDLOAD_DOWNLOAD_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].extracted_pack_dir = WHDLOAD_DIR_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].full_text_name_of_pack = WHDLOAD_TEXT_NAME_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].updated_dat_downloaded = 0;
    WHDLoadPackDefs[MAGAZINES].user_requested_download = 0;
    WHDLoadPackDefs[MAGAZINES].file_count = 0;
}
void extract_and_validate_HTML_links(struct whdload_pack_def WHDLoadPackDefs[], int size_WHDLoadPackDef)
{
    FILE *file;
    char buffer[BUFFER_SIZE];
    char file_path_to_html_file[256];
    sprintf(file_path_to_html_file, "%s/index.html", DIR_TEMP);

    file = fopen(file_path_to_html_file, "r");
    if (file == NULL)
    {
        printf("Error opening file.\n");
        log_error(LOG_GENERAL, "html: failed to open %s\n", file_path_to_html_file);
        return;
    }

    memset(&g_html_scan_stats, 0, sizeof(g_html_scan_stats));
    log_info(LOG_GENERAL, "html: scanning %s\n", file_path_to_html_file);
    log_info(LOG_GENERAL,
             "html: active filters prefix='%s' content='%s' remove_prefix='%s' cleanup_rules=%ld\n",
             (HTML_LINK_PREFIX_FILTER ? HTML_LINK_PREFIX_FILTER : "<null>"),
             (HTML_LINK_CONTENT_FILTER ? HTML_LINK_CONTENT_FILTER : "<null>"),
             (FILE_PART_TO_REMOVE ? FILE_PART_TO_REMOVE : "<null>"),
             (long)LINK_CLEANUP_REMOVAL_COUNT);

    /* Read the file line by line */
    while (fgets(buffer, BUFFER_SIZE, file) != NULL)
    {
        g_html_scan_stats.lines_scanned++;
        if (strstr(buffer, "<a href=\"") == NULL)
        {
            continue;
        }

        g_html_scan_stats.href_lines_found++;
        /* Extract links and check them */
        download_WHDLoadPacks_From_Links(buffer, WHDLoadPackDefs, 5);
    }

    fclose(file);

    log_info(LOG_GENERAL,
             "html: scan summary lines=%lu href_lines=%lu parsed_links=%lu prefix_match=%lu content_match=%lu pack_match=%lu requested_pack_match=%lu downloads_triggered=%lu\n",
             g_html_scan_stats.lines_scanned,
             g_html_scan_stats.href_lines_found,
             g_html_scan_stats.links_parsed,
             g_html_scan_stats.prefix_matches,
             g_html_scan_stats.content_matches,
             g_html_scan_stats.pack_filter_matches,
             g_html_scan_stats.pack_requested_matches,
             g_html_scan_stats.downloads_triggered);

    if (g_html_scan_stats.pack_requested_matches == 0)
    {
        log_warning(LOG_GENERAL, "html: no requested pack links matched. Check selected pack flags and filter strings.\n");
    }
}

int download_WHDLoadPacks_From_Links(const char *bufferm, struct whdload_pack_def WHDLoadPackDefs[], int size_WHDLoadPackDef)
{
    char *startPos;
    char link[MAX_LINK_LENGTH];
    char downloadCommand[2024];
    char fileName[128] = {0};
    int downloadFile = 0, i;
    int any_pack_match = 0;
    int download_result;          /* Result of download operations */
    //char temp_string[1024];       /* Buffer for building command strings */   
    char file_path_to_save[256];
    char download_address[256];
    /* Look for the start of a link */
    startPos = strstr(bufferm, "<a href=\"");
    if (startPos != NULL)
    {

        char *endPos = strstr(startPos, "\">");
        if (endPos != NULL)
        {
            int linkLength = endPos - (startPos + 9); /* 9 is length of '<a href="' */
            if (linkLength < MAX_LINK_LENGTH)
            {
                strncpy(link, startPos + 9, linkLength);
                link[linkLength] = '\0'; /* Ensure the string is null-terminated */
                g_html_scan_stats.links_parsed++;
                log_debug(LOG_GENERAL, "html: found href link='%s'\n", link);
                // printf("Link: %s\n", link);

                /* Check if the link matches the pattern */
                if (HTML_LINK_PREFIX_FILTER != NULL && strstr(link, HTML_LINK_PREFIX_FILTER) != NULL)
                {
                    g_html_scan_stats.prefix_matches++;
                    if (HTML_LINK_CONTENT_FILTER != NULL && strstr(link, HTML_LINK_CONTENT_FILTER)) /* make sure its a WHDLoad archive*/
                    {
                        int cleanup_index;
                        char date_buf[11] = {0};

                        g_html_scan_stats.content_matches++;

                        remove_all_occurrences(link, "amp;"); /* system has problems with the & symbol*/
                        strncpy(fileName, link, sizeof(fileName));
                        remove_all_occurrences(fileName, FILE_PART_TO_REMOVE);

                        for (cleanup_index = 0; cleanup_index < LINK_CLEANUP_REMOVAL_COUNT; cleanup_index++)
                        {
                            if (LINK_CLEANUP_REMOVALS[cleanup_index] != NULL && LINK_CLEANUP_REMOVALS[cleanup_index][0] != '\0')
                            {
                                remove_all_occurrences(fileName, LINK_CLEANUP_REMOVALS[cleanup_index]);
                            }
                        }

                        if (extract_date_from_filename(fileName, date_buf, sizeof(date_buf)) == 0)
                        {
                            strncpy(date_buf, "<none>", sizeof(date_buf) - 1);
                            date_buf[sizeof(date_buf) - 1] = '\0';
                        }

                        log_debug(LOG_GENERAL, "html: normalized file='%s' date='%s'\n", fileName, date_buf);
                        // printf("File name: %s\n", fileName);
                        downloadFile = 0;
                        for (i = 0; i < 5; i++)
                        {
                            // printf("Filter dat files: %s\n", WHDLoadPackDefs[i].filter_dat_files);

                            if (strstr(fileName, WHDLoadPackDefs[i].filter_dat_files))
                            {
                                any_pack_match = 1;
                                g_html_scan_stats.pack_filter_matches++;
                                log_info(LOG_GENERAL,
                                         "html: pack-filter match pack='%s' filter='%s' file='%s' date='%s' requested=%ld\n",
                                         WHDLoadPackDefs[i].full_text_name_of_pack,
                                         WHDLoadPackDefs[i].filter_dat_files,
                                         fileName,
                                         date_buf,
                                         (long)WHDLoadPackDefs[i].user_requested_download);

                                if (WHDLoadPackDefs[i].user_requested_download == 1)
                                {
                                    g_html_scan_stats.pack_requested_matches++;
                                    printf(textReset textBold "Found %s DAT zip file link.  Downloading..." textReset "\n", WHDLoadPackDefs[i].full_text_name_of_pack);
                                    downloadFile = compare_and_decide_DatFileDownload(fileName, WHDLoadPackDefs[i].filter_dat_files);
                                    WHDLoadPackDefs[i].updated_dat_downloaded = 1;
                                }
                            }
                        }

                        if (!any_pack_match)
                        {
                            log_debug(LOG_GENERAL, "html: no pack filter matched normalized file='%s'\n", fileName);
                        }

                        if (downloadFile == 1)
                        {
                                sprintf(file_path_to_save, "%s/%s", DIR_ZIP_FILES,fileName);
                                sprintf(download_address, "%s/%s", DOWNLOAD_WEBSITE,link);
                                g_html_scan_stats.downloads_triggered++;
                                log_info(LOG_GENERAL, "html: downloading dat zip from '%s' to '%s'\n", download_address, file_path_to_save);
                                download_result = ad_download_http_file_with_retries(download_address, file_path_to_save, TRUE);
                                log_info(LOG_GENERAL, "html: download result=%ld file='%s'\n", (long)download_result, fileName);
                            /* Legacy wget shell command removed; direct download API is used instead. */
                            //SystemTagList(downloadCommand, NULL);
                        }
                        else
                        {
                            log_debug(LOG_GENERAL, "html: no download needed for '%s'\n", fileName);
                        }
                    }
                    else
                    {
                        log_debug(LOG_GENERAL, "html: skipped (content filter mismatch) link='%s' required='%s'\n", link, HTML_LINK_CONTENT_FILTER);
                    }
                }
                else
                {
                    log_debug(LOG_GENERAL, "html: skipped (prefix mismatch) link='%s' required='%s'\n", link, HTML_LINK_PREFIX_FILTER);
                }
            }
            else
            {
                log_debug(LOG_GENERAL, "html: malformed href line, end marker not found\n");
            }
        }
        else
        {
            log_debug(LOG_GENERAL, "html: href too long, skipping line\n");
        }
    }
    return 0;
}
int compare_and_decide_DatFileDownload(char *fileName, const char *searchText)
{
    char tempDateStr[11];
    char longDate[50];
    long oldFileDate = 0, newFileDate = 0;
    char tempFileName[128] = {0};
    extract_date_from_filename(fileName, tempDateStr, sizeof(tempDateStr));
    convert_date_to_long_style(tempDateStr, longDate);
    newFileDate = convert_string_date_to_int(tempDateStr);
    log_info(LOG_GENERAL,
             "dat-check: remote file='%s' filter='%s' extracted_date='%s' numeric_date=%ld\n",
             fileName,
             searchText,
             tempDateStr,
             newFileDate);
    printf("File last updated online on %s.\n", longDate);
    Get_latest_filename_from_directory(DIR_ZIP_FILES, searchText, tempFileName);
    if (strlen(tempFileName) > 0)
    {
        extract_date_from_filename(tempFileName, tempDateStr, sizeof(tempDateStr));
        oldFileDate = convert_string_date_to_int(tempDateStr);
        log_info(LOG_GENERAL,
                 "dat-check: local file='%s' extracted_date='%s' numeric_date=%ld\n",
                 tempFileName,
                 tempDateStr,
                 oldFileDate);
        if (newFileDate > oldFileDate)
        {
            printf("The server's file is %ld days newer than the current one. Downloading now...\n",
                   newFileDate - oldFileDate);
            log_info(LOG_GENERAL, "dat-check: remote is newer by %ld days, will download\n", newFileDate - oldFileDate);
            return 1;
        }
        else
        {
            printf("Already downloaded, skipping download.\n");
            log_info(LOG_GENERAL, "dat-check: local file is up-to-date, skipping download\n");
            return 0;
        }
    }
    else
    {
        printf("Local file not found, downloading now...\n");
        log_info(LOG_GENERAL, "dat-check: no local file found for filter='%s', will download\n", searchText);
        return 1; /* file not found, download it */
    }
}

/**
 * Extracts DAT files from ZIP archives in a given directory and renames them.
 *
 * This function scans the specified directory (`zipPath`) for ZIP files that match
 * the filters defined in the `WHDLoadPackDefs` array. For each matching ZIP file,
 * if the user requested the download and the DAT was updated, it extracts the DAT
 * file from the ZIP archive using the `unzip` command, piping the output directly
 * to a new XML file in the DAT files directory. The output filename is constructed
 * using the filter and the date extracted from the ZIP filename.
 *
 * The function avoids extracting files to disk (which can cause issues with long
 * filenames on Amiga filesystems) by piping the output directly.
 *
 * @param zipPath Path to the directory containing ZIP files.
 * @param WHDLoadPackDefs Array of pack definitions with filters and flags.
 * @param size_WHDLoadPackDef Number of elements in WHDLoadPackDefs.
 * @param verboseMode If set, shows full unzip output.
 * @return TRUE if any DAT files were extracted, FALSE otherwise.
 */
BOOL extract_Zip_file_and_rename(const char *zipPath, struct whdload_pack_def WHDLoadPackDefs[], int size_WHDLoadPackDef, int verboseMode)
{
    struct FileInfoBlock *fib;    /* Pointer to file info structure for directory scanning */
    BOOL found = FALSE;           /* Flag to indicate if any DAT files were extracted */
    STRPTR fileName;              /* Pointer to the current file name being processed */
    LONG nameLen;                 /* Length of the current file name */
    BPTR lock;                    /* Lock for the directory being scanned */
    char unzipCommand[256] = {0}; /* Buffer to hold the constructed unzip command */
    char datFileName[256] = {0};  /* Buffer for the output DAT/XML file name */
    char zipFileDate[11] = {0};   /* Buffer to hold the extracted date from the ZIP filename */
    int counter = 0;              /* Counter for the number of DAT files extracted */
    int i;                        /* Loop variable for iterating pack definitions */

    /* Check if the ZIP directory exists */
    if (!does_file_or_folder_exist(zipPath, 0))
    {
        printf("Zipfile '%s' not found!\n", zipPath);
        return FALSE;
    }

    /* Lock the directory for reading */
    lock = Lock(zipPath, ACCESS_READ);
    if (!lock)
        return FALSE;

    /* Allocate memory for FileInfoBlock structure */
    fib = amiga_malloc(sizeof(struct FileInfoBlock));
    if (!fib)
    {
        printf("Failed to allocate memory for FileInfoBlock.\n");
        UnLock(lock);
        return FALSE;
    }

    /* Start examining the directory */
    if (!Examine(lock, fib))
    {
        amiga_free(fib);
        UnLock(lock);
        return FALSE;
    }

    /* Iterate through all files in the directory */
    while (ExNext(lock, fib))
    {
        /* Skip directories, only process files */
        if (fib->fib_DirEntryType >= 0)
            continue;

        fileName = fib->fib_FileName;
        nameLen = strlen(fileName);

        /* Only process files with .zip extension */
        if (nameLen <= 3 || stricmp_custom(&fileName[nameLen - 4], ".zip") != 0)
            continue;

        /* Check against all pack definitions */
        for (i = 0; i < size_WHDLoadPackDef; i++)
        {
            /* Does the filename match the current pack's filter? */
            if (!strstr(fileName, WHDLoadPackDefs[i].filter_dat_files))
                continue;
            /* Only process if user requested and DAT was updated */
            if (WHDLoadPackDefs[i].user_requested_download != 1)
                continue;
            if (WHDLoadPackDefs[i].updated_dat_downloaded != 1)
                continue;

            counter++;
            /* Extract date from filename for output file naming */
            extract_date_from_filename(fileName, zipFileDate, sizeof(zipFileDate));

            /* Build the output DAT filename */
            if (WHDLoadPackDefs[i].filter_dat_files[strlen(WHDLoadPackDefs[i].filter_dat_files) - 1] == '(')
            {
                /* If filter ends with '(', append date and closing parenthesis */
                sprintf(datFileName, "%s/%s%s).xml", DIR_DAT_FILES, WHDLoadPackDefs[i].filter_dat_files, zipFileDate);
            }
            else
            {
                /* Otherwise, append date in parentheses */
                sprintf(datFileName, "%s/%s(%s).xml", DIR_DAT_FILES, WHDLoadPackDefs[i].filter_dat_files, zipFileDate);
            }

            found = TRUE;

            /* Build the unzip command. Quiet mode is default unless VERBOSE is requested. */
            if (verboseMode == 1)
            {
                sprintf(unzipCommand, "unzip -a -p  \"%s/%s\" >\"%s\"", DIR_ZIP_FILES, fileName, datFileName);
            }
            else
            {
                sprintf(unzipCommand, "unzip -qq -a -p  \"%s/%s\" >\"%s\"", DIR_ZIP_FILES, fileName, datFileName);
            }

            /* Set timezone variable for unzip */
            SetVar("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 29, GVF_LOCAL_ONLY);

            /* Print progress message */
            printf("Extracting DATs from %s (%s)...                    ", WHDLoadPackDefs[i].full_text_name_of_pack, zipFileDate);
            fflush(stdout);

            /* Execute the unzip command */
            SystemTagList(unzipCommand, NULL);

            /* Clear the line for next output */
            ClearLine_SetCursorToStartOfLine();
            /* Optionally normalize filename for Amiga FS (currently commented out) */
            /* normalize_Zip_filename_for_Amiga_FS(fileName, WHDLoadPackDefs, size_WHDLoadPackDef); */
        }
    }

    /* Print summary of extracted files */
    printf("Extracted DAT files from %d zip files.       \n", counter);

    /* Free allocated memory and unlock directory */
    amiga_free(fib);
    UnLock(lock);
    return found;
}

/**
 * @brief Processes DAT files for each WHDLoad pack and extracts ROM names.
 *
 * This function scans the DAT files directory for XML files matching each pack's filter.
 * For each matching file, if the user requested the download and the DAT was updated,
 * it extracts ROM names to a new .txt file and deletes the original XML file.
 *
 * @param whdload_pack_defs Array of WHDLoad pack definitions.
 * @param num_packs Number of elements in whdload_pack_defs.
 * @return TRUE if any files were processed, FALSE otherwise.
 */
BOOL process_and_archive_WHDLoadDat_Files(struct whdload_pack_def whdload_pack_defs[], int num_packs)
{
    struct FileInfoBlock *fib;            /* Information structure for directory entries */
    BOOL has_file_been_processed = FALSE; /* Tracks if any files were processed */
    STRPTR file_name;                     /* Pointer to current file name */
    LONG name_len;                        /* Length of current file name */
    BPTR directory_lock;                  /* Lock on the DAT files directory */
    char file_path[256];                  /* Complete path to the input XML file */
    char output_file_path[256];           /* Complete path to the output text file */
    int i;                                /* Loop counter for pack definitions */

    /* Lock the directory containing DAT files for reading */
    directory_lock = Lock(DIR_DAT_FILES, ACCESS_READ);
    if (!directory_lock)
    {
        printf("Failed to lock the directory: %s\n", DIR_DAT_FILES);
        return FALSE;
    }

    /* Allocate memory for the FileInfoBlock structure used to examine directory entries */
    fib = amiga_malloc(sizeof(struct FileInfoBlock));
    if (!fib)
    {
        printf("Failed to allocate memory for FileInfoBlock.\n");
        UnLock(directory_lock);
        return FALSE;
    }

    /* Begin examining directory entries */
    if (Examine(directory_lock, fib))
    {
        /* Process each entry in the directory */
        while (ExNext(directory_lock, fib))
        {
            /* Only process files (not directories) */
            if (fib->fib_DirEntryType < 0)
            {
                file_name = fib->fib_FileName;
                name_len = strlen(file_name);

                /* Only process .xml files */
                if (name_len > 4 && stricmp_custom(file_name + name_len - 4, ".xml") == 0)
                {
                    /* Check each pack definition for a match with this XML file */
                    for (i = 0; i < num_packs; i++)
                    {
                        /* Process file if:
                           1. Filename contains the filter string for this pack
                           2. User requested to download this pack
                           3. Updated DAT file was downloaded for this pack */
                        if (strstr(file_name, whdload_pack_defs[i].filter_dat_files) && 
                            whdload_pack_defs[i].user_requested_download == 1 && 
                            whdload_pack_defs[i].updated_dat_downloaded == 1)
                        {
                            char temp_file_name[256];
                            has_file_been_processed = TRUE;
                            
                            /* Build the full path to the input XML file */
                            sprintf(file_path, "%s/%s", DIR_DAT_FILES, file_name);

                            /* Create output filename by removing .xml extension */
                            strncpy(temp_file_name, file_name, name_len - 4);
                            temp_file_name[name_len - 4] = '\0';
                            sprintf(output_file_path, "%s/%s.txt", DIR_DAT_FILES, temp_file_name);

                            /* Extract ROM names from XML file and save to text file */
                            whdload_pack_defs[i].file_count = extract_and_save_rom_names_from_XML(
                                file_path, 
                                output_file_path, 
                                whdload_pack_defs[i].full_text_name_of_pack
                            );

                            /* Delete the original XML file to save space */
                            DeleteFile(file_path);
                        }
                    }
                }
            }
        }
    }

    /* Clean up resources */
    amiga_free(fib);
    UnLock(directory_lock);
    
    /* Return whether any files were processed */
    return has_file_been_processed;
}

/**
 * @brief Determines if a game should be included based on filtering criteria.
 *
 * Checks if a game should be included in the output based on multiple filter
 * criteria such as AGA compatibility, CD format, NTSC format, and language.
 *
 * @param game Pointer to game_metadata structure containing game information.
 * @return int 1 if the game should be included, 0 if it should be filtered out.
 */
int should_include_game(const game_metadata *game)
{
    /* Check if AGA games should be skipped */
    if (strcmp(game->machineType, "AGA") == 0 && skip_AGA == 1)
    {
        return 0;
    }

    /* Check if CD32/CDTV games should be skipped */
    if (((strcmp(game->mediaFormat, "CD32") == 0) ||
         (strcmp(game->mediaFormat, "CDTV") == 0)) &&
        skip_CD == 1)
    {
        return 0;
    }

    /* Check if NTSC games should be skipped */
    if (strcmp(game->videoFormat, "NTSC") == 0 && skip_NTSC == 1)
    {
        return 0;
    }

    /* Check if non-English games should be skipped */
    if (skip_NonEnglish == 1 && strcmp(game->language, "English") != 0)
    {
        return 0;
    }

    /* If all filters pass, include the game */
    return 1;
}

static BOOL extract_xml_attribute_value(const char *source,
                                        const char *attribute_name,
                                        char *out_value,
                                        size_t out_value_size)
{
    char pattern[32] = {0};
    const char *value_start;
    const char *value_end;
    size_t value_length;

    if (source == NULL || attribute_name == NULL || out_value == NULL || out_value_size == 0)
    {
        return FALSE;
    }

    if (snprintf(pattern, sizeof(pattern), "%s=\"", attribute_name) >= (int)sizeof(pattern))
    {
        return FALSE;
    }

    value_start = strstr(source, pattern);
    if (value_start == NULL)
    {
        return FALSE;
    }

    value_start += strlen(pattern);
    value_end = strchr(value_start, '"');
    if (value_end == NULL)
    {
        return FALSE;
    }

    value_length = (size_t)(value_end - value_start);
    if (value_length >= out_value_size)
    {
        value_length = out_value_size - 1;
    }

    strncpy(out_value, value_start, value_length);
    out_value[value_length] = '\0';

    return TRUE;
}

/**
 * @brief Extracts ROM names from an XML file and saves them to a text file.
 *
 * Processes an XML file containing ROM information, extracts ROM names from <rom>
 * tags, applies filtering based on machine type, media format, video format, and
 * language, and saves the filtered names to a text file.
 *
 * @param input_file_path Path to the input XML file.
 * @param output_file_path Path to the output text file.
 * @param pack_name Name of the WHDLoad pack for progress messages.
 * @return int Number of ROM names extracted and saved.
 */
int extract_and_save_rom_names_from_XML(char *input_file_path, char *output_file_path, const char *pack_name)
{
    FILE *input_file = NULL;      /* File handle for input */
    FILE *output_file = NULL;     /* File handle for output */
    char line[1024];              /* Buffer for reading lines */
    char *rom_start = NULL;       /* Pointer to <rom> tag in the line */
    char *name_attr_start = NULL; /* Pointer to start of name attribute */
    char *name_attr_end = NULL;   /* Pointer to end of name attribute */
    char name_value[1024];        /* Buffer for extracted name value */
    int name_length = 0;          /* Length of extracted name */
    game_metadata game;           /* Structure to hold parsed game info */
    int file_count = 0;           /* Count of files processed */
    long line_count_of_file = 0;  /* Total lines in input file */
    long line_count = 0;          /* Current line being processed */
    time_t last_update_time = 0;  /* Time of last progress update */
    time_t current_time;          /* Current time for progress updates */
    char percent_str[32];         /* Buffer for percentage string */
    char size_value[32];          /* Extracted size attribute */
    char crc_value[64];           /* Extracted crc attribute */

    /* Remove any existing output file */
    DeleteFile(output_file_path);

    /* Count lines in the input file for progress tracking */
    line_count_of_file = CountLines(input_file_path);

    /* Open the input and output files */
    input_file = fopen(input_file_path, "r");
    output_file = fopen(output_file_path, "w");

    /* Check if files opened successfully */
    if (input_file == NULL || output_file == NULL)
    {
        perror("Error opening file");
        /* Close any file that was opened successfully */
        if (input_file != NULL)
            fclose(input_file);
        if (output_file != NULL)
            fclose(output_file);
        return 0;
    }

    /* Set stdout to unbuffered mode for real-time updates */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Initialize counters and timer */
    line_count = 0;
    last_update_time = time(NULL);

    /* Process each line in the input file */
    while (fgets(line, sizeof(line), input_file) != NULL)
    {
        /* Look for the <rom tag in the current line */
        rom_start = strstr(line, "<rom");
        if (rom_start != NULL)
        {
            /* Find the start of the name attribute */
            name_attr_start = strstr(rom_start, "name=\"");
            if (name_attr_start != NULL)
            {
                /* Extract the name attribute value */
                name_attr_start += 6;                         /* Skip "name=" */
                name_attr_end = strchr(name_attr_start, '"'); /* Find closing quote */

                if (name_attr_end != NULL)
                {
                    /* Calculate the length and extract the name */
                    name_length = name_attr_end - name_attr_start;
                    strncpy(name_value, name_attr_start, name_length);
                    name_value[name_length] = '\0';

                    /* Clean up the name value */
                    remove_all_occurrences(name_value, "amp;");

                    /* Parse game information from the filename */
                    extract_game_info_from_filename(name_value, &game);

                    /* Check if the game should be included based on filters */
                    if (should_include_game(&game))
                    {
                        BOOL has_size;
                        BOOL has_crc;

                        size_value[0] = '\0';
                        crc_value[0] = '\0';
                        has_size = extract_xml_attribute_value(rom_start, "size", size_value, sizeof(size_value));
                        has_crc = extract_xml_attribute_value(rom_start, "crc", crc_value, sizeof(crc_value));

                        /* Write name and optional metadata as TSV: name\tsize\tcrc */
                        if (has_size || has_crc)
                        {
                            fprintf(output_file,
                                    "%s\t%s\t%s\n",
                                    name_value,
                                    has_size ? size_value : "",
                                    has_crc ? crc_value : "");
                        }
                        else
                        {
                            fprintf(output_file, "%s\n", name_value);
                        }

                        file_count++;
                    }
                }
            }
        }

        /* Update line counter */
        line_count++;

        /* Update progress display once per second */
        current_time = time(NULL);
        if (current_time - last_update_time >= 1)
        {
            sprintf(percent_str, "\r%s progress: %d %%",
                    pack_name, (int)((line_count * 100) / line_count_of_file));
            printf("%s", percent_str);
            last_update_time = current_time;
        }
    }

    /* Print completion message */
    printf("\r%s complete      \n", pack_name);

    /* Close files */
    fclose(input_file);
    fclose(output_file);

    return file_count;
}

char *get_first_matching_fileName(const char *fileNameToFind)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char pattern[256], *fullFilename = NULL;

    /* Construct the full pattern to match, adjusted for correct pattern usage */
    sprintf(pattern, "%s", fileNameToFind);

    if (!(fib = amiga_malloc(sizeof(struct FileInfoBlock))))
    {
        return NULL;
    }

    if (!(lock = Lock(DIR_DAT_FILES, ACCESS_READ)))
    {
        amiga_free(fib);
        return NULL;
    }

    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            if (strlen(fib->fib_FileName) > strlen(fileNameToFind))
            {
                /* length greater */
                if (strncasecmp_custom(pattern, fib->fib_FileName, strlen(pattern)) == 0)
                {
                    /* Pattern matched, construct full filename */
                    fullFilename = amiga_malloc(strlen(fib->fib_FileName) + 1);
                    if (fullFilename)
                    {
                        strcpy(fullFilename, fib->fib_FileName);
                    }
                    break; /* Exit loop once match is found */
                }
            }
            /* } */
        }
    }

    UnLock(lock);
    amiga_free(fib);
    return fullFilename;
}

long download_roms_if_file_exists(struct whdload_pack_def *WHDLoadPackDefs,
                                  const struct download_option *download_options,
                                  int replaceFiles,
                                  download_progress_state *progress_state)
{
    char foundFilename[256] = {0};
    char *fileNameToRead = get_first_matching_fileName(WHDLoadPackDefs->filter_dat_files);
    long returnCode = 0;

    if (fileNameToRead == NULL)
    {
        log_warning(LOG_GENERAL,
                    "download: no DAT file found for filter '%s' in '%s'\n",
                    WHDLoadPackDefs->filter_dat_files,
                    DIR_DAT_FILES);
        return 20;
    }

    sanitize_amiga_file_path(fileNameToRead);
    sprintf(foundFilename, "%s/%s", DIR_DAT_FILES, fileNameToRead);

    if (does_file_or_folder_exist(foundFilename, 0))
    {
        returnCode = download_roms_from_file(foundFilename,
                                             WHDLoadPackDefs,
                                             download_options,
                                             replaceFiles,
                                             progress_state);
    }
    else
    {
        printf("File not found %s\n", foundFilename);
        returnCode = 20;
    }

    amiga_free(fileNameToRead);
    return returnCode;
}

LONG extract_existing_archives_if_file_exists(struct whdload_pack_def *WHDLoadPackDefs,
                                              const struct download_option *download_options)
{
    char foundFilename[256] = {0};
    char *fileNameToRead = get_first_matching_fileName(WHDLoadPackDefs->filter_dat_files);
    LONG returnCode = 0;

    if (fileNameToRead == NULL)
    {
        log_warning(LOG_GENERAL,
                    "extract: no DAT file found for filter '%s' in '%s'\n",
                    WHDLoadPackDefs->filter_dat_files,
                    DIR_DAT_FILES);
        return 20;
    }

    sanitize_amiga_file_path(fileNameToRead);
    sprintf(foundFilename, "%s/%s", DIR_DAT_FILES, fileNameToRead);

    if (does_file_or_folder_exist(foundFilename, 0))
    {
        returnCode = extract_existing_archives_from_file(foundFilename, WHDLoadPackDefs, download_options);
    }
    else
    {
        printf("DAT file not found %s\n", foundFilename);
        log_warning(LOG_GENERAL, "extract: DAT file not found '%s'\n", foundFilename);
        returnCode = 20;
    }

    amiga_free(fileNameToRead);
    return returnCode;
}

LONG extract_existing_archives_from_file(const char *filename,
                                         struct whdload_pack_def *WHDLoadPackDefs,
                                         const struct download_option *download_options)
{
    FILE *filePtr;
    char buffer[1024] = {0};
    char parse_buffer[1024] = {0};
    char archivePath[512] = {0};
    char firstLetter[2] = {0};
    char *archive_name = NULL;
    char *archive_crc = NULL;
    ULONG archive_size_bytes = 0;
    int len = 0;
    LONG extractCode = 0;
    LONG extractedCount = 0;
    LONG missingCount = 0;
    LONG skippedCount = 0;
    LONG failedCount = 0;

    filePtr = fopen(filename, "r");
    if (filePtr == NULL)
    {
        log_error(LOG_GENERAL, "extract: unable to open DAT file '%s'\n", filename);
        return 1;
    }

    while (fgets(buffer, sizeof(buffer), filePtr) != NULL)
    {
        len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n'))
        {
            buffer[len - 1] = '\0';
            len--;
        }

        if (buffer[0] == '\0')
        {
            continue;
        }

        strncpy(parse_buffer, buffer, sizeof(parse_buffer) - 1);
        parse_buffer[sizeof(parse_buffer) - 1] = '\0';
        parse_dat_list_entry(parse_buffer, &archive_name, &archive_size_bytes, &archive_crc);
        if (archive_name == NULL || archive_name[0] == '\0')
        {
            continue;
        }

        firstLetter[0] = archive_name[0];
        firstLetter[1] = '\0';
        get_folder_name_from_character(firstLetter);

        sprintf(archivePath,
                "%s/%s/%s/%s",
                DIR_GAME_DOWNLOADS,
                WHDLoadPackDefs->extracted_pack_dir,
                firstLetter,
                archive_name);
        sanitize_amiga_file_path(archivePath);

        if (!does_file_or_folder_exist(archivePath, 1))
        {
            missingCount++;
            continue;
        }

        extractCode = extract_process_downloaded_archive(archivePath,
                                                         archive_name,
                                                         WHDLoadPackDefs->extracted_pack_dir,
                                                         firstLetter,
                                                         WHDLoadPackDefs->full_text_name_of_pack,
                                                         download_options);
        if (extractCode == EXTRACT_RESULT_OK)
        {
            extractedCount++;
        }
        else if (is_nonfatal_extract_skip(extractCode))
        {
            const char *skip_reason = extract_skip_reason_from_code(extractCode);

            skippedCount++;
            log_info(LOG_GENERAL,
                     "extract: skipped existing archive '%s' (code=%ld)\n",
                     archivePath,
                     (long)extractCode);

            if (skip_reason != NULL)
            {
                report_add_extraction_skip(archive_name,
                                           WHDLoadPackDefs->full_text_name_of_pack,
                                           skip_reason);
            }
        }
        else
        {
            failedCount++;
            log_warning(LOG_GENERAL,
                        "extract: failed to process existing archive '%s' (code=%ld)\n",
                        archivePath,
                        (long)extractCode);
        }
    }

    fclose(filePtr);

    log_info(LOG_GENERAL,
             "extract: existing archive pass for '%s' complete (extracted=%ld missing=%ld skipped=%ld failed=%ld)\n",
             WHDLoadPackDefs->full_text_name_of_pack,
             (long)extractedCount,
             (long)missingCount,
             (long)skippedCount,
             (long)failedCount);

    return (failedCount > 0) ? 1 : 0;
}

/* Function to read a file and print its contents */
long download_roms_from_file(const char *filename,
                             struct whdload_pack_def *WHDLoadPackDefs,
                             const struct download_option *download_options,
                             int replaceFiles,
                             download_progress_state *progress_state)
{
    FILE *filePtr;
    char buffer[1024] = {0};
    char parse_buffer[1024] = {0};
    char *archive_name = NULL;
    char *archive_crc = NULL;
    ULONG archive_size_bytes = 0;
    int len = 0;
    filePtr = fopen(filename, "r");
    if (filePtr == NULL)
    {
        printf("Error: Unable to open file %s\n", filename);
        return 1;
    }
    printf(textReset "Scanning...\n");

    while (fgets(buffer, sizeof(buffer), filePtr) != NULL)
    {
        len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n'))
        {                           /* Check if the last character is a carriage return */
            buffer[len - 1] = '\0'; /* Replace it with a null terminator */
            len--;
        }

        if (buffer[0] == '\0')
        {
            continue;
        }

        strncpy(parse_buffer, buffer, sizeof(parse_buffer) - 1);
        parse_buffer[sizeof(parse_buffer) - 1] = '\0';
        parse_dat_list_entry(parse_buffer, &archive_name, &archive_size_bytes, &archive_crc);
        if (archive_name == NULL || archive_name[0] == '\0')
        {
            continue;
        }

        if (execute_archive_download_command(archive_name,
                                             archive_size_bytes,
                                             archive_crc,
                                             WHDLoadPackDefs,
                                             download_options,
                                             replaceFiles,
                                             progress_state) == 20)
        {
            printf(textReset "User cancelled download or error\n");
            fclose(filePtr);
            return 20;
        }
    }

    fclose(filePtr);
    if (WHDLoadPackDefs->count_new_files_downloaded < 1)
    {
        printf(textReset "No new files to download.\n");
    }
    return 0;
}

LONG execute_archive_download_command(const char *downloadWHDFile,
                                      ULONG archive_size_bytes,
                                      const char *archive_crc,
                                      struct whdload_pack_def *WHDLoadPackDefs,
                                      const struct download_option *download_options,
                                      int replaceFiles,
                                      download_progress_state *progress_state)
{
    char fileName[256] = {0};
    char downloadUrl[256] = {0};
    char downloadFirstLetter[2] = {0};
    char formattedFileName[256] = {0};
    char formattedFileNameP2[256] = {0};
    char matchedExtractFolder[EXTRACT_MAX_PATH] = {0};
    BOOL download_silent = FALSE;
    LONG extractCode;
    LONG returnCode;
    LONG current_download_number = 0;
    ULONG remaining_mb_whole = 0;
    ULONG remaining_mb_tenths = 0;
    ULONG archive_size_kb = archive_size_bytes_to_kb(archive_size_bytes);
    ULONG max_retries = 0;
    ULONG total_attempts = 1;
    ULONG attempt_number = 0;
    ULONG backoff_seconds = 1;

    if (replaceFiles != 0)
    {
        log_debug(LOG_DOWNLOAD, "download: replaceFiles flag is set but currently unused\n");
    }

    if (archive_crc != NULL && archive_crc[0] != '\0')
    {
        log_debug(LOG_DOWNLOAD,
                  "download: metadata for '%s' size=%ld crc=%s\n",
                  downloadWHDFile,
                  (long)archive_size_bytes,
                  archive_crc);
    }

    sprintf(downloadFirstLetter, "%c", downloadWHDFile[0]);
    get_folder_name_from_character(downloadFirstLetter);

    sprintf(fileName, "%s/%s/%s/%s", DIR_GAME_DOWNLOADS, WHDLoadPackDefs->extracted_pack_dir, downloadFirstLetter, downloadWHDFile);
    sanitize_amiga_file_path(fileName);

    /* If a .downloading marker exists from a previous interrupted run,
       delete the partial archive and fall through to re-download. */
    if (has_download_marker(fileName))
    {
        printf(textReset "Incomplete download detected, re-downloading: %s\n", downloadWHDFile);
        log_info(LOG_DOWNLOAD,
                 "download: found .downloading marker for '%s', deleting partial file and re-downloading\n",
                 downloadWHDFile);
        DeleteFile((STRPTR)fileName);   /* may not exist — that's OK */
        delete_download_marker(fileName);
    }

    if (does_file_or_folder_exist(fileName, 1) &&
        !(download_options != NULL && download_options->force_download == TRUE))
    {
        if (no_skip_messages < 1)
        {
            printf(textReset "File already exists, skipping: %s\n", downloadWHDFile);
        }
        if (download_options != NULL && download_options->extract_archives == TRUE)
        {
            const char *skip_reason = NULL;

            extractCode = extract_process_downloaded_archive(fileName,
                                                             downloadWHDFile,
                                                             WHDLoadPackDefs->extracted_pack_dir,
                                                             downloadFirstLetter,
                                                             WHDLoadPackDefs->full_text_name_of_pack,
                                                             download_options);

            skip_reason = extract_skip_reason_from_code(extractCode);
            if (skip_reason != NULL)
            {
                report_add_extraction_skip(downloadWHDFile,
                                           WHDLoadPackDefs->full_text_name_of_pack,
                                           skip_reason);
            }

            if (extractCode != EXTRACT_RESULT_OK && !is_nonfatal_extract_skip(extractCode))
            {
                log_warning(LOG_GENERAL,
                            "extract: existing archive '%s' failed extraction check/recovery (code=%ld), continuing\n",
                            downloadWHDFile,
                            (long)extractCode);
            }
            else
            {
                log_debug(LOG_GENERAL,
                          "extract: existing archive '%s' passed extraction check/recovery\n",
                          downloadWHDFile);
            }
        }

        report_add_local_cache_reuse(downloadWHDFile, WHDLoadPackDefs->full_text_name_of_pack);
        log_info(LOG_GENERAL,
             "download: reused local archive cache for '%s' (no HTTP download)\n",
             downloadWHDFile);

        files_skipped = files_skipped + 1;
        WHDLoadPackDefs->count_existing_files_skipped = WHDLoadPackDefs->count_existing_files_skipped + 1;

        if (progress_state != NULL && archive_size_kb > 0)
        {
            if (progress_state->remaining_queued_kb >= archive_size_kb)
            {
                progress_state->remaining_queued_kb -= archive_size_kb;
            }
            else
            {
                progress_state->remaining_queued_kb = 0;
            }
        }

        return 2;
    }

    if (download_options != NULL &&
        extract_is_archive_already_extracted(fileName,
                                             downloadWHDFile,
                                             WHDLoadPackDefs->extracted_pack_dir,
                                             downloadFirstLetter,
                                             download_options,
                                             matchedExtractFolder,
                                             sizeof(matchedExtractFolder)))
    {
        if (no_skip_messages < 1)
        {
            printf(textReset "Already extracted, skipping download: %s\n", downloadWHDFile);
        }

        log_info(LOG_GENERAL,
                 "download: skipped '%s' because extracted marker already exists at '%s'\n",
                 downloadWHDFile,
                 matchedExtractFolder);

        files_skipped = files_skipped + 1;
        WHDLoadPackDefs->count_existing_files_skipped = WHDLoadPackDefs->count_existing_files_skipped + 1;

        if (progress_state != NULL && archive_size_kb > 0)
        {
            if (progress_state->remaining_queued_kb >= archive_size_kb)
            {
                progress_state->remaining_queued_kb -= archive_size_kb;
            }
            else
            {
                progress_state->remaining_queued_kb = 0;
            }
        }

        return 2;
    }

    if (progress_state != NULL)
    {
        ULONG remaining_kb = progress_state->remaining_queued_kb;

        progress_state->current_download_index = progress_state->current_download_index + 1;
        current_download_number = progress_state->current_download_index;

        remaining_mb_whole = remaining_kb / 1024UL;
        remaining_mb_tenths = ((remaining_kb % 1024UL) * 10UL) / 1024UL;
    }

    Format_text_split_by_Caps(downloadWHDFile, formattedFileName, sizeof(formattedFileName));
    turn_filename_into_text_with_spaces(downloadWHDFile, formattedFileNameP2);

    if (progress_state != NULL && progress_state->total_queued_entries > 0)
    {
           printf("\n" textReset textBold "Download %ld of %ld (%ld.%ld MB left)\n" textReset textBold "Fetching %s " textReset "(%s) (Ctrl-c to cancel)" textBlue ".\n\n",
               (long)current_download_number,
               (long)progress_state->total_queued_entries,
               (long)remaining_mb_whole,
               (long)remaining_mb_tenths,
               formattedFileName,
               formattedFileNameP2);
    }
    else
    {
        printf("\n" textReset textBold "Fetching %s " textReset "(%s) (Ctrl-c to cancel)" textBlue ".\n\n",
               formattedFileName,
               formattedFileNameP2);
    }

    create_directory_based_on_filename(WHDLoadPackDefs->extracted_pack_dir, downloadWHDFile);
    sprintf(downloadUrl, "%s%s/%s", WHDLoadPackDefs->download_url, downloadFirstLetter, downloadWHDFile);

    if (download_options != NULL && download_options->verbose_output == FALSE)
    {
        download_silent = TRUE;
    }

    if (!ad_get_config_value(ADTAG_MaxRetries, &max_retries))
    {
        max_retries = 2;
        log_warning(LOG_DOWNLOAD,
                    "download: failed to read ADTAG_MaxRetries, using fallback retries=%ld for '%s'\n",
                    (long)max_retries,
                    downloadWHDFile);
    }

    total_attempts = max_retries + 1;
    returnCode = AD_ERROR;

    for (attempt_number = 1; attempt_number <= total_attempts; attempt_number++)
    {
        BOOL retry_due_to_error = FALSE;
        BOOL retry_due_to_crc = FALSE;

        /* Drop a marker so interrupted downloads are detected on next run */
        create_download_marker(fileName);

        log_info(LOG_DOWNLOAD,
                 "download: attempt %ld/%ld for '%s'\n",
                 (long)attempt_number,
                 (long)total_attempts,
                 downloadWHDFile);

        /* Default runs use reduced output unless VERBOSE is requested. */
        returnCode = (LONG)ad_download_http_file(downloadUrl, fileName, download_silent);

        if (returnCode == AD_CANCELLED)
        {
            log_info(LOG_DOWNLOAD, "download: user cancelled '%s' via Ctrl-C\n", downloadWHDFile);
            /* Leave partial file + marker on disk; next run will clean up */
            return 20;
        }

        if (returnCode == AD_SUCCESS &&
            download_options != NULL &&
            download_options->crc_check == TRUE &&
            archive_crc != NULL && archive_crc[0] != '\0')
        {
            LONG crc_result = (LONG)ad_verify_file_crc(fileName, archive_crc);
            if (crc_result != AD_SUCCESS)
            {
                returnCode = crc_result;
                if (attempt_number < total_attempts)
                {
                    printf(textReset "CRC failed: %s (expected %s) - retrying.\n",
                           downloadWHDFile,
                           archive_crc);
                }
                else
                {
                    printf(textReset "CRC failed: %s (expected %s) - no retries left.\n",
                           downloadWHDFile,
                           archive_crc);
                }
                fflush(stdout);
                log_warning(LOG_DOWNLOAD,
                            "download: CRC verification failed for '%s' on attempt %ld/%ld (expected=%s, code=%ld)\n",
                            downloadWHDFile,
                            (long)attempt_number,
                            (long)total_attempts,
                            archive_crc,
                            (long)returnCode);
            }
            else
            {
                printf(textReset "CRC OK: %s\n", downloadWHDFile);
                fflush(stdout);
                log_info(LOG_DOWNLOAD,
                         "download: CRC verified for '%s' on attempt %ld/%ld\n",
                         downloadWHDFile,
                         (long)attempt_number,
                         (long)total_attempts);
            }
        }

        if (returnCode == AD_SUCCESS)
        {
            if (attempt_number > 1)
            {
                printf(textReset "Download completed after retry %ld/%ld: %s\n",
                       (long)(attempt_number - 1),
                       (long)max_retries,
                       downloadWHDFile);
                fflush(stdout);
            }

            delete_download_marker(fileName);
            break;
        }

        ad_print_download_error((int)returnCode);
        log_error(LOG_DOWNLOAD,
                  "download: attempt %ld/%ld failed for '%s' (code=%ld)\n",
                  (long)attempt_number,
                  (long)total_attempts,
                  downloadWHDFile,
                  (long)returnCode);

        retry_due_to_error = is_retryable_archive_download_error(returnCode);
        retry_due_to_crc = (returnCode == AD_CRC_ERROR);

        if ((retry_due_to_error || retry_due_to_crc) && attempt_number < total_attempts)
        {
            DeleteFile((STRPTR)fileName);
            delete_download_marker(fileName);

            printf("Retry %ld/%ld after %s in %ld second%s...\n",
                   (long)attempt_number,
                   (long)max_retries,
                   download_retry_reason_text(returnCode),
                   (long)backoff_seconds,
                   (backoff_seconds == 1) ? "" : "s");

            log_info(LOG_DOWNLOAD,
                     "download: retrying '%s' after %s (retry %ld/%ld, delay=%lds)\n",
                     downloadWHDFile,
                     download_retry_reason_text(returnCode),
                     (long)attempt_number,
                     (long)max_retries,
                     (long)backoff_seconds);

            Delay(backoff_seconds * 50); /* Amiga DOS ticks are 1/50 sec */

            if (backoff_seconds < 8)
            {
                backoff_seconds <<= 1;
            }

            continue;
        }

        DeleteFile((STRPTR)fileName);
        delete_download_marker(fileName);

        report_add_download_failure(downloadWHDFile,
                                    WHDLoadPackDefs->full_text_name_of_pack,
                                    download_failure_reason_text(returnCode));

        if (progress_state != NULL && archive_size_kb > 0)
        {
            if (progress_state->remaining_queued_kb >= archive_size_kb)
            {
                progress_state->remaining_queued_kb -= archive_size_kb;
            }
            else
            {
                progress_state->remaining_queued_kb = 0;
            }
        }

        return returnCode;
    }

    if (returnCode != AD_SUCCESS)
    {
        DeleteFile((STRPTR)fileName);
        delete_download_marker(fileName);

        report_add_download_failure(downloadWHDFile,
                                    WHDLoadPackDefs->full_text_name_of_pack,
                                    download_failure_reason_text(returnCode));

        if (progress_state != NULL && archive_size_kb > 0)
        {
            if (progress_state->remaining_queued_kb >= archive_size_kb)
            {
                progress_state->remaining_queued_kb -= archive_size_kb;
            }
            else
            {
                progress_state->remaining_queued_kb = 0;
            }
        }

        return returnCode;
    }

    files_downloaded = files_downloaded + 1;
    WHDLoadPackDefs->count_new_files_downloaded = WHDLoadPackDefs->count_new_files_downloaded + 1;

    {
        game_metadata metadata = {0};
        char old_archive_name[256] = {0};
        const char *old_archive = NULL;

        extract_game_info_from_filename(downloadWHDFile, &metadata);
        if (metadata.title[0] != '\0' &&
            extract_index_find_update_candidate(downloadWHDFile,
                                                old_archive_name,
                                                sizeof(old_archive_name)))
        {
            old_archive = old_archive_name;
        }

        report_add(downloadWHDFile, WHDLoadPackDefs->full_text_name_of_pack, old_archive);
    }

    if (download_options != NULL && download_options->extract_archives == TRUE)
    {
        const char *skip_reason = NULL;

        extractCode = extract_process_downloaded_archive(fileName,
                                                         downloadWHDFile,
                                                         WHDLoadPackDefs->extracted_pack_dir,
                                                         downloadFirstLetter,
                                                         WHDLoadPackDefs->full_text_name_of_pack,
                                                         download_options);

        skip_reason = extract_skip_reason_from_code(extractCode);
        if (skip_reason != NULL)
        {
            report_add_extraction_skip(downloadWHDFile,
                                       WHDLoadPackDefs->full_text_name_of_pack,
                                       skip_reason);
        }

        if (extractCode != EXTRACT_RESULT_OK && !is_nonfatal_extract_skip(extractCode))
        {
            log_warning(LOG_GENERAL,
                        "extract: post-download extraction failed for '%s' (code=%ld), continuing\n",
                        downloadWHDFile,
                        (long)extractCode);
        }
    }

    if (progress_state != NULL && archive_size_kb > 0)
    {
        if (progress_state->remaining_queued_kb >= archive_size_kb)
        {
            progress_state->remaining_queued_kb -= archive_size_kb;
        }
        else
        {
            progress_state->remaining_queued_kb = 0;
        }
    }

    return returnCode;
}

void Format_text_split_by_Caps(const char *original, char *buffer, size_t buffer_len)
{

    const char *ptr = original;
    size_t buf_index = 0;

    if (original == NULL || buffer == NULL || buffer_len == 0)
        return;

    /* Ensure the buffer starts clean */
    memset(buffer, 0, buffer_len);

    /* Process each character in the original string */
    while (*ptr != '\0' && buf_index < buffer_len - 1)
    {
        /* Stop processing if a '.' or '_' is found */
        if (*ptr == '.' || *ptr == '_')
            break;

        /* Check for capital letters or '&' and add a space if not the first character */
        if ((isupper(*ptr) || *ptr == '&') && ptr != original)
        {
            if (buf_index < buffer_len - 2)
            {
                buffer[buf_index++] = ' ';
            }
        }

        /* Copy the current character into the buffer */
        buffer[buf_index++] = *ptr++;

        /* Ensure we do not exceed buffer length */
        if (buf_index == buffer_len - 1)
            break;
    }

    /* Null-terminate the buffer */
    buffer[buf_index] = '\0';
}

void turn_filename_into_text_with_spaces(const char *filename, char *outputArray)
{
    int i = 0, j = 0;
    int startAppending = 0;

    while (filename[i] != '\0')
    {
        if (filename[i] == '_')
        {
            /* Check if the next sequence is ".l" or ".L" before starting appending */
            if (filename[i + 1] == '.' && (filename[i + 2] == 'l' || filename[i + 2] == 'L'))
            {
                break; /* Stop appending when ".l" or ".L" is immediately after an underscore */
            }
            /* Start appending after an underscore */
            if (startAppending > 0)
            {
                outputArray[j] = ' ';
                j++;
            }
            startAppending = 1;
        }
        else if (startAppending)
        {
            /* Stop appending if ".l" or ".L" is found during the appending process */
            if (filename[i] == '.' && (filename[i + 1] == 'l' || filename[i + 1] == 'L'))
            {
                break;
            }
            outputArray[j++] = filename[i];
        }
        i++;
    }
    outputArray[j] = '\0'; /* Null terminate the output array */
}

void create_directory_based_on_filename(const char *parentDir, const char *fileName)
{
    size_t dirLen = strlen(parentDir);

    char downloadFirstLetter[2];
    char fileStore[256];
    char cleanParentDir[256];
    strcpy(cleanParentDir, parentDir);

    /* Check if the last character is a backslash and remove it */
    if (cleanParentDir[dirLen - 1] == '/')
    {
        cleanParentDir[dirLen - 1] = '\0';
    }
    /* Now directory is safe to use with create_Directory_and_unlock */
    create_Directory_and_unlock(DIR_GAME_DOWNLOADS);
    sprintf(fileStore, "%s/%s", DIR_GAME_DOWNLOADS, cleanParentDir);
    sanitize_amiga_file_path(fileStore);

    create_Directory_and_unlock(fileStore);
    sprintf(downloadFirstLetter, "%c", fileName[0]);
    get_folder_name_from_character(downloadFirstLetter);
    sprintf(fileStore, "%s/%s/%s", DIR_GAME_DOWNLOADS, cleanParentDir, downloadFirstLetter);
    sanitize_amiga_file_path(fileStore);

    create_Directory_and_unlock(fileStore);
}

void get_folder_name_from_character(char *c)
{
    if (isalpha(*c))
    {
        /* If *c is alphabetical, convert it to uppercase */
        *c = toupper(*c);
    }
    else if (isdigit(*c))
    {
        /* If *c is numerical (0-9), convert it to '0' */
        *c = '0';
    }
    else
    {
        /* For any other character, set *c to '0' */
        *c = '0';
    }
}

int extract_date_from_filename(const char *filename, char *buffer, int bufSize)
{
    /* Pointer to start of date in the filename */
    char *start = NULL;
    /* Pointer to end of date in the filename */
    char *end = NULL;

    /* Look for the opening parenthesis of the date format */
    start = strchr(filename, '(');
    if (start != NULL)
    {
        /* Look for the closing parenthesis following the opening parenthesis */
        end = strchr(start, ')');
        if (end != NULL && (end - start - 1 == 10) && bufSize >= 11)
        { /* Ensure the length and buffer size are adequate */
            /* Copy the date from the filename to the buffer */
            strncpy(buffer, start + 1, end - start - 1);
            /* Null terminate the string */
            buffer[end - start - 1] = '\0';
            return 1; /* Success */
        }
    }
    /* If no valid date format is found, or buffer is too small, return 0 */
    return 0;
}

long convert_string_date_to_int(const char *date)
{
    int year, month, day;

    if (date == NULL)
    {
        return 0;
    }

    /* Expected input format from extract_date_from_filename(): YYYY-MM-DD */
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3)
    {
        return 0;
    }

    return (long)(year * 10000 + month * 100 + day);
}

int compare_dates_greater_then_date2(const char *date1, const char *date2)
{
    long date1Int = convert_string_date_to_int(date1);
    long date2Int = convert_string_date_to_int(date2);

    return (date2Int > date1Int) ? 1 : 0;
}

void create_day_with_suffix(int day, char *buffer)
{
    switch (day)
    {
    case 1:
    case 21:
    case 31:
        sprintf(buffer, "%dst", day);
        break;
    case 2:
    case 22:
        sprintf(buffer, "%dnd", day);
        break;
    case 3:
    case 23:
        sprintf(buffer, "%drd", day);
        break;
    default:
        sprintf(buffer, "%dth", day);
        break;
    }
}

const char *get_month_name(int month)
{
    const char *months[] = {
        "January", "February", "March",
        "April", "May", "June",
        "July", "August", "September",
        "October", "November", "December"};
    if (month < 1 || month > 12)
    {
        return "Unknown";
    }
    return months[month - 1];
}

void convert_date_to_long_style(const char *date, char *result)
{
    int day, month, year;
    char daySuffix[5]; // Buffer for the day with ordinal suffix
    const char *monthName;

    if (date == NULL || result == NULL)
    {
        return;
    }

    // Parse the date
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3)
    {
        strcpy(result, "Unknown date");
        return;
    }

    // Convert day to ordinal suffix form
    create_day_with_suffix(day, daySuffix);

    // Get the month name
    monthName = get_month_name(month);

    // Format the final result
    sprintf(result, "%s %s %d", daySuffix, monthName, year);
}

int Get_latest_filename_from_directory(const char *directory, const char *searchText, char *latestFileName)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    char dateStr[11];
    long newestDate = 0;
    char newestFile[256] = {0};
    char fullPath[512]; // Buffer for full path
    int found = 0;

    if (!(lock = Lock(directory, ACCESS_READ)))
    {
        printf("Failed to lock directory: %s\n", directory);
        return 0;
    }

    if (!(fib = AllocDosObject(DOS_FIB, NULL)))
    {
        UnLock(lock);
        printf("Failed to allocate FileInfoBlock\n");
        return 0;
    }

    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {

            if (fib->fib_DirEntryType < 0 && strstr(fib->fib_FileName, searchText))
            {
                extract_date_from_filename(fib->fib_FileName, dateStr, sizeof(dateStr));
                if (strlen(dateStr) > 1)
                {

                    long fileDate = convert_string_date_to_int(dateStr);
                    if (fileDate > newestDate)
                    {
                        newestDate = fileDate;
                        strcpy(newestFile, fib->fib_FileName);
                        found = 1;
                    }
                }
            }
        }
    }
    else
    {
        printf("Failed to examine directory: %s\n", directory);
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return 0;
    }

    /* Second pass: Delete older files if necessary */
    if (found)
    {
        strcpy(latestFileName, newestFile);
        sprintf(fullPath, "%s/%s", directory, newestFile); // Save full path of the newest file
        if (Examine(lock, fib))
        {
            while (ExNext(lock, fib))
            {
                if (fib->fib_DirEntryType < 0 && strstr(fib->fib_FileName, searchText) && strcmp(fib->fib_FileName, newestFile) != 0)
                {
                    if (extract_date_from_filename(fib->fib_FileName, dateStr, sizeof(dateStr)))
                    {
                        long fileDate = convert_string_date_to_int(dateStr);
                        if (fileDate < newestDate)
                        {
                            sprintf(fullPath, "%s/%s", directory, fib->fib_FileName); // Full path for deletion
                            DeleteFile(fullPath);                                     // Delete using full path
                        }
                    }
                }
            }
        }
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return found; // Return 1 if successful, 0 otherwise
}

BOOL is_file_locked(const char *filePath)
{
    // Attempt to obtain a shared lock on the file
    BPTR lock = Lock(filePath, ACCESS_READ);

    if (lock)
    {
        // Successfully obtained a lock, file is not locked by another process
        UnLock(lock);
    }
    else
    {
        // Failed to obtain a lock, it could be locked or does not exist
        return TRUE;
    }
}

/**
 * @brief Appends or writes text to a file.
 *
 * Opens the target file in either new or read-write mode, optionally deletes
 * any existing file, writes the provided text, and closes the file.
 *
 * @param target_filename Path of the file to write to.
 * @param append_text     Text to append to the file.
 * @param create_new_file If TRUE, a new file is created; otherwise text
 *                        is appended to the existing file.
 *
 * @return TRUE on success, FALSE otherwise.
 */
BOOL append_string_to_file(const char *target_filename, const char *append_text, BOOL create_new_file)
{
    /* Declare variables */
    BPTR file_handle;
    LONG file_open_mode = (create_new_file == 1) ? MODE_NEWFILE : MODE_READWRITE;
    size_t textLength = strlen(append_text);

    /* If create_new_file is 1, try to delete any existing file */
    {
        if (create_new_file == 1)
        {
            if (!DeleteFile(target_filename))
            {
                printf("Error: Unable to delete existing file %s. IoErr: %ld\n", target_filename, IoErr());
                return FALSE;
            }
        }
    }

    /* Determine if we need to add a newline at the end */
    {
        BOOL needsNewline = (append_text != NULL && textLength > 0 && append_text[textLength - 1] != '\n');

        /* Open the file in new or read/write mode */
        file_handle = Open((CONST_STRPTR)target_filename, file_open_mode);
        if (!file_handle)
        {
            printf("Error: Unable to open file %s. IoErr: %ld\n", target_filename, IoErr());
            return FALSE;
        }

        /* If not creating a new file, seek to the end for appending */
        if (!create_new_file)
        {
            Seek(file_handle, 0, OFFSET_END);
        }

        /* Write the provided text */
        if (FPuts(file_handle, (CONST_STRPTR)append_text) == EOF)
        {
            Close(file_handle);
            return FALSE;
        }

        /* Optionally add a newline */
        if (needsNewline)
        {
            if (FPuts(file_handle, "\n") == EOF)
            {
                Close(file_handle);
                printf("Error: Unable to write newline to file %s. IoErr: %ld\n", target_filename, IoErr());
                return FALSE;
            }
        }

        /* Close the file after writing */
        Close(file_handle);
    }

    /* Return success */
    return TRUE;
}

void delete_all_files_in_dir(const char *directory)
{
    struct FileInfoBlock *file_info_block;
    BPTR directory_lock;
    BOOL do_more_entries_exist;
    char clean_path_buffer[256];
    strcpy(clean_path_buffer, directory);
    sanitize_amiga_file_path(clean_path_buffer);

    /* Allocate memory for FileInfoBlock */
    if (!(file_info_block = AllocDosObject(DOS_FIB, NULL)))
    {
        printf("Failed to allocate memory for FileInfoBlock\n");
        return;
    }

    /* Try to lock the directory */
    if (!(directory_lock = Lock(clean_path_buffer, ACCESS_READ)))
    {
        printf("Failed to lock the directory: %s\n", clean_path_buffer);
        printf("Error message: (%ld) \n", (long)IoErr());
        FreeDosObject(DOS_FIB, file_info_block);
        return;
    }

    /* Examine the directory */
    if (!Examine(directory_lock, file_info_block))
    {
        printf("Failed to examine the directory: %s\n", clean_path_buffer);
        UnLock(directory_lock);
        FreeDosObject(DOS_FIB, file_info_block);
        return;
    }

    /* Iterate through the directory */
    do_more_entries_exist = ExNext(directory_lock, file_info_block);
    while (do_more_entries_exist)
    {
        /* Check if it is a file */
        if (file_info_block->fib_DirEntryType < 0)
        {
            /* Build the full path for the file */
            char filePath[256];
            sprintf(filePath, "%s/%s", clean_path_buffer, file_info_block->fib_FileName);

            /* Delete the file */
            if (!DeleteFile(filePath))
            {
                printf("Failed to delete file: %s\n", filePath);
            }
        }
        do_more_entries_exist = ExNext(directory_lock, file_info_block);
    }

    /* Check for errors from ExNext */
    if (IoErr() != ERROR_NO_MORE_ENTRIES)
    {
        printf("Error reading directory\n");
    }

    /* Clean up */
    UnLock(directory_lock);
    FreeDosObject(DOS_FIB, file_info_block);
}

/**
 * @brief Sanitizes an Amiga file path for safety and compatibility
 *
 * This function processes an Amiga path string to ensure it follows proper format:
 * - Removes redundant slashes (multiple consecutive slashes are reduced to one)
 * - After a colon (device separator), all immediate slashes are reduced to none
 * - Removes leading and trailing whitespace
 * - Removes carriage returns and line feeds
 * - Replaces illegal Amiga filename characters with underscores
 * - Handles trailing slashes consistently
 *
 * @param path Pointer to the path string to sanitize (modified in-place)
 */
void sanitize_amiga_file_path(char *path)
{
    ULONG path_len;        /* Length of the input path */
    char *sanitized_path;  /* Temporary buffer for the sanitized path */
    int src_index;         /* Index for reading from the source path */
    int dest_index;        /* Index for writing to the sanitized path */
    int after_colon;       /* Flag to track position after device colon */
    
    /* Handle null pointer */
    if (path == NULL)
    {
        return;
    }

    /* Calculate length and allocate memory for sanitized path */
    path_len = strlen(path) + 1; /* Include null terminator */
    sanitized_path = (char *)amiga_malloc(path_len);
    if (sanitized_path == NULL)
    {
        /* Memory allocation failed */
        return;
    }

    /* Process the path character by character */
    src_index = 0;
    dest_index = 0;
    after_colon = 0;
    
    while (path[src_index] != '\0')
    {
        /* Handle colon (device separator) */
        if (path[src_index] == ':')
        {
            sanitized_path[dest_index++] = path[src_index++];
            after_colon = 1;
            
            /* Skip all following slashes after a colon */
            while (path[src_index] == '/')
            {
                src_index++;
            }
        }
        /* Handle consecutive slashes */
        else if (path[src_index] == '/' && path[src_index + 1] == '/')
        {
            /* Skip redundant slashes */
            src_index++;
        }
        /* Handle illegal Amiga filename characters */
        else if (strchr("*?#|<>\"{}", path[src_index]) != NULL)
        {
            /* Replace illegal characters with underscore */
            sanitized_path[dest_index++] = '_';
            src_index++;
        }
        /* Handle control characters (ASCII 0-31) */
        else if ((unsigned char)path[src_index] < 32)
        {
            /* Skip control characters */
            src_index++;
        }
        /* Copy other valid characters */
        else
        {
            sanitized_path[dest_index++] = path[src_index++];
            after_colon = 0;
        }
    }

    /* Remove trailing slash if present */
    if (dest_index > 0 && sanitized_path[dest_index - 1] == '/')
    {
        dest_index--;
    }

    /* Ensure the result is null-terminated */
    sanitized_path[dest_index] = '\0';

    /* Clean up the string and copy back to the original path */
    remove_CR_LF_from_string(sanitized_path);
    trim(sanitized_path);
    strcpy(path, sanitized_path);
    
    /* Free the allocated memory */
    amiga_free(sanitized_path);
}

static BOOL is_existing_directory(const char *path)
{
    BPTR existing_lock;
    struct FileInfoBlock *fib;
    BOOL is_directory = FALSE;

    if (path == NULL || path[0] == '\0')
    {
        return FALSE;
    }

    existing_lock = Lock(path, ACCESS_READ);
    if (!existing_lock)
    {
        return FALSE;
    }

    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL)
    {
        if (Examine(existing_lock, fib))
        {
            if (fib->fib_DirEntryType >= 0)
            {
                is_directory = TRUE;
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }

    UnLock(existing_lock);
    return is_directory;
}

BOOL create_Directory_and_unlock(const char *dirName)
{
    char path_copy[512];
    char *slash;
    BPTR lock;

    if (dirName == NULL || dirName[0] == '\0')
    {
        log_error(LOG_GENERAL, "dir: invalid directory name\n");
        return FALSE;
    }

    if (is_existing_directory(dirName))
    {
        log_debug(LOG_GENERAL, "dir: already exists '%s'\n", dirName);
        return TRUE;
    }

    /* Create each path component so nested paths work reliably. */
    strncpy(path_copy, dirName, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    slash = strchr(path_copy, '/');
    while (slash != NULL)
    {
        BPTR step_lock;

        *slash = '\0';
        if (path_copy[0] != '\0' && does_file_or_folder_exist(path_copy, 0) == 0)
        {
            step_lock = CreateDir(path_copy);
            if (step_lock)
            {
                UnLock(step_lock);
                log_info(LOG_GENERAL, "dir: created intermediate '%s'\n", path_copy);
            }
            else
            {
                LONG err = IoErr();
                if (err != ERROR_OBJECT_EXISTS)
                {
                    if (is_existing_directory(path_copy))
                    {
                        log_debug(LOG_GENERAL, "dir: intermediate already exists '%s'\n", path_copy);
                        *slash = '/';
                        slash = strchr(slash + 1, '/');
                        continue;
                    }

                    log_error(LOG_GENERAL, "dir: failed to create intermediate '%s' (IoErr=%ld)\n", path_copy, (long)err);
                    return FALSE;
                }
                log_debug(LOG_GENERAL, "dir: intermediate already exists '%s'\n", path_copy);
            }
        }
        *slash = '/';
        slash = strchr(slash + 1, '/');
    }

    lock = CreateDir(dirName);
    if (lock)
    {
        UnLock(lock);
        log_info(LOG_GENERAL, "dir: created '%s'\n", dirName);
        return TRUE;
    }
    else
    {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_EXISTS)
        {
            log_debug(LOG_GENERAL, "dir: already exists '%s'\n", dirName);
            return TRUE;
        }

        if (is_existing_directory(dirName))
        {
            log_debug(LOG_GENERAL, "dir: already exists '%s'\n", dirName);
            return TRUE;
        }

        log_error(LOG_GENERAL, "dir: failed to create '%s' (IoErr=%ld)\n", dirName, (long)err);
        return FALSE;
    }
}

void remove_all_occurrences(char *source_string, const char *toRemove)
{
    int lenToRemove = strlen(toRemove);
    char *p = strstr(source_string, toRemove); // Find the start of the substring to remove
    while (p != NULL)
    {
        *p = '\0';                              // Temporarily terminate the string at the start of the substring to remove
        strcat(source_string, p + lenToRemove); // Concatenate the remaining part of the string
        p = strstr(source_string, toRemove);    // Look for the substring again, in case it appears multiple times
    }
}

char *concatStrings(const char *s1, const char *s2)
{
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    char *result = amiga_malloc(len1 + len2 + 1);

    if (result == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void ensure_time_zone_set(void)
{
    /* The unzip program displays a warning message if the time zone
    variable is not set correctly.  If it's not set, this code sets it
    for the local session only  */
    CONST_STRPTR name = "TZ";
    CONST_STRPTR value = "GMT0BST,M3.5.0/01,M10.5.0/02"; /* UK Timezone */
    LONG size = strlen(value) + 1;                       /* include null terminator */
    LONG flags = GVF_GLOBAL_ONLY;
    char *tz = getenv("TZ");
    // printf("TZ: %s\n", tz);
    // printf("Size: %d\n", strlen(tz));

    if (strlen(tz) == 0)
    {

        SetVar(name, value, size, flags);
    }
}

int get_bsdSocket_version(void)
{
    /* Attempt to open the bsdsocket.library */
    int version = 0;
    RP_Socket_Base = OpenLibrary("bsdsocket.library", 0);
    if (RP_Socket_Base == NULL)
    {
        return -1; /* Indicates that the library is not open, possibly no internet connection.*/
    }

    version = RP_Socket_Base->lib_Version;
    CloseLibrary(RP_Socket_Base); /* Always close the library after opening it.*/

    return version;
}

/**
 * @brief Extracts version information from an executable file
 *
 * Searches through a file for "$VER:" tag and extracts the version information
 * that follows it. Returns the version string without the "$VER:" prefix.
 * If the file is not found or cannot be opened, returns an appropriate error message.
 *
 * @param file_path Path to the executable file to examine
 * @return Allocated string containing the version information or error message
 *         (Caller is responsible for freeing this memory with FreeVec)
 */
char *get_executable_version(const char *file_path)
{
    BPTR file;                /* File handle */
    UBYTE buffer[BUFFER_SIZE]; /* Buffer for reading file content */
    char *version = NULL;     /* Temporary storage for full version string */
    char *clean_version = NULL; /* Final version string without prefix */
    char *error_msg = NULL;   /* Error message if something fails */
    LONG bytes_read;          /* Number of bytes read from file */
    int i;                    /* Loop index for searching buffer */
    int start, end;           /* Start and end positions of version string */
    int version_len;          /* Length of full version string */
    int clean_len;            /* Length of version string without prefix */

    /* Tag length for "$VER:" marker scanning */
    const int tag_len = 5;

    /* Check if file exists */
    if (!does_file_or_folder_exist(file_path, 0))
    {
        error_msg = amiga_malloc(128);
        if (error_msg != NULL)
        {
            sprintf(error_msg, "[File not found: %s]", file_path);
        }
        return error_msg;
    }

    /* Open the file for reading */
    file = Open((CONST_STRPTR)file_path, MODE_OLDFILE);
    if (!file)
    {
        error_msg = amiga_malloc(64);
        if (error_msg != NULL)
        {
            strcpy(error_msg, "[Unable to open file]");
        }
        return error_msg;
    }

    /* Read the file in chunks and search for version tag */
    while ((bytes_read = Read(file, buffer, BUFFER_SIZE)) > 0)
    {
        /* Search each position in the buffer for tag match */
        for (i = 0; i <= bytes_read - tag_len; ++i)
        {
            if (buffer[i] == '$' &&
                buffer[i + 1] == 'V' &&
                buffer[i + 2] == 'E' &&
                buffer[i + 3] == 'R' &&
                buffer[i + 4] == ':')
            {
                /* Found the version tag - mark start position */
                start = i;
                
                /* Find the end of the version string (printable ASCII chars) */
                end = i + tag_len;
                while (end < bytes_read && buffer[end] >= 0x20 && buffer[end] <= 0x7E)
                {
                    ++end;
                }

                /* Calculate lengths and allocate memory */
                version_len = end - start;
                version = amiga_malloc(version_len + 1);
                
                if (version != NULL)
                {
                    /* Copy the full version string */
                    CopyMem(&buffer[start], version, version_len);
                    version[version_len] = '\0';

                    /* Extract just the version part (without $VER: prefix) */
                    if (version_len > tag_len)
                    {
                        clean_len = version_len - tag_len;
                        clean_version = amiga_malloc(clean_len + 1);
                        
                        if (clean_version != NULL)
                        {
                            CopyMem(version + tag_len, clean_version, clean_len);
                            clean_version[clean_len] = '\0';
                        }
                    }

                    amiga_free(version);
                }

                Close(file);
                return clean_version;
            }
        }
    }

    /* Close the file since we're done with it */
    Close(file);

    /* No version info found */
    error_msg = amiga_malloc(64);
    if (error_msg != NULL)
    {
        strcpy(error_msg, "[No version info found]");
    }
    return error_msg;
}
