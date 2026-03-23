#include "platform/platform.h"

#include <string.h>

#include "cli/cli_parser.h"
#include "log/log.h"
#include "utilities.h"

static void apply_timeout_argument(download_option *options, const char *value)
{
    ULONG parsed_value;
    ULONG raw_value;

    if (options == NULL || value == NULL)
    {
        return;
    }

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

BOOL cli_is_argument_present(int argc, char *argv[], const char *argument)
{
    int i;
    size_t argument_length;

    if (argv == NULL || argument == NULL)
    {
        return FALSE;
    }

    argument_length = strlen(argument);
    for (i = 0; i < argc; i++)
    {
        if (strncasecmp_custom(argv[i], argument, argument_length) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL cli_has_pack_selection(int argc, char *argv[])
{
    int i;

    if (argv == NULL)
    {
        return FALSE;
    }

    for (i = 0; i < argc; i++)
    {
        if (strncasecmp_custom(argv[i], "DOWNLOADGAMES", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADBETAGAMES", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADDEMOS", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADBETADEMOS", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADMAGS", strlen(argv[i])) == 0 ||
            strncasecmp_custom(argv[i], "DOWNLOADALL", strlen(argv[i])) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

void cli_apply_arguments(int argc,
                         char *argv[],
                         struct whdload_pack_def whdload_pack_defs[],
                         struct download_option *download_options)
{
    int i;

    if (argv == NULL || whdload_pack_defs == NULL || download_options == NULL)
    {
        return;
    }

    for (i = 0; i < argc; i++)
    {
        if (strncasecmp_custom(argv[i], "DOWNLOADGAMES", strlen(argv[i])) == 0)
            whdload_pack_defs[GAMES].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "SKIPAGA", strlen(argv[i])) == 0)
            skip_AGA = 1;

        if (strncasecmp_custom(argv[i], "SKIPCD", strlen(argv[i])) == 0)
            skip_CD = 1;

        if (strncasecmp_custom(argv[i], "SKIPNTSC", strlen(argv[i])) == 0)
            skip_NTSC = 1;

        if (strncasecmp_custom(argv[i], "SKIPNONENGLISH", strlen(argv[i])) == 0)
            skip_NonEnglish = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADBETAGAMES", strlen(argv[i])) == 0)
            whdload_pack_defs[GAMES_BETA].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADDEMOS", strlen(argv[i])) == 0)
            whdload_pack_defs[DEMOS].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADBETADEMOS", strlen(argv[i])) == 0)
            whdload_pack_defs[DEMOS_BETA].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADMAGS", strlen(argv[i])) == 0)
            whdload_pack_defs[MAGAZINES].user_requested_download = 1;

        if (strncasecmp_custom(argv[i], "DOWNLOADALL", strlen(argv[i])) == 0)
        {
            whdload_pack_defs[GAMES].user_requested_download = 1;
            whdload_pack_defs[GAMES_BETA].user_requested_download = 1;
            whdload_pack_defs[DEMOS].user_requested_download = 1;
            whdload_pack_defs[DEMOS_BETA].user_requested_download = 1;
            whdload_pack_defs[MAGAZINES].user_requested_download = 1;
        }

        if (strncasecmp_custom(argv[i], "NOSKIPREPOR", strlen(argv[i])) == 0)
            download_options->no_skip_messages = 1;

        if (strncasecmp_custom(argv[i], "VERBOSE", strlen(argv[i])) == 0)
            download_options->verbose_output = 1;

        if (strncasecmp_custom(argv[i], "ENABLELOGGING", strlen(argv[i])) == 0)
            download_options->enable_logging = TRUE;

        if (strncasecmp_custom(argv[i], "NOEXTRACT", strlen(argv[i])) == 0)
            download_options->extract_archives = FALSE;

        if (strncasecmp_custom(argv[i], "EXTRACTTO=", 10) == 0)
        {
            const char *extract_target = argv[i] + 10;
            if (extract_target[0] == '\0')
            {
                download_options->extract_path = NULL;
            }
            else
            {
                download_options->extract_path = extract_target;
            }
        }

        if (strncasecmp_custom(argv[i], "KEEPARCHIVES", strlen(argv[i])) == 0)
            download_options->delete_archives_after_extract = FALSE;

        if (strncasecmp_custom(argv[i], "DELETEARCHIVES", strlen(argv[i])) == 0)
            download_options->delete_archives_after_extract = TRUE;

        if (strncasecmp_custom(argv[i], "EXTRACTONLY", strlen(argv[i])) == 0)
        {
            download_options->extract_existing_only = 1;
            download_options->extract_archives = TRUE;
        }

        if (strncasecmp_custom(argv[i], "FORCEEXTRACT", strlen(argv[i])) == 0)
            download_options->force_extract = TRUE;

        if (strncasecmp_custom(argv[i], "NODOWNLOADSKIP", strlen(argv[i])) == 0)
            download_options->skip_download_if_extracted = FALSE;

        if (strncasecmp_custom(argv[i], "VERIFYMARKER", strlen(argv[i])) == 0)
            download_options->verify_archive_marker_before_download = TRUE;

        if (strncasecmp_custom(argv[i], "FORCEDOWNLOAD", strlen(argv[i])) == 0)
            download_options->force_download = TRUE;

        if (strncasecmp_custom(argv[i], "PURGEARCHIVES", strlen(argv[i])) == 0)
            download_options->purge_archives = TRUE;

        if (strncasecmp_custom(argv[i], "NOICONS", strlen(argv[i])) == 0)
            download_options->use_custom_icons = FALSE;

        if (strncasecmp_custom(argv[i], "DISABLECOUNTERS", strlen(argv[i])) == 0)
            download_options->disable_counters = TRUE;

        if (strncasecmp_custom(argv[i], "CRCCHECK", strlen(argv[i])) == 0)
            download_options->crc_check = TRUE;

        if (strncasecmp_custom(argv[i], "ESTIMATESPACE", strlen(argv[i])) == 0)
            download_options->estimate_space = TRUE;

        if (strncasecmp_custom(argv[i], "TIMEOUT=", 8) == 0)
            apply_timeout_argument(download_options, argv[i] + 8);
    }

    /* If ESTIMATESPACE was requested and no pack was explicitly selected,
     * automatically select all packs so the user gets a full picture. */
    if (download_options->estimate_space == TRUE)
    {
        int any_requested = 0;
        int j;
        for (j = 0; j < 5; j++)
        {
            if (whdload_pack_defs[j].user_requested_download == 1)
            {
                any_requested = 1;
                break;
            }
        }
        if (!any_requested)
        {
            whdload_pack_defs[GAMES].user_requested_download        = 1;
            whdload_pack_defs[GAMES_BETA].user_requested_download   = 1;
            whdload_pack_defs[DEMOS].user_requested_download        = 1;
            whdload_pack_defs[DEMOS_BETA].user_requested_download   = 1;
            whdload_pack_defs[MAGAZINES].user_requested_download    = 1;
        }
    }
}
