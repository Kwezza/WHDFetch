#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ini_parser.h"
#include "log/log.h"
#include "platform/platform.h"
#include "utilities.h"

#define INI_FILE_PATH_PRIMARY "PROGDIR:whdfetch.ini"
#define INI_FILE_PATH_LEGACY "PROGDIR:WHDDownloader.ini"
#define INI_MAX_LINE 512
#define INI_MAX_ALLOCATED_STRINGS 64

static char *g_ini_owned_strings[INI_MAX_ALLOCATED_STRINGS];
static int g_ini_owned_string_count = 0;

static BOOL remember_ini_owned_string(char *ptr)
{
    if (ptr == NULL)
    {
        return FALSE;
    }

    if (g_ini_owned_string_count >= INI_MAX_ALLOCATED_STRINGS)
    {
        log_warning(LOG_GENERAL, "ini: allocation tracking limit reached (%ld), potential leak\n", (long)INI_MAX_ALLOCATED_STRINGS);
        return FALSE;
    }

    g_ini_owned_strings[g_ini_owned_string_count++] = ptr;
    return TRUE;
}

typedef enum {
    INI_SECTION_NONE = 0,
    INI_SECTION_GLOBAL,
    INI_SECTION_FILTERS,
    INI_SECTION_PACK
} IniSectionType;

static BOOL parse_boolean_value(const char *value, int *out_value)
{
    if (stricmp_custom(value, "1") == 0 ||
        stricmp_custom(value, "true") == 0 ||
        stricmp_custom(value, "yes") == 0 ||
        stricmp_custom(value, "on") == 0)
    {
        *out_value = 1;
        return TRUE;
    }

    if (stricmp_custom(value, "0") == 0 ||
        stricmp_custom(value, "false") == 0 ||
        stricmp_custom(value, "no") == 0 ||
        stricmp_custom(value, "off") == 0)
    {
        *out_value = 0;
        return TRUE;
    }

    return FALSE;
}

static BOOL set_string_override(const char **target, const char *value)
{
    char *dup;

    if (target == NULL || value == NULL)
    {
        return FALSE;
    }

    dup = amiga_strdup(value);
    if (dup == NULL)
    {
        log_warning(LOG_GENERAL, "ini: memory allocation failed for value override\n");
        return FALSE;
    }

    if (!remember_ini_owned_string(dup))
    {
        amiga_free(dup);
        return FALSE;
    }

    *target = dup;
    return TRUE;
}

static int map_pack_section_to_index(const char *section_name)
{
    if (stricmp_custom(section_name, "pack.demos_beta") == 0)
        return DEMOS_BETA;
    if (stricmp_custom(section_name, "pack.demos") == 0)
        return DEMOS;
    if (stricmp_custom(section_name, "pack.games_beta") == 0)
        return GAMES_BETA;
    if (stricmp_custom(section_name, "pack.games") == 0)
        return GAMES;
    if (stricmp_custom(section_name, "pack.magazines") == 0)
        return MAGAZINES;

    return -1;
}

static BOOL set_cleanup_removals_from_csv(const char *csv)
{
    char buffer[INI_MAX_LINE];
    char *token;
    int count = 0;

    if (csv == NULL)
    {
        return FALSE;
    }

    strncpy(buffer, csv, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    token = strtok(buffer, ",");
    while (token != NULL)
    {
        trim(token);
        if (token[0] != '\0')
        {
            if (count >= MAX_LINK_CLEANUP_REMOVALS)
            {
                log_warning(LOG_GENERAL, "ini: too many cleanup removals, max is %ld\n", (long)MAX_LINK_CLEANUP_REMOVALS);
                break;
            }

            if (!set_string_override(&LINK_CLEANUP_REMOVALS[count], token))
            {
                return FALSE;
            }

            count++;
        }

        token = strtok(NULL, ",");
    }

    LINK_CLEANUP_REMOVAL_COUNT = count;
    return TRUE;
}

static void apply_global_key_value(const char *key, const char *value, download_option *download_opts)
{
    int bool_value;

    if (download_opts == NULL)
    {
        return;
    }

    if (stricmp_custom(key, "download_website") == 0)
    {
        set_string_override(&DOWNLOAD_WEBSITE, value);
        return;
    }

    if (stricmp_custom(key, "file_part_to_remove") == 0)
    {
        set_string_override(&FILE_PART_TO_REMOVE, value);
        return;
    }

    if (stricmp_custom(key, "html_link_prefix_filter") == 0)
    {
        set_string_override(&HTML_LINK_PREFIX_FILTER, value);
        return;
    }

    if (stricmp_custom(key, "html_link_content_filter") == 0)
    {
        set_string_override(&HTML_LINK_CONTENT_FILTER, value);
        return;
    }

    if (stricmp_custom(key, "link_cleanup_removals") == 0)
    {
        set_cleanup_removals_from_csv(value);
        return;
    }

    if (stricmp_custom(key, "timeout_seconds") == 0)
    {
        ULONG parsed_timeout;
        ULONG raw_timeout;

        if (!parse_timeout_seconds(value, &parsed_timeout, &raw_timeout))
        {
            log_warning(LOG_GENERAL, "ini: invalid timeout_seconds value '%s' ignored\n", value);
            return;
        }

        if (parsed_timeout != raw_timeout)
        {
            log_warning(LOG_GENERAL,
                        "ini: timeout_seconds clamped to %ld seconds (%ld-%ld allowed)\n",
                        (long)parsed_timeout,
                        (long)TIMEOUT_SECONDS_MIN,
                        (long)TIMEOUT_SECONDS_MAX);
        }

        download_opts->timeout_seconds = parsed_timeout;
        return;
    }

    if (stricmp_custom(key, "extract_archives") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->extract_archives = bool_value ? TRUE : FALSE;
        return;
    }

    if (stricmp_custom(key, "skip_existing_extractions") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->skip_existing_extractions = bool_value ? TRUE : FALSE;
        return;
    }

    if (stricmp_custom(key, "skip_download_if_extracted") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->skip_download_if_extracted = bool_value ? TRUE : FALSE;
        return;
    }

    if (stricmp_custom(key, "force_download") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->force_download = bool_value ? TRUE : FALSE;
        return;
    }

    if (stricmp_custom(key, "extract_path") == 0)
    {
        if (value[0] == '\0')
        {
            download_opts->extract_path = NULL;
            return;
        }

        set_string_override(&download_opts->extract_path, value);
        return;
    }

    if (stricmp_custom(key, "delete_archives_after_extract") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->delete_archives_after_extract = bool_value ? TRUE : FALSE;
        return;
    }

    if (stricmp_custom(key, "use_custom_icons") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->use_custom_icons = bool_value ? TRUE : FALSE;
        return;
    }

    if (stricmp_custom(key, "unsnapshot_icons") == 0)
    {
        if (!parse_boolean_value(value, &bool_value))
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
            return;
        }

        download_opts->unsnapshot_icons = bool_value ? TRUE : FALSE;
        return;
    }

    log_debug(LOG_GENERAL, "ini: unknown [global] key '%s' ignored\n", key);
}

static void apply_filters_key_value(const char *key, const char *value)
{
    int bool_value;

    if (!parse_boolean_value(value, &bool_value))
    {
        log_warning(LOG_GENERAL, "ini: invalid boolean value '%s' ignored\n", value);
        return;
    }

    if (stricmp_custom(key, "skip_aga") == 0)
    {
        skip_AGA = bool_value;
        return;
    }

    if (stricmp_custom(key, "skip_cd") == 0)
    {
        skip_CD = bool_value;
        return;
    }

    if (stricmp_custom(key, "skip_ntsc") == 0)
    {
        skip_NTSC = bool_value;
        return;
    }

    if (stricmp_custom(key, "skip_non_english") == 0)
    {
        skip_NonEnglish = bool_value;
        return;
    }

    log_debug(LOG_GENERAL, "ini: unknown [filters] key '%s' ignored\n", key);
}

static void apply_pack_key_value(whdload_pack_def pack_defs[], int pack_index, const char *key, const char *value)
{
    int bool_value;

    if (pack_index < 0)
    {
        return;
    }

    if (stricmp_custom(key, "enabled") == 0)
    {
        if (parse_boolean_value(value, &bool_value))
        {
            pack_defs[pack_index].user_requested_download = bool_value ? 1 : 0;
        }
        else
        {
            log_warning(LOG_GENERAL, "ini: invalid boolean '%s' for key '%s'\n", value, key);
        }
        return;
    }

    if (stricmp_custom(key, "display_name") == 0)
    {
        set_string_override(&pack_defs[pack_index].full_text_name_of_pack, value);
        return;
    }

    if (stricmp_custom(key, "download_url") == 0)
    {
        set_string_override(&pack_defs[pack_index].download_url, value);
        return;
    }

    if (stricmp_custom(key, "local_dir") == 0)
    {
        set_string_override(&pack_defs[pack_index].extracted_pack_dir, value);
        return;
    }

    if (stricmp_custom(key, "filter_dat") == 0)
    {
        set_string_override(&pack_defs[pack_index].filter_dat_files, value);
        return;
    }

    if (stricmp_custom(key, "filter_zip") == 0)
    {
        set_string_override(&pack_defs[pack_index].filter_zip_files, value);
        return;
    }

    log_debug(LOG_GENERAL, "ini: unknown pack key '%s' ignored\n", key);
}

BOOL ini_parser_load_overrides(whdload_pack_def pack_defs[], download_option *download_opts)
{
    FILE *ini_file;
    const char *loaded_ini_path = NULL;
    char line[INI_MAX_LINE];
    char current_section[64] = "";
    IniSectionType current_type = INI_SECTION_NONE;
    int current_pack_index = -1;

    if (pack_defs == NULL || download_opts == NULL)
    {
        log_warning(LOG_GENERAL, "ini: invalid parser input pointers\n");
        return FALSE;
    }

    ini_file = fopen(INI_FILE_PATH_PRIMARY, "r");
    if (ini_file != NULL)
    {
        loaded_ini_path = INI_FILE_PATH_PRIMARY;
    }
    else
    {
        ini_file = fopen(INI_FILE_PATH_LEGACY, "r");
        if (ini_file != NULL)
        {
            loaded_ini_path = INI_FILE_PATH_LEGACY;
            log_info(LOG_GENERAL,
                     "ini: primary config missing, using legacy fallback %s\n",
                     INI_FILE_PATH_LEGACY);
        }
    }

    if (ini_file == NULL)
    {
        /* Optional file: silently keep defaults when missing. */
        return FALSE;
    }

    log_info(LOG_GENERAL, "ini: loading %s\n", loaded_ini_path);

    while (fgets(line, sizeof(line), ini_file) != NULL)
    {
        char *trimmed = line;

        trim(trimmed);
        if (trimmed[0] == '\0' || trimmed[0] == ';')
        {
            continue;
        }

        if (trimmed[0] == '[')
        {
            size_t len = strlen(trimmed);
            if (len > 2 && trimmed[len - 1] == ']')
            {
                trimmed[len - 1] = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
                trim(current_section);

                current_pack_index = -1;
                current_type = INI_SECTION_NONE;

                if (stricmp_custom(current_section, "global") == 0)
                {
                    current_type = INI_SECTION_GLOBAL;
                }
                else if (stricmp_custom(current_section, "filters") == 0)
                {
                    current_type = INI_SECTION_FILTERS;
                }
                else
                {
                    current_pack_index = map_pack_section_to_index(current_section);
                    if (current_pack_index >= 0)
                    {
                        current_type = INI_SECTION_PACK;
                    }
                }

                log_debug(LOG_GENERAL, "ini: section [%s]\n", current_section);
                continue;
            }

            log_warning(LOG_GENERAL, "ini: malformed section line ignored: %s\n", trimmed);
            continue;
        }

        if (strchr(trimmed, '=') == NULL)
        {
            log_warning(LOG_GENERAL, "ini: malformed key/value line ignored: %s\n", trimmed);
            continue;
        }

        {
            char *value = strchr(trimmed, '=');
            char key[128];

            *value = '\0';
            value++;

            strncpy(key, trimmed, sizeof(key) - 1);
            key[sizeof(key) - 1] = '\0';

            trim(key);
            trim(value);

            if (key[0] == '\0')
            {
                log_warning(LOG_GENERAL, "ini: empty key ignored\n");
                continue;
            }

            switch (current_type)
            {
                case INI_SECTION_GLOBAL:
                    apply_global_key_value(key, value, download_opts);
                    break;

                case INI_SECTION_FILTERS:
                    apply_filters_key_value(key, value);
                    break;

                case INI_SECTION_PACK:
                    apply_pack_key_value(pack_defs, current_pack_index, key, value);
                    break;

                case INI_SECTION_NONE:
                default:
                    log_debug(LOG_GENERAL, "ini: key outside known section ignored: %s\n", key);
                    break;
            }
        }
    }

    fclose(ini_file);
    log_info(LOG_GENERAL, "ini: parse complete\n");

    return TRUE;
}

void ini_parser_cleanup_overrides(void)
{
    int i;

    for (i = g_ini_owned_string_count - 1; i >= 0; i--)
    {
        if (g_ini_owned_strings[i] != NULL)
        {
            amiga_free(g_ini_owned_strings[i]);
            g_ini_owned_strings[i] = NULL;
        }
    }

    g_ini_owned_string_count = 0;
}
