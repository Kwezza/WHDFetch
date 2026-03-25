#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#include "startup/startup.h"
#include "build_version.h"
#include "log/log.h"
#include "tag_text.h"
#include "utilities.h"

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

BOOL validate_extraction_startup_configuration(const download_option *download_options)
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

void log_effective_configuration(const whdload_pack_def *pack_defs,
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
             "config[%s]: options no_skip=%ld verbose=%ld disable_counters=%ld crc_check=%ld estimate_space=%ld extract=%ld extract_only=%ld skip_existing=%ld force_extract=%ld skip_download=%ld verify_marker=%ld force_download=%ld extract_path='%s' delete_archives=%ld purge_archives=%ld skip_aga=%ld skip_cd=%ld skip_ntsc=%ld skip_non_english=%ld use_custom_icons=%ld unsnapshot_icons=%ld\n",
             stage,
             (long)download_options->no_skip_messages,
             (long)download_options->verbose_output,
             (long)download_options->disable_counters,
             (long)download_options->crc_check,
             (long)download_options->estimate_space,
             (long)download_options->extract_archives,
             (long)download_options->extract_existing_only,
             (long)download_options->skip_existing_extractions,
             (long)download_options->force_extract,
             (long)download_options->skip_download_if_extracted,
             (long)download_options->verify_archive_marker_before_download,
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

BOOL startup_text_and_needed_progs_are_installed(int number_of_args)
{
    int bsd_version = get_bsdSocket_version();
    char *versionInfo;
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
            add_line(&tb, "");
            add_line(&tb, "  <b>Planning:</b>");
            add_line(&tb, "  ESTIMATESPACE/S<ex23>Apply filters and print space estimates without downloading archives");
            add_line(&tb, "");
            add_line(&tb, "  <b>Filtering:</b>");
            add_line(&tb, "  SKIPAGA/S<ex23>Skip AGA packages");
            add_line(&tb, "  SKIPCD/S<ex23>Skip CDTV/CD32 packages");
            add_line(&tb, "  SKIPNTSC/S<ex23>Skip NTSC packages");
            add_line(&tb, "  SKIPNONENGLISH/S<ex23>Skip non-English packages");
            add_line(&tb, "");
            add_line(&tb, "  <b>Extraction:</b>");
            add_line(&tb, "  EXTRACTTO/K<ex23>Extract archives to a separate target path");
            add_line(&tb, "  NOEXTRACT/S<ex23>Disable post-download archive extraction");
            add_line(&tb, "  EXTRACTONLY/S<ex23>Extract already-downloaded archives only");
            add_line(&tb, "  KEEPARCHIVES/S<ex23>Keep downloaded archives after extraction");
            add_line(&tb, "  DELETEARCHIVES/S<ex23>Delete downloaded archives after extraction");
            add_line(&tb, "  FORCEEXTRACT/S<ex23>Always extract even when ArchiveName.txt matches");
            add_line(&tb, "");
            add_line(&tb, "  <b>Download Skip:</b>");
            add_line(&tb, "  NODOWNLOADSKIP/S<ex23>Disable marker-based pre-download skip");
            add_line(&tb, "  VERIFYMARKER/S<ex23>Verify ArchiveName.txt before skipping a download");
            add_line(&tb, "  FORCEDOWNLOAD/S<ex23>Always download archives, bypassing pre-download skip checks");
            add_line(&tb, "");
            add_line(&tb, "  <b>Reporting and diagnostics:</b>");
            add_line(&tb, "  NOSKIPREPORT/S<ex23>Don't report skipped existing archives");
            add_line(&tb, "  ENABLELOGGING/S<ex23>Enable log file output (disabled by default)");
            add_line(&tb, "  VERBOSE/S<ex23>Show more detailed output during processing");
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
            add_line(&tb, "</b>-<ex04>A fast Amiga setup is recommended for large downloads.");
            add_line(&tb, "</b>-<ex04>On WinUAE, entire sets can be downloaded in under an hour.");
            add_line(&tb, "</b>-<ex04>On real Amiga hardware, speed may vary depending on CPU and network connectivity.");
            add_line(&tb, "</b>-<ex04>Archive extraction can be configured with NOEXTRACT, EXTRACTTO, KEEPARCHIVES, and DELETEARCHIVES.");
            add_line(&tb, "</b>-<ex04>PURGEARCHIVES removes downloaded .lha and .lzx files under GameFiles/ after confirmation.");
            add_line(&tb, "</b>-<ex04>By default, extraction is skipped when ArchiveName.txt line 2 exactly matches the archive filename.");
            add_line(&tb, "</b>-<ex04>By default, pre-download skip uses fast checks based on folder matching and .archive_index cache data.");
            add_line(&tb, "</b>-<ex04>Use VERIFYMARKER to verify ArchiveName.txt before skipping a download.");
            add_line(&tb, "</b>-<ex04>Use FORCEEXTRACT to bypass the extraction skip check and always re-extract.");
            add_line(&tb, "</b>-<ex04>Use FORCEDOWNLOAD to bypass pre-download skip and always fetch archives.");
            add_line(&tb, "</b>-<ex04>Use EXTRACTONLY to process archives already present in GameFiles/.");
            add_line(&tb, "</b>-<ex04>PURGEARCHIVES keeps extracted folders intact; only archive files are deleted.");
            add_line(&tb, "</b>-<ex04>Project page: github.com/Kwezza/WHDFetch");

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
