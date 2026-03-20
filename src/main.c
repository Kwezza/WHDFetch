/*
Retroplay WHD Downloader (whdfetch)

Small CLI tool for downloading and extracting Retroplay WHDLoad packs.
No GUI. Shell only.

Flow
====
1) Download index.html from the Retroplay pack site.
2) Parse pack links and download DAT ZIP files.
3) Extract DAT/XML and read <rom name="..."> entries.
4) Apply optional filters: AGA, CD, NTSC, non-English.
5) Download ROM archives to GameFiles/<pack>/<letter>/.
6) Extract with c:lha, write ArchiveName.txt, update .archive_index,
     and optionally replace icons.

Requires
========
- AmigaOS 3.0+ (3.1+ recommended).
- bsdsocket.library-compatible TCP/IP stack (Roadshow tested).
- c:lha in path.

Notes
=====
- DAT parsing and filesystem scans are heavy on slower machines.
- Config order: defaults -> INI -> CLI.
    INI files: whdfetch.ini, legacy WHDDownloader.ini.
- Build target: VBCC (+aos68k, C99).
- Other TCP/IP stacks are not well tested.
*/
#include "main.h"
#include "cli/cli_parser.h"
#include "config/app_defaults.h"
#include "utilities.h"
#include "ini_parser.h"
#include "lifecycle/lifecycle.h"
#include "startup/startup.h"
#include "tag_text.h"
#include "phases/dat_phase.h"
#include "phases/download_phase.h"
#include "phases/html_phase.h"
#include "report/report.h"
#include "download/amiga_download.h"
#include "extract/extract.h"
#include "platform/platform.h"
#include "log/log.h"

#if defined(__AMIGA__)
/* VBCC stack override: helps avoid late-exit crashes from stack exhaustion. */
ULONG __stack = 131072;
#endif

/* Track whether the download library was successfully initialized so that
 * do_shutdown() only calls ad_cleanup_download_lib() when appropriate. */
static BOOL g_download_lib_initialized = FALSE;

#define do_shutdown() lifecycle_do_shutdown(&g_download_lib_initialized)

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

    if (cli_is_argument_present(argc, argv, "PURGEARCHIVES"))
    {
        if (!lifecycle_confirm_purge_archives())
        {
            do_shutdown();
            return 0;
        }

        if (!lifecycle_purge_downloaded_archives())
        {
            do_shutdown();
            return RETURN_FAIL;
        }

        do_shutdown();
        return 0;
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
    has_cli_pack_selection = cli_has_pack_selection(argc, argv) ? 1 : 0;

    if (has_cli_pack_selection)
    {
        for (i = 0; i < 5; i++)
        {
            whdload_pack_defs[i].user_requested_download = 0;
        }
    }

    if (cli_is_argument_present(argc, argv, "HELP"))
    {
        startup_text_and_needed_progs_are_installed(0);
        log_info(LOG_GENERAL, "main: HELP requested, exiting\n");
        do_shutdown();
        return 0;
    }

    /* Process command line arguments */
    cli_apply_arguments(argc, argv, whdload_pack_defs, &download_options);

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
    html_phase_extract_and_validate_links(whdload_pack_defs, 5, DIR_TEMP, DIR_ZIP_FILES);

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

