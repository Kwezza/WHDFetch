#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "extract.h"
#include "gamefile_parser.h"
#include "icon_unsnapshot.h"
#include "log/log.h"
#include "platform/platform.h"
#include "platform/amiga_headers.h"
#include "utilities.h"

#define EXTRACT_LISTING_FILE "T:whdd_lha_listing.txt"
#define EXTRACT_ICONS_DIR    "PROGDIR:Icons"
#define EXTRACT_INDEX_FILE_NAME ".archive_index"
#define EXTRACT_INDEX_LINE_MAX  768

#define EXTRACT_LHA_TOOL "c:lha"
#define EXTRACT_UNLZX_TOOL "c:unlzx"

typedef struct archive_index_cache {
    char target_directory[EXTRACT_MAX_PATH];
    char **archive_names;
    char **folder_names;
    int entry_count;
    int capacity;
    BOOL loaded;
    BOOL dirty;
} archive_index_cache;

static archive_index_cache g_archive_index_cache = {0};

static BOOL extract_read_archive_name_from_metadata(const char *target_directory,
                                                    const char *game_folder_name,
                                                    char *out_archive_name,
                                                    size_t out_archive_name_size);

static BOOL extract_is_unlzx_available(void)
{
    return does_file_or_folder_exist(EXTRACT_UNLZX_TOOL, 0);
}

static char *index_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
    {
        return NULL;
    }

    len = strlen(text);
    copy = amiga_malloc(len + 1);
    if (copy == NULL)
    {
        return NULL;
    }

    strcpy(copy, text);
    return copy;
}

static void index_clear_entries(archive_index_cache *cache)
{
    int i;

    if (cache == NULL)
    {
        return;
    }

    for (i = 0; i < cache->entry_count; i++)
    {
        if (cache->archive_names != NULL && cache->archive_names[i] != NULL)
        {
            amiga_free(cache->archive_names[i]);
            cache->archive_names[i] = NULL;
        }

        if (cache->folder_names != NULL && cache->folder_names[i] != NULL)
        {
            amiga_free(cache->folder_names[i]);
            cache->folder_names[i] = NULL;
        }
    }

    if (cache->archive_names != NULL)
    {
        amiga_free(cache->archive_names);
        cache->archive_names = NULL;
    }

    if (cache->folder_names != NULL)
    {
        amiga_free(cache->folder_names);
        cache->folder_names = NULL;
    }

    cache->entry_count = 0;
    cache->capacity = 0;
}

static void index_reset_cache(archive_index_cache *cache)
{
    if (cache == NULL)
    {
        return;
    }

    index_clear_entries(cache);
    cache->target_directory[0] = '\0';
    cache->loaded = FALSE;
    cache->dirty = FALSE;
}

static BOOL index_build_file_path(const char *target_directory,
                                  char *out_index_path,
                                  size_t out_index_path_size)
{
    if (target_directory == NULL || out_index_path == NULL || out_index_path_size == 0)
    {
        return FALSE;
    }

    if (snprintf(out_index_path,
                 out_index_path_size,
                 "%s/%s",
                 target_directory,
                 EXTRACT_INDEX_FILE_NAME) >= (int)out_index_path_size)
    {
        return FALSE;
    }

    sanitize_amiga_file_path(out_index_path);
    return TRUE;
}

static int index_find_entry(const archive_index_cache *cache, const char *archive_filename)
{
    int i;

    if (cache == NULL || archive_filename == NULL)
    {
        return -1;
    }

    for (i = 0; i < cache->entry_count; i++)
    {
        if (cache->archive_names[i] != NULL && strcmp(cache->archive_names[i], archive_filename) == 0)
        {
            return i;
        }
    }

    return -1;
}

static BOOL index_ensure_capacity(archive_index_cache *cache, int required_capacity)
{
    int new_capacity;
    int i;
    char **new_archive_names;
    char **new_folder_names;

    if (cache == NULL)
    {
        return FALSE;
    }

    if (cache->capacity >= required_capacity)
    {
        return TRUE;
    }

    new_capacity = (cache->capacity > 0) ? cache->capacity : 32;
    while (new_capacity < required_capacity)
    {
        new_capacity *= 2;
    }

    new_archive_names = amiga_malloc((size_t)new_capacity * sizeof(char *));
    new_folder_names = amiga_malloc((size_t)new_capacity * sizeof(char *));
    if (new_archive_names == NULL || new_folder_names == NULL)
    {
        if (new_archive_names != NULL)
            amiga_free(new_archive_names);
        if (new_folder_names != NULL)
            amiga_free(new_folder_names);
        return FALSE;
    }

    memset(new_archive_names, 0, (size_t)new_capacity * sizeof(char *));
    memset(new_folder_names, 0, (size_t)new_capacity * sizeof(char *));

    for (i = 0; i < cache->entry_count; i++)
    {
        new_archive_names[i] = cache->archive_names[i];
        new_folder_names[i] = cache->folder_names[i];
    }

    if (cache->archive_names != NULL)
    {
        amiga_free(cache->archive_names);
    }

    if (cache->folder_names != NULL)
    {
        amiga_free(cache->folder_names);
    }

    cache->archive_names = new_archive_names;
    cache->folder_names = new_folder_names;
    cache->capacity = new_capacity;
    return TRUE;
}

static BOOL index_set_entry(archive_index_cache *cache,
                            const char *archive_filename,
                            const char *folder_name,
                            BOOL *out_changed)
{
    int entry_index;
    char *new_archive_name;
    char *new_folder_name;

    if (out_changed != NULL)
    {
        *out_changed = FALSE;
    }

    if (cache == NULL || archive_filename == NULL || folder_name == NULL ||
        archive_filename[0] == '\0' || folder_name[0] == '\0')
    {
        return FALSE;
    }

    entry_index = index_find_entry(cache, archive_filename);
    if (entry_index >= 0)
    {
        if (cache->folder_names[entry_index] != NULL && strcmp(cache->folder_names[entry_index], folder_name) == 0)
        {
            return TRUE;
        }

        new_folder_name = index_strdup(folder_name);
        if (new_folder_name == NULL)
        {
            return FALSE;
        }

        if (cache->folder_names[entry_index] != NULL)
        {
            amiga_free(cache->folder_names[entry_index]);
        }
        cache->folder_names[entry_index] = new_folder_name;

        if (out_changed != NULL)
        {
            *out_changed = TRUE;
        }
        return TRUE;
    }

    if (!index_ensure_capacity(cache, cache->entry_count + 1))
    {
        return FALSE;
    }

    new_archive_name = index_strdup(archive_filename);
    new_folder_name = index_strdup(folder_name);
    if (new_archive_name == NULL || new_folder_name == NULL)
    {
        if (new_archive_name != NULL)
            amiga_free(new_archive_name);
        if (new_folder_name != NULL)
            amiga_free(new_folder_name);
        return FALSE;
    }

    cache->archive_names[cache->entry_count] = new_archive_name;
    cache->folder_names[cache->entry_count] = new_folder_name;
    cache->entry_count++;

    if (out_changed != NULL)
    {
        *out_changed = TRUE;
    }

    return TRUE;
}

static BOOL index_remove_entry_by_archive_name(archive_index_cache *cache, const char *archive_filename)
{
    int entry_index;
    int i;

    if (cache == NULL || archive_filename == NULL)
    {
        return FALSE;
    }

    entry_index = index_find_entry(cache, archive_filename);
    if (entry_index < 0)
    {
        return FALSE;
    }

    if (cache->archive_names[entry_index] != NULL)
    {
        amiga_free(cache->archive_names[entry_index]);
        cache->archive_names[entry_index] = NULL;
    }

    if (cache->folder_names[entry_index] != NULL)
    {
        amiga_free(cache->folder_names[entry_index]);
        cache->folder_names[entry_index] = NULL;
    }

    for (i = entry_index; i < cache->entry_count - 1; i++)
    {
        cache->archive_names[i] = cache->archive_names[i + 1];
        cache->folder_names[i] = cache->folder_names[i + 1];
    }

    if (cache->entry_count > 0)
    {
        cache->archive_names[cache->entry_count - 1] = NULL;
        cache->folder_names[cache->entry_count - 1] = NULL;
        cache->entry_count--;
    }

    cache->dirty = TRUE;
    return TRUE;
}

static BOOL index_parse_file(const char *index_path, archive_index_cache *cache)
{
    FILE *index_file;
    char line[EXTRACT_INDEX_LINE_MAX] = {0};

    if (index_path == NULL || cache == NULL)
    {
        return FALSE;
    }

    index_file = fopen(index_path, "r");
    if (index_file == NULL)
    {
        return FALSE;
    }

    while (fgets(line, sizeof(line), index_file) != NULL)
    {
        char *cursor;
        char *tab;
        char *archive_name;
        char *folder_name;

        remove_CR_LF_from_string(line);
        cursor = line;
        while (*cursor != '\0' && isspace((unsigned char)*cursor))
        {
            cursor++;
        }

        if (cursor[0] == '\0')
        {
            continue;
        }

        if (cursor[0] == '#' || cursor[0] == ';')
        {
            continue;
        }

        tab = strchr(cursor, '\t');
        if (tab == NULL)
        {
            log_warning(LOG_GENERAL, "extract-index: skipping malformed line in '%s'\n", index_path);
            continue;
        }

        *tab = '\0';
        archive_name = cursor;
        folder_name = tab + 1;

        if (archive_name[0] == '\0' || folder_name[0] == '\0')
        {
            log_warning(LOG_GENERAL, "extract-index: skipping empty entry in '%s'\n", index_path);
            continue;
        }

        if (!index_set_entry(cache, archive_name, folder_name, NULL))
        {
            fclose(index_file);
            return FALSE;
        }
    }

    fclose(index_file);
    return TRUE;
}

static BOOL index_write_file(const char *index_path, const archive_index_cache *cache)
{
    FILE *index_file;
    int i;

    if (index_path == NULL || cache == NULL)
    {
        return FALSE;
    }

    index_file = fopen(index_path, "w");
    if (index_file == NULL)
    {
        return FALSE;
    }

    if (fprintf(index_file,
                "# Retroplay WHD Downloader archive index (auto-generated)\n"
                "# Maps archive filename to extracted folder name.\n"
                "# Format: <archive_filename>\\t<folder_name>\n") < 0)
    {
        fclose(index_file);
        return FALSE;
    }

    for (i = 0; i < cache->entry_count; i++)
    {
        if (cache->archive_names[i] == NULL || cache->folder_names[i] == NULL)
        {
            continue;
        }

        if (fprintf(index_file, "%s\t%s\n", cache->archive_names[i], cache->folder_names[i]) < 0)
        {
            fclose(index_file);
            return FALSE;
        }
    }

    fclose(index_file);

#ifdef FIBF_HIDDEN
    if (!SetProtection(index_path, FIBF_HIDDEN))
    {
        log_warning(LOG_GENERAL,
                    "extract-index: SetProtection(FIBF_HIDDEN) failed for '%s' (IoErr=%ld)\n",
                    index_path,
                    (long)IoErr());
    }
#endif

    return TRUE;
}

static BOOL index_build_from_scan(const char *target_directory, archive_index_cache *cache)
{
    BPTR directory_lock;
    struct FileInfoBlock *fib;
    char metadata_archive_name[EXTRACT_MAX_PATH] = {0};

    if (target_directory == NULL || cache == NULL)
    {
        return FALSE;
    }

    directory_lock = Lock(target_directory, ACCESS_READ);
    if (directory_lock == 0)
    {
        return FALSE;
    }

    fib = amiga_malloc(sizeof(struct FileInfoBlock));
    if (fib == NULL)
    {
        UnLock(directory_lock);
        return FALSE;
    }

    if (!Examine(directory_lock, fib))
    {
        amiga_free(fib);
        UnLock(directory_lock);
        return FALSE;
    }

    while (ExNext(directory_lock, fib))
    {
        if (fib->fib_DirEntryType <= 0)
        {
            continue;
        }

        if (fib->fib_FileName[0] == '.')
        {
            continue;
        }

        if (!extract_read_archive_name_from_metadata(target_directory,
                                                     fib->fib_FileName,
                                                     metadata_archive_name,
                                                     sizeof(metadata_archive_name)))
        {
            continue;
        }

        if (!index_set_entry(cache, metadata_archive_name, fib->fib_FileName, NULL))
        {
            amiga_free(fib);
            UnLock(directory_lock);
            return FALSE;
        }
    }

    amiga_free(fib);
    UnLock(directory_lock);
    return TRUE;
}

BOOL extract_index_load(const char *target_directory)
{
    char index_path[EXTRACT_MAX_PATH] = {0};

    if (target_directory == NULL || target_directory[0] == '\0')
    {
        return FALSE;
    }

    if (g_archive_index_cache.loaded && strcmp(g_archive_index_cache.target_directory, target_directory) == 0)
    {
        return TRUE;
    }

    if (g_archive_index_cache.loaded)
    {
        extract_index_flush();
    }

    index_reset_cache(&g_archive_index_cache);

    strncpy(g_archive_index_cache.target_directory,
            target_directory,
            sizeof(g_archive_index_cache.target_directory) - 1);
    g_archive_index_cache.target_directory[sizeof(g_archive_index_cache.target_directory) - 1] = '\0';
    sanitize_amiga_file_path(g_archive_index_cache.target_directory);

    if (!index_build_file_path(g_archive_index_cache.target_directory, index_path, sizeof(index_path)))
    {
        index_reset_cache(&g_archive_index_cache);
        return FALSE;
    }

    if (does_file_or_folder_exist(index_path, 0))
    {
        if (!index_parse_file(index_path, &g_archive_index_cache))
        {
            index_reset_cache(&g_archive_index_cache);
            return FALSE;
        }

        g_archive_index_cache.loaded = TRUE;
        g_archive_index_cache.dirty = FALSE;
        return TRUE;
    }

    if (!index_build_from_scan(g_archive_index_cache.target_directory, &g_archive_index_cache))
    {
        index_reset_cache(&g_archive_index_cache);
        return FALSE;
    }

    g_archive_index_cache.loaded = TRUE;
    g_archive_index_cache.dirty = (g_archive_index_cache.entry_count > 0) ? TRUE : FALSE;

    if (g_archive_index_cache.dirty)
    {
        if (!index_write_file(index_path, &g_archive_index_cache))
        {
            log_warning(LOG_GENERAL,
                        "extract-index: built cache for '%s' but failed to persist '%s'\n",
                        g_archive_index_cache.target_directory,
                        index_path);
        }
        else
        {
            g_archive_index_cache.dirty = FALSE;
        }
    }

    return TRUE;
}

BOOL extract_index_lookup(const char *target_directory,
                          const char *archive_filename,
                          char *out_folder_name,
                          size_t out_folder_name_size)
{
    int entry_index;

    if (target_directory == NULL || archive_filename == NULL ||
        out_folder_name == NULL || out_folder_name_size == 0)
    {
        return FALSE;
    }

    if (!extract_index_load(target_directory))
    {
        return FALSE;
    }

    entry_index = index_find_entry(&g_archive_index_cache, archive_filename);
    if (entry_index < 0 || g_archive_index_cache.folder_names[entry_index] == NULL)
    {
        return FALSE;
    }

    strncpy(out_folder_name, g_archive_index_cache.folder_names[entry_index], out_folder_name_size - 1);
    out_folder_name[out_folder_name_size - 1] = '\0';
    return TRUE;
}

BOOL extract_index_find_by_title(const char *title,
                                 char *out_old_archive,
                                 size_t out_size)
{
    size_t title_len;
    int i;

    if (title == NULL || title[0] == '\0' || out_old_archive == NULL || out_size == 0)
    {
        return FALSE;
    }

    if (!g_archive_index_cache.loaded)
    {
        return FALSE;
    }

    title_len = strlen(title);

    for (i = 0; i < g_archive_index_cache.entry_count; i++)
    {
        const char *archive_name;
        char separator;

        archive_name = g_archive_index_cache.archive_names[i];
        if (archive_name == NULL)
        {
            continue;
        }

        if (strncmp(archive_name, title, title_len) != 0)
        {
            continue;
        }

        separator = archive_name[title_len];
        if (separator != '_' && separator != '.')
        {
            continue;
        }

        strncpy(out_old_archive, archive_name, out_size - 1);
        out_old_archive[out_size - 1] = '\0';
        return TRUE;
    }

    return FALSE;
}

static int extract_compare_version_strings(const char *left, const char *right)
{
    const unsigned char *l = (const unsigned char *)left;
    const unsigned char *r = (const unsigned char *)right;

    if (left == NULL || right == NULL)
    {
        if (left == right)
        {
            return 0;
        }

        return (left == NULL) ? -1 : 1;
    }

    while (*l != '\0' || *r != '\0')
    {
        if (isdigit(*l) && isdigit(*r))
        {
            unsigned long ln = 0;
            unsigned long rn = 0;

            while (*l == '0')
            {
                l++;
            }

            while (*r == '0')
            {
                r++;
            }

            while (isdigit(*l))
            {
                ln = (ln * 10UL) + (unsigned long)(*l - '0');
                l++;
            }

            while (isdigit(*r))
            {
                rn = (rn * 10UL) + (unsigned long)(*r - '0');
                r++;
            }

            if (ln != rn)
            {
                return (ln > rn) ? 1 : -1;
            }

            continue;
        }

        while (*l != '\0' && !isalnum(*l))
        {
            l++;
        }

        while (*r != '\0' && !isalnum(*r))
        {
            r++;
        }

        if (*l == '\0' || *r == '\0')
        {
            break;
        }

        if (isdigit(*l) || isdigit(*r))
        {
            continue;
        }

        {
            int lc = tolower(*l);
            int rc = tolower(*r);

            if (lc != rc)
            {
                return (lc > rc) ? 1 : -1;
            }
        }

        l++;
        r++;
    }

    while (*l != '\0' && !isalnum(*l))
    {
        l++;
    }

    while (*r != '\0' && !isalnum(*r))
    {
        r++;
    }

    if (*l == '\0' && *r == '\0')
    {
        return 0;
    }

    return (*l == '\0') ? -1 : 1;
}

static BOOL extract_has_numeric_version_token(const char *version)
{
    const unsigned char *ptr = (const unsigned char *)version;

    if (version == NULL || version[0] == '\0')
    {
        return FALSE;
    }

    while (*ptr != '\0')
    {
        if (isdigit(*ptr))
        {
            return TRUE;
        }

        ptr++;
    }

    return FALSE;
}

static BOOL extract_strings_equal_ci(const char *left, const char *right)
{
    size_t left_len;
    size_t right_len;

    if (left == NULL || right == NULL)
    {
        return (left == right) ? TRUE : FALSE;
    }

    left_len = strlen(left);
    right_len = strlen(right);
    if (left_len != right_len)
    {
        return FALSE;
    }

    if (strncasecmp_custom(left, right, left_len) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

static BOOL extract_parse_archive_metadata(const char *archive_name, game_metadata *out_metadata)
{
    if (archive_name == NULL || archive_name[0] == '\0' || out_metadata == NULL)
    {
        return FALSE;
    }

    memset(out_metadata, 0, sizeof(*out_metadata));
    extract_game_info_from_filename(archive_name, out_metadata);
    if (out_metadata->title[0] == '\0')
    {
        return FALSE;
    }

    return TRUE;
}

static BOOL extract_is_same_release_identity(const game_metadata *new_metadata,
                                             const game_metadata *old_metadata)
{
    if (new_metadata == NULL || old_metadata == NULL)
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->title, old_metadata->title))
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->special, old_metadata->special))
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->machineType, old_metadata->machineType))
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->videoFormat, old_metadata->videoFormat))
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->language, old_metadata->language))
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->mediaFormat, old_metadata->mediaFormat))
    {
        return FALSE;
    }

    if (!extract_strings_equal_ci(new_metadata->sps, old_metadata->sps))
    {
        return FALSE;
    }

    if (new_metadata->numberOfDisks != old_metadata->numberOfDisks)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL extract_index_find_update_candidate(const char *new_archive_filename,
                                         char *out_old_archive,
                                         size_t out_size)
{
    game_metadata new_metadata = {0};
    char best_archive_name[EXTRACT_MAX_NAME] = {0};
    char best_version[MAX_VERSION_LEN] = {0};
    BOOL found = FALSE;
    int i;

    if (new_archive_filename == NULL || new_archive_filename[0] == '\0' ||
        out_old_archive == NULL || out_size == 0)
    {
        return FALSE;
    }

    if (!g_archive_index_cache.loaded)
    {
        return FALSE;
    }

    if (!extract_parse_archive_metadata(new_archive_filename, &new_metadata))
    {
        return FALSE;
    }

    if (!extract_has_numeric_version_token(new_metadata.version))
    {
        return FALSE;
    }

    for (i = 0; i < g_archive_index_cache.entry_count; i++)
    {
        const char *archive_name;
        game_metadata old_metadata = {0};
        int cmp_new_vs_old;

        archive_name = g_archive_index_cache.archive_names[i];
        if (archive_name == NULL || archive_name[0] == '\0')
        {
            continue;
        }

        if (strcmp(archive_name, new_archive_filename) == 0)
        {
            continue;
        }

        if (!extract_parse_archive_metadata(archive_name, &old_metadata))
        {
            continue;
        }

        if (!extract_is_same_release_identity(&new_metadata, &old_metadata))
        {
            continue;
        }

        if (!extract_has_numeric_version_token(old_metadata.version))
        {
            continue;
        }

        cmp_new_vs_old = extract_compare_version_strings(new_metadata.version, old_metadata.version);
        if (cmp_new_vs_old <= 0)
        {
            continue;
        }

        if (!found || extract_compare_version_strings(old_metadata.version, best_version) > 0)
        {
            strncpy(best_archive_name, archive_name, sizeof(best_archive_name) - 1);
            best_archive_name[sizeof(best_archive_name) - 1] = '\0';

            strncpy(best_version, old_metadata.version, sizeof(best_version) - 1);
            best_version[sizeof(best_version) - 1] = '\0';

            found = TRUE;
        }
    }

    if (!found)
    {
        return FALSE;
    }

    strncpy(out_old_archive, best_archive_name, out_size - 1);
    out_old_archive[out_size - 1] = '\0';
    return TRUE;
}

BOOL extract_index_update(const char *target_directory,
                          const char *archive_filename,
                          const char *folder_name)
{
    BOOL changed = FALSE;

    if (target_directory == NULL || archive_filename == NULL || folder_name == NULL)
    {
        return FALSE;
    }

    if (!extract_index_load(target_directory))
    {
        return FALSE;
    }

    if (!index_set_entry(&g_archive_index_cache, archive_filename, folder_name, &changed))
    {
        return FALSE;
    }

    if (changed)
    {
        g_archive_index_cache.dirty = TRUE;
    }

    return TRUE;
}

void extract_index_flush(void)
{
    char index_path[EXTRACT_MAX_PATH] = {0};

    if (!g_archive_index_cache.loaded)
    {
        return;
    }

    if (g_archive_index_cache.dirty)
    {
        if (index_build_file_path(g_archive_index_cache.target_directory,
                                  index_path,
                                  sizeof(index_path)))
        {
            if (!index_write_file(index_path, &g_archive_index_cache))
            {
                log_warning(LOG_GENERAL,
                            "extract-index: failed to flush '%s'\n",
                            index_path);
            }
        }
        else
        {
            log_warning(LOG_GENERAL,
                        "extract-index: failed to build index path during flush for '%s'\n",
                        g_archive_index_cache.target_directory);
        }
    }

    index_reset_cache(&g_archive_index_cache);
}

static void extract_apply_whdload_folder_icon(const char *target_directory,
                                               const char *game_folder_name,
                                               const download_option *download_options)
{
    char dest_icon_name[EXTRACT_MAX_PATH] = {0};
    char dest_info_path[EXTRACT_MAX_PATH + 5] = {0};
    struct DiskObject *diskobj;

    static int template_checked = 0;
    static BOOL template_exists = FALSE;

    if (target_directory == NULL || game_folder_name == NULL || download_options == NULL)
        return;

    if (!download_options->use_custom_icons)
        return;

    /* One-time check: does PROGDIR:Icons/WHD folder.info exist? */
    if (!template_checked)
    {
        struct DiskObject *probe = GetDiskObject("PROGDIR:Icons/WHD folder");
        if (probe != NULL)
        {
            FreeDiskObject(probe);
            template_exists = TRUE;
            log_info(LOG_GENERAL, "extract: WHD folder template icon found\n");
        }
        else
        {
            template_exists = FALSE;
            log_debug(LOG_GENERAL, "extract: no 'WHD folder.info' in PROGDIR:Icons, game folder icons unchanged\n");
        }
        template_checked = 1;
    }

    if (!template_exists)
        return;

    /* Build dest paths: target_directory/game_folder_name (no .info) for PutDiskObject,
       and target_directory/game_folder_name.info for the existence/delete check. */
    if (snprintf(dest_icon_name, sizeof(dest_icon_name), "%s/%s",
                 target_directory, game_folder_name) >= (int)sizeof(dest_icon_name))
    {
        log_warning(LOG_GENERAL, "extract: WHD folder icon path too long for '%s'\n", game_folder_name);
        return;
    }
    sanitize_amiga_file_path(dest_icon_name);

    if (snprintf(dest_info_path, sizeof(dest_info_path), "%s.info", dest_icon_name)
            >= (int)sizeof(dest_info_path))
        return;

    /* If there is an existing icon, clear its protection and delete it */
    if (does_file_or_folder_exist(dest_info_path, 0))
    {
        if (!SetProtection(dest_info_path, 0))
        {
            log_warning(LOG_GENERAL,
                        "extract: SetProtection failed for '%s' (IoErr=%ld), skipping WHD folder icon\n",
                        dest_info_path, (long)IoErr());
            return;
        }

        if (!DeleteFile(dest_info_path))
        {
            log_warning(LOG_GENERAL,
                        "extract: could not delete existing icon '%s' (IoErr=%ld), skipping WHD folder icon\n",
                        dest_info_path, (long)IoErr());
            return;
        }
    }

    /* Load a fresh copy of the template and write it under the game folder name */
    diskobj = GetDiskObject("PROGDIR:Icons/WHD folder");
    if (diskobj == NULL)
    {
        log_warning(LOG_GENERAL,
                    "extract: GetDiskObject('WHD folder') failed unexpectedly (IoErr=%ld)\n",
                    (long)IoErr());
        return;
    }

    if (!PutDiskObject(dest_icon_name, diskobj))
    {
        log_warning(LOG_GENERAL,
                    "extract: PutDiskObject failed for '%s' (IoErr=%ld)\n",
                    dest_icon_name, (long)IoErr());
    }
    else
    {
        log_info(LOG_GENERAL, "extract: applied WHD folder icon to '%s'\n", dest_icon_name);

        if (download_options->unsnapshot_icons)
        {
            if (!strip_icon_position(dest_icon_name))
            {
                log_warning(LOG_GENERAL,
                            "extract: unsnapshot failed for '%s.info' (IoErr=%ld)\n",
                            dest_icon_name,
                            (long)IoErr());
            }
        }
    }

    FreeDiskObject(diskobj);
}

static void extract_ensure_drawer_icon(const char *dir_path, const download_option *download_options)
{
    char info_path[EXTRACT_MAX_PATH] = {0};
    char custom_source[EXTRACT_MAX_PATH] = {0};
    const char *leaf_name;
    struct DiskObject *diskobj;
    BOOL used_custom_icon = FALSE;

    static int icons_dir_checked = 0;
    static BOOL icons_dir_exists = FALSE;

    if (dir_path == NULL || download_options == NULL)
        return;

    /* Check whether the icon already exists - nothing to do */
    if (snprintf(info_path, sizeof(info_path), "%s.info", dir_path) >= (int)sizeof(info_path))
        return;
    sanitize_amiga_file_path(info_path);
    if (does_file_or_folder_exist(info_path, 0))
        return;

    /* Extract the leaf name (last path component after '/' or ':', or whole string) */
    leaf_name = strrchr(dir_path, '/');
    if (leaf_name != NULL)
    {
        leaf_name++;
    }
    else
    {
        const char *colon = strrchr(dir_path, ':');
        leaf_name = (colon != NULL) ? colon + 1 : dir_path;
    }

    if (leaf_name[0] == '\0')
        return;

    diskobj = NULL;

    /* Try custom icon from PROGDIR:Icons/ if the feature is enabled */
    if (download_options->use_custom_icons)
    {
        if (!icons_dir_checked)
        {
            icons_dir_exists = does_file_or_folder_exist(EXTRACT_ICONS_DIR, 0);
            icons_dir_checked = 1;
            if (icons_dir_exists)
                log_info(LOG_GENERAL, "extract: custom icons folder found at '%s'\n", EXTRACT_ICONS_DIR);
            else
                log_debug(LOG_GENERAL, "extract: no custom icons folder at '%s', using defaults\n", EXTRACT_ICONS_DIR);
        }

        if (icons_dir_exists)
        {
            if (snprintf(custom_source, sizeof(custom_source), "%s/%s", EXTRACT_ICONS_DIR, leaf_name) < (int)sizeof(custom_source))
            {
                sanitize_amiga_file_path(custom_source);
                diskobj = GetDiskObject(custom_source);
                if (diskobj != NULL)
                {
                    used_custom_icon = TRUE;
                    log_debug(LOG_GENERAL, "extract: using custom icon '%s.info' for '%s'\n", custom_source, dir_path);
                }
            }
        }
    }

    /* Fall back to the system default drawer icon */
    if (diskobj == NULL)
    {
        diskobj = GetDefDiskObject(WBDRAWER);
        if (diskobj == NULL)
        {
            log_warning(LOG_GENERAL, "extract: GetDefDiskObject(WBDRAWER) failed for '%s' (IoErr=%ld)\n",
                        dir_path, (long)IoErr());
            return;
        }
    }

    if (!PutDiskObject(dir_path, diskobj))
    {
        log_warning(LOG_GENERAL, "extract: PutDiskObject failed for '%s' (IoErr=%ld)\n",
                    dir_path, (long)IoErr());
    }
    else
    {
        log_info(LOG_GENERAL, "extract: created drawer icon for '%s'\n", dir_path);

        if (used_custom_icon && download_options->unsnapshot_icons)
        {
            if (!strip_icon_position(dir_path))
            {
                log_warning(LOG_GENERAL,
                            "extract: unsnapshot failed for '%s.info' (IoErr=%ld)\n",
                            dir_path,
                            (long)IoErr());
            }
        }
    }

    FreeDiskObject(diskobj);
}

static BOOL extract_get_parent_directory(const char *archive_path,
                                         char *out_parent_directory,
                                         size_t out_parent_directory_size)
{
    const char *last_slash;
    size_t dir_len;

    if (archive_path == NULL || out_parent_directory == NULL || out_parent_directory_size == 0)
    {
        return FALSE;
    }

    last_slash = strrchr(archive_path, '/');
    if (last_slash == NULL)
    {
        return FALSE;
    }

    dir_len = (size_t)(last_slash - archive_path);
    if (dir_len == 0 || dir_len >= out_parent_directory_size)
    {
        return FALSE;
    }

    strncpy(out_parent_directory, archive_path, dir_len);
    out_parent_directory[dir_len] = '\0';
    sanitize_amiga_file_path(out_parent_directory);

    return TRUE;
}

static void extract_strip_trailing_slash(char *path)
{
    size_t len;

    if (path == NULL)
    {
        return;
    }

    len = strlen(path);
    while (len > 0 && path[len - 1] == '/')
    {
        path[len - 1] = '\0';
        len--;
    }
}

static BOOL extract_prepare_existing_target_folder(const char *target_directory, const char *game_folder_name)
{
    char folder_path[EXTRACT_MAX_PATH] = {0};
    char protect_command[EXTRACT_MAX_PATH + 64] = {0};

    if (target_directory == NULL || game_folder_name == NULL)
    {
        return FALSE;
    }

    if (snprintf(folder_path, sizeof(folder_path), "%s/%s", target_directory, game_folder_name) >= (int)sizeof(folder_path))
    {
        log_error(LOG_GENERAL, "extract: target folder path too long for protect step\n");
        return FALSE;
    }

    sanitize_amiga_file_path(folder_path);

    if (!does_file_or_folder_exist(folder_path, 0))
    {
        return TRUE;
    }

    if (snprintf(protect_command,
                 sizeof(protect_command),
                 "protect \"%s/#?\" ALL rwed >NIL:",
                 folder_path) >= (int)sizeof(protect_command))
    {
        log_error(LOG_GENERAL, "extract: protect command too long\n");
        return FALSE;
    }

    log_info(LOG_GENERAL, "extract: preparing existing folder '%s' for replacement\n", folder_path);
    if (SystemTagList(protect_command, NULL) != 0)
    {
        log_warning(LOG_GENERAL, "extract: protect command returned non-zero for '%s'\n", folder_path);
    }

    return TRUE;
}

static BOOL extract_get_top_level_directory_from_lha(const char *archive_path,
                                                     char *out_folder_name,
                                                     size_t out_folder_name_size)
{
    char clean_archive_path[EXTRACT_MAX_PATH] = {0};
    char list_command[EXTRACT_MAX_PATH * 2] = {0};
    FILE *listing_file;
    char line[EXTRACT_MAX_PATH] = {0};
    char path_prefix[EXTRACT_MAX_PATH] = {0};

    if (archive_path == NULL || out_folder_name == NULL || out_folder_name_size == 0)
    {
        return FALSE;
    }

    strncpy(clean_archive_path, archive_path, sizeof(clean_archive_path) - 1);
    clean_archive_path[sizeof(clean_archive_path) - 1] = '\0';
    sanitize_amiga_file_path(clean_archive_path);

    if (snprintf(list_command,
                 sizeof(list_command),
                 "c:lha vq \"%s\" >%s",
                 clean_archive_path,
                 EXTRACT_LISTING_FILE) >= (int)sizeof(list_command))
    {
        return FALSE;
    }

    DeleteFile(EXTRACT_LISTING_FILE);
    if (SystemTagList(list_command, NULL) != 0)
    {
        return FALSE;
    }

    listing_file = fopen(EXTRACT_LISTING_FILE, "r");
    if (listing_file == NULL)
    {
        return FALSE;
    }

    while (fgets(line, sizeof(line), listing_file) != NULL)
    {
        char *slash;
        size_t prefix_len;
        char *folder_start;

        if (strstr(line, "Listing of archive") != NULL)
        {
            continue;
        }

        remove_CR_LF_from_string(line);
        trim(line);

        if (line[0] == '\0')
        {
            continue;
        }

        slash = strchr(line, '/');
        if (slash != NULL)
        {
            prefix_len = (size_t)(slash - line);
            if (prefix_len > 0 && prefix_len < sizeof(path_prefix))
            {
                strncpy(path_prefix, line, prefix_len);
                path_prefix[prefix_len] = '\0';
                trim(path_prefix);

                folder_start = path_prefix + strlen(path_prefix);
                while (folder_start > path_prefix && !isspace((unsigned char)folder_start[-1]))
                {
                    folder_start--;
                }

                if (folder_start[0] == '\0' || strlen(folder_start) >= out_folder_name_size)
                {
                    continue;
                }

                strncpy(out_folder_name, folder_start, out_folder_name_size - 1);
                out_folder_name[out_folder_name_size - 1] = '\0';
                remove_CR_LF_from_string(out_folder_name);
                trim(out_folder_name);
                sanitize_amiga_file_path(out_folder_name);

                if (out_folder_name[0] == '\0')
                {
                    continue;
                }

                fclose(listing_file);
                DeleteFile(EXTRACT_LISTING_FILE);
                return TRUE;
            }
        }
    }

    fclose(listing_file);
    DeleteFile(EXTRACT_LISTING_FILE);
    return FALSE;
}

static BOOL extract_get_top_level_directory_from_lzx(const char *archive_path,
                                                     char *out_folder_name,
                                                     size_t out_folder_name_size)
{
    char clean_archive_path[EXTRACT_MAX_PATH] = {0};
    char list_command[EXTRACT_MAX_PATH * 2] = {0};
    FILE *listing_file;
    char line[EXTRACT_MAX_PATH] = {0};
    char path_prefix[EXTRACT_MAX_PATH] = {0};

    if (archive_path == NULL || out_folder_name == NULL || out_folder_name_size == 0)
    {
        return FALSE;
    }

    strncpy(clean_archive_path, archive_path, sizeof(clean_archive_path) - 1);
    clean_archive_path[sizeof(clean_archive_path) - 1] = '\0';
    sanitize_amiga_file_path(clean_archive_path);

    if (snprintf(list_command,
                 sizeof(list_command),
                 EXTRACT_UNLZX_TOOL " v \"%s\" >%s",
                 clean_archive_path,
                 EXTRACT_LISTING_FILE) >= (int)sizeof(list_command))
    {
        return FALSE;
    }

    log_debug(LOG_GENERAL, "extract: probing unlzx listing using '%s'\n", list_command);

    DeleteFile(EXTRACT_LISTING_FILE);
    if (SystemTagList(list_command, NULL) != 0)
    {
        return FALSE;
    }

    listing_file = fopen(EXTRACT_LISTING_FILE, "r");
    if (listing_file == NULL)
    {
        return FALSE;
    }

    while (fgets(line, sizeof(line), listing_file) != NULL)
    {
        char *slash;
        char *token_start;
        size_t prefix_len;

        if (strstr(line, "UNLZX") != NULL ||
            strstr(line, "Viewing archive") != NULL ||
            strstr(line, "Original") != NULL ||
            strstr(line, "--------") != NULL ||
            strstr(line, "Operation") != NULL)
        {
            continue;
        }

        remove_CR_LF_from_string(line);
        trim(line);

        if (line[0] == '\0')
        {
            continue;
        }

        /* Use last slash in the line so we parse the filename token,
           not the ratio field token like "n/a". */
        slash = strrchr(line, '/');
        if (slash != NULL)
        {
            token_start = slash;
            while (token_start > line && !isspace((unsigned char)token_start[-1]))
            {
                token_start--;
            }

            prefix_len = (size_t)(slash - token_start);
            if (prefix_len > 0 && prefix_len < sizeof(path_prefix))
            {
                strncpy(path_prefix, token_start, prefix_len);
                path_prefix[prefix_len] = '\0';
                trim(path_prefix);

                if (path_prefix[0] == '\0' || strlen(path_prefix) >= out_folder_name_size)
                {
                    continue;
                }

                strncpy(out_folder_name, path_prefix, out_folder_name_size - 1);
                out_folder_name[out_folder_name_size - 1] = '\0';
                remove_CR_LF_from_string(out_folder_name);
                trim(out_folder_name);
                sanitize_amiga_file_path(out_folder_name);

                if (out_folder_name[0] == '\0')
                {
                    continue;
                }

                log_debug(LOG_GENERAL,
                          "extract: unlzx listing resolved top-level folder '%s' from '%s'\n",
                          out_folder_name,
                          clean_archive_path);

                fclose(listing_file);
                DeleteFile(EXTRACT_LISTING_FILE);
                return TRUE;
            }
        }
    }

    fclose(listing_file);
    DeleteFile(EXTRACT_LISTING_FILE);
    log_debug(LOG_GENERAL,
              "extract: unlzx listing did not yield top-level folder for '%s'; using heuristic fallback\n",
              clean_archive_path);
    return FALSE;
}

static BOOL extract_delete_archive_if_needed(const char *archive_path, const download_option *download_options)
{
    if (archive_path == NULL || download_options == NULL)
    {
        return FALSE;
    }

    if (!extract_should_delete_archive(download_options))
    {
        return TRUE;
    }

    if (!DeleteFile(archive_path))
    {
        log_warning(LOG_GENERAL, "extract: failed to delete archive '%s' (IoErr=%ld)\n", archive_path, (long)IoErr());
        return FALSE;
    }

    log_info(LOG_GENERAL, "extract: deleted archive '%s' after successful extraction\n", archive_path);
    return TRUE;
}

BOOL extract_should_delete_archive(const download_option *download_options)
{
    if (download_options == NULL)
    {
        return FALSE;
    }

    return (download_options->delete_archives_after_extract == TRUE) ? TRUE : FALSE;
}

BOOL extract_derive_game_folder_name(const char *archive_filename,
                                     char *out_folder_name,
                                     size_t out_folder_name_size)
{
    const char *underscore;
    size_t copy_len;

    if (archive_filename == NULL || out_folder_name == NULL || out_folder_name_size == 0)
    {
        return FALSE;
    }

    underscore = strchr(archive_filename, '_');
    if (underscore != NULL)
    {
        copy_len = (size_t)(underscore - archive_filename);
    }
    else
    {
        const char *dot = strrchr(archive_filename, '.');
        if (dot != NULL)
        {
            copy_len = (size_t)(dot - archive_filename);
        }
        else
        {
            copy_len = strlen(archive_filename);
        }
    }

    if (copy_len == 0 || copy_len >= out_folder_name_size)
    {
        return FALSE;
    }

    strncpy(out_folder_name, archive_filename, copy_len);
    out_folder_name[copy_len] = '\0';
    sanitize_amiga_file_path(out_folder_name);

    return TRUE;
}

BOOL extract_build_target_directory(const char *archive_path,
                                    const char *pack_dir,
                                    const char *first_letter,
                                    const download_option *download_options,
                                    char *out_target_directory,
                                    size_t out_target_directory_size)
{
    char clean_pack_dir[EXTRACT_MAX_NAME] = {0};
    char normalized_letter[2] = {0};

    if (archive_path == NULL || pack_dir == NULL || first_letter == NULL || download_options == NULL ||
        out_target_directory == NULL || out_target_directory_size == 0)
    {
        return FALSE;
    }

    strncpy(clean_pack_dir, pack_dir, sizeof(clean_pack_dir) - 1);
    clean_pack_dir[sizeof(clean_pack_dir) - 1] = '\0';
    extract_strip_trailing_slash(clean_pack_dir);

    normalized_letter[0] = first_letter[0];
    get_folder_name_from_character(&normalized_letter[0]);

    if (download_options->extract_path != NULL && download_options->extract_path[0] != '\0')
    {
        char base_path[EXTRACT_MAX_PATH] = {0};
        char pack_path[EXTRACT_MAX_PATH] = {0};

        strncpy(base_path, download_options->extract_path, sizeof(base_path) - 1);
        base_path[sizeof(base_path) - 1] = '\0';
        sanitize_amiga_file_path(base_path);

        if (snprintf(pack_path, sizeof(pack_path), "%s/%s", base_path, clean_pack_dir) >= (int)sizeof(pack_path))
        {
            return FALSE;
        }
        sanitize_amiga_file_path(pack_path);

        if (snprintf(out_target_directory,
                     out_target_directory_size,
                     "%s/%s",
                     pack_path,
                     normalized_letter) >= (int)out_target_directory_size)
        {
            return FALSE;
        }
        sanitize_amiga_file_path(out_target_directory);

        if (!create_Directory_and_unlock(base_path) ||
            !create_Directory_and_unlock(pack_path) ||
            !create_Directory_and_unlock(out_target_directory))
        {
            return FALSE;
        }

        extract_ensure_drawer_icon(base_path, download_options);
        extract_ensure_drawer_icon(pack_path, download_options);
        extract_ensure_drawer_icon(out_target_directory, download_options);

        return TRUE;
    }

    return extract_get_parent_directory(archive_path, out_target_directory, out_target_directory_size);
}

LONG extract_archive_with_lha(const char *archive_path, const char *target_directory)
{
    char clean_archive_path[EXTRACT_MAX_PATH] = {0};
    char clean_target_directory[EXTRACT_MAX_PATH] = {0};
    char command[EXTRACT_MAX_PATH * 2] = {0};
    LONG result;

    if (archive_path == NULL || target_directory == NULL)
    {
        return EXTRACT_RESULT_INVALID_INPUT;
    }

    strncpy(clean_archive_path, archive_path, sizeof(clean_archive_path) - 1);
    clean_archive_path[sizeof(clean_archive_path) - 1] = '\0';
    sanitize_amiga_file_path(clean_archive_path);

    strncpy(clean_target_directory, target_directory, sizeof(clean_target_directory) - 1);
    clean_target_directory[sizeof(clean_target_directory) - 1] = '\0';
    sanitize_amiga_file_path(clean_target_directory);

    if (snprintf(command,
                 sizeof(command),
                 EXTRACT_LHA_TOOL " -T -M -N -m x \"%s\" \"%s/\"",
                 clean_archive_path,
                 clean_target_directory) >= (int)sizeof(command))
    {
        log_error(LOG_GENERAL, "extract: extraction command too long\n");
        return EXTRACT_RESULT_TARGET_PATH_FAILED;
    }

    log_info(LOG_GENERAL, "extract: executing '%s'\n", command);

    result = SystemTagList(command, NULL);
    if (result != 0)
    {
        log_error(LOG_GENERAL,
                  "extract: lha extraction failed for '%s' -> '%s' (return=%ld)\n",
                  clean_archive_path,
                  clean_target_directory,
                  (long)result);
    }

    return result;
}

static LONG extract_archive_with_unlzx(const char *archive_path, const char *target_directory)
{
    char clean_archive_path[EXTRACT_MAX_PATH] = {0};
    char clean_target_directory[EXTRACT_MAX_PATH] = {0};
    char command[EXTRACT_MAX_PATH * 2] = {0};
    LONG result;

    if (archive_path == NULL || target_directory == NULL)
    {
        return EXTRACT_RESULT_INVALID_INPUT;
    }

    if (!extract_is_unlzx_available())
    {
        log_warning(LOG_GENERAL,
                    "extract: skipping LZX archive '%s' because %s is missing\n",
                    archive_path,
                    EXTRACT_UNLZX_TOOL);
        printf("Skipping LZX extraction for '%s' because %s is missing.\n", archive_path, EXTRACT_UNLZX_TOOL);
        return EXTRACT_RESULT_SKIPPED_TOOL_MISSING;
    }

    strncpy(clean_archive_path, archive_path, sizeof(clean_archive_path) - 1);
    clean_archive_path[sizeof(clean_archive_path) - 1] = '\0';
    sanitize_amiga_file_path(clean_archive_path);

    strncpy(clean_target_directory, target_directory, sizeof(clean_target_directory) - 1);
    clean_target_directory[sizeof(clean_target_directory) - 1] = '\0';
    sanitize_amiga_file_path(clean_target_directory);

    if (snprintf(command,
                 sizeof(command),
                 EXTRACT_UNLZX_TOOL " -m x \"%s\" \"%s/\"",
                 clean_archive_path,
                 clean_target_directory) >= (int)sizeof(command))
    {
        log_error(LOG_GENERAL, "extract: unlzx extraction command too long\n");
        return EXTRACT_RESULT_TARGET_PATH_FAILED;
    }

    log_info(LOG_GENERAL, "extract: executing '%s'\n", command);

    result = SystemTagList(command, NULL);
    if (result != 0)
    {
        log_error(LOG_GENERAL,
                  "extract: unlzx extraction failed for '%s' -> '%s' (return=%ld)\n",
                  clean_archive_path,
                  clean_target_directory,
                  (long)result);
    }

    return result;
}

static LONG extract_archive_with_dispatch(const char *archive_path,
                                          const char *archive_filename,
                                          const char *target_directory)
{
    archive_type detected_type;

    if (archive_path == NULL || archive_filename == NULL || target_directory == NULL)
    {
        return EXTRACT_RESULT_INVALID_INPUT;
    }

    detected_type = detect_archive_type_from_filename(archive_filename);

    if (detected_type == ARCHIVE_TYPE_LHA)
    {
        return extract_archive_with_lha(archive_path, target_directory);
    }

    if (detected_type == ARCHIVE_TYPE_LZX)
    {
        return extract_archive_with_unlzx(archive_path, target_directory);
    }

    log_warning(LOG_GENERAL,
                "extract: skipping unsupported archive type for '%s'\n",
                archive_filename);
    printf("Skipping unsupported archive type: '%s'.\n", archive_filename);
    return EXTRACT_RESULT_SKIPPED_UNSUPPORTED_ARCHIVE;
}

BOOL extract_write_archive_metadata(const char *target_directory,
                                    const char *game_folder_name,
                                    const char *category_name,
                                    const char *archive_filename)
{
    FILE *metadata_file;
    char metadata_path[EXTRACT_MAX_PATH] = {0};

    if (target_directory == NULL || game_folder_name == NULL || category_name == NULL || archive_filename == NULL)
    {
        return FALSE;
    }

    if (snprintf(metadata_path,
                 sizeof(metadata_path),
                 "%s/%s/ArchiveName.txt",
                 target_directory,
                 game_folder_name) >= (int)sizeof(metadata_path))
    {
        log_error(LOG_GENERAL, "extract: metadata path too long\n");
        return FALSE;
    }

    sanitize_amiga_file_path(metadata_path);

    metadata_file = fopen(metadata_path, "w");
    if (metadata_file == NULL)
    {
        log_error(LOG_GENERAL, "extract: failed to open metadata file '%s'\n", metadata_path);
        return FALSE;
    }

    fprintf(metadata_file, "%s\n%s\n", category_name, archive_filename);
    fclose(metadata_file);

    log_info(LOG_GENERAL, "extract: wrote metadata '%s'\n", metadata_path);
    return TRUE;
}

static BOOL extract_read_archive_name_from_metadata(const char *target_directory,
                                                    const char *game_folder_name,
                                                    char *out_archive_name,
                                                    size_t out_archive_name_size)
{
    char metadata_path[EXTRACT_MAX_PATH] = {0};
    char discard_line[EXTRACT_MAX_PATH] = {0};
    FILE *metadata_file;

    if (target_directory == NULL || game_folder_name == NULL || out_archive_name == NULL ||
        out_archive_name_size == 0)
    {
        return FALSE;
    }

    if (snprintf(metadata_path,
                 sizeof(metadata_path),
                 "%s/%s/ArchiveName.txt",
                 target_directory,
                 game_folder_name) >= (int)sizeof(metadata_path))
    {
        log_warning(LOG_GENERAL, "extract: metadata path too long while checking existing extraction\n");
        return FALSE;
    }

    sanitize_amiga_file_path(metadata_path);

    if (!does_file_or_folder_exist(metadata_path, 0))
    {
        return FALSE;
    }

    metadata_file = fopen(metadata_path, "r");
    if (metadata_file == NULL)
    {
        log_warning(LOG_GENERAL, "extract: failed to open existing metadata file '%s'\n", metadata_path);
        return FALSE;
    }

    if (fgets(discard_line, sizeof(discard_line), metadata_file) == NULL)
    {
        fclose(metadata_file);
        log_warning(LOG_GENERAL, "extract: metadata file '%s' is missing category line\n", metadata_path);
        return FALSE;
    }

    if (fgets(out_archive_name, (int)out_archive_name_size, metadata_file) == NULL)
    {
        fclose(metadata_file);
        log_warning(LOG_GENERAL, "extract: metadata file '%s' is missing archive line\n", metadata_path);
        return FALSE;
    }

    fclose(metadata_file);

    remove_CR_LF_from_string(out_archive_name);
    return TRUE;
}

static BOOL extract_find_archive_by_scanning_metadata(const char *target_directory,
                                                      const char *archive_filename,
                                                      char *out_match_folder_path,
                                                      size_t out_match_folder_path_size)
{
    BPTR directory_lock;
    struct FileInfoBlock *fib;
    char metadata_archive_name[EXTRACT_MAX_PATH] = {0};
    char folder_path[EXTRACT_MAX_PATH] = {0};

    if (target_directory == NULL || archive_filename == NULL)
    {
        return FALSE;
    }

    directory_lock = Lock(target_directory, ACCESS_READ);
    if (directory_lock == 0)
    {
        return FALSE;
    }

    fib = amiga_malloc(sizeof(struct FileInfoBlock));
    if (fib == NULL)
    {
        UnLock(directory_lock);
        return FALSE;
    }

    if (!Examine(directory_lock, fib))
    {
        amiga_free(fib);
        UnLock(directory_lock);
        return FALSE;
    }

    while (ExNext(directory_lock, fib))
    {
        if (fib->fib_DirEntryType <= 0)
        {
            continue;
        }

        if (fib->fib_FileName[0] == '.')
        {
            continue;
        }

        if (!extract_read_archive_name_from_metadata(target_directory,
                                                     fib->fib_FileName,
                                                     metadata_archive_name,
                                                     sizeof(metadata_archive_name)))
        {
            continue;
        }

        if (strcmp(metadata_archive_name, archive_filename) == 0)
        {
            if (out_match_folder_path != NULL && out_match_folder_path_size > 0)
            {
                if (snprintf(folder_path,
                             sizeof(folder_path),
                             "%s/%s",
                             target_directory,
                             fib->fib_FileName) < (int)sizeof(folder_path))
                {
                    sanitize_amiga_file_path(folder_path);
                    strncpy(out_match_folder_path, folder_path, out_match_folder_path_size - 1);
                    out_match_folder_path[out_match_folder_path_size - 1] = '\0';
                }
            }

            amiga_free(fib);
            UnLock(directory_lock);
            return TRUE;
        }
    }

    amiga_free(fib);
    UnLock(directory_lock);
    return FALSE;
}

BOOL extract_is_archive_already_extracted(const char *archive_path,
                                          const char *archive_filename,
                                          const char *pack_dir,
                                          const char *first_letter,
                                          const download_option *download_options,
                                          char *out_match_folder_path,
                                          size_t out_match_folder_path_size)
{
    char target_directory[EXTRACT_MAX_PATH] = {0};
    char heuristic_folder_name[EXTRACT_MAX_NAME] = {0};
    char indexed_folder_name[EXTRACT_MAX_NAME] = {0};
    char metadata_archive_name[EXTRACT_MAX_PATH] = {0};
    char verify_path[EXTRACT_MAX_PATH] = {0};
    BOOL index_loaded;

    if (archive_path == NULL || archive_filename == NULL || pack_dir == NULL || first_letter == NULL ||
        download_options == NULL)
    {
        return FALSE;
    }

    if (download_options->extract_archives == FALSE ||
        download_options->skip_download_if_extracted == FALSE ||
        download_options->force_download == TRUE)
    {
        return FALSE;
    }

    if (!extract_build_target_directory(archive_path,
                                        pack_dir,
                                        first_letter,
                                        download_options,
                                        target_directory,
                                        sizeof(target_directory)))
    {
        return FALSE;
    }

    if (download_options->verify_archive_marker_before_download == TRUE &&
        extract_derive_game_folder_name(archive_filename,
                                        heuristic_folder_name,
                                        sizeof(heuristic_folder_name)))
    {
        if (extract_read_archive_name_from_metadata(target_directory,
                                                    heuristic_folder_name,
                                                    metadata_archive_name,
                                                    sizeof(metadata_archive_name)) &&
            strcmp(metadata_archive_name, archive_filename) == 0)
        {
            if (out_match_folder_path != NULL && out_match_folder_path_size > 0)
            {
                snprintf(out_match_folder_path,
                         out_match_folder_path_size,
                         "%s/%s",
                         target_directory,
                         heuristic_folder_name);
                sanitize_amiga_file_path(out_match_folder_path);
            }

            return TRUE;
        }
    }

    /* Tier 2: archive index lookup (fast path). */
    index_loaded = extract_index_load(target_directory);
    if (index_loaded)
    {
        if (extract_index_lookup(target_directory,
                                 archive_filename,
                                 indexed_folder_name,
                                 sizeof(indexed_folder_name)))
        {
            if (snprintf(verify_path,
                         sizeof(verify_path),
                         "%s/%s",
                         target_directory,
                         indexed_folder_name) < (int)sizeof(verify_path))
            {
                BPTR verify_lock;

                sanitize_amiga_file_path(verify_path);
                verify_lock = Lock(verify_path, ACCESS_READ);
                if (verify_lock != 0)
                {
                    UnLock(verify_lock);

                    if (out_match_folder_path != NULL && out_match_folder_path_size > 0)
                    {
                        strncpy(out_match_folder_path, verify_path, out_match_folder_path_size - 1);
                        out_match_folder_path[out_match_folder_path_size - 1] = '\0';
                    }

                    return TRUE;
                }

                /* Stale index entry: folder was removed manually. */
                if (index_remove_entry_by_archive_name(&g_archive_index_cache, archive_filename))
                {
                    log_debug(LOG_GENERAL,
                              "extract-index: removed stale entry for '%s' in '%s'\n",
                              archive_filename,
                              target_directory);
                }
            }
        }

        return FALSE;
    }

    if (download_options->verify_archive_marker_before_download == FALSE)
    {
        return FALSE;
    }

    /* Fallback only if the index could not be loaded at all. */
    return extract_find_archive_by_scanning_metadata(target_directory,
                                                     archive_filename,
                                                     out_match_folder_path,
                                                     out_match_folder_path_size);
}

LONG extract_process_downloaded_archive(const char *archive_path,
                                        const char *archive_filename,
                                        const char *pack_dir,
                                        const char *first_letter,
                                        const char *category_name,
                                        const download_option *download_options)
{
    char target_directory[EXTRACT_MAX_PATH] = {0};
    char game_folder_name[EXTRACT_MAX_NAME] = {0};
    char existing_archive_name[EXTRACT_MAX_PATH] = {0};
    archive_type detected_type;
    LONG extract_result;

    if (archive_path == NULL || archive_filename == NULL || pack_dir == NULL || first_letter == NULL ||
        category_name == NULL || download_options == NULL)
    {
        return EXTRACT_RESULT_INVALID_INPUT;
    }

    if (!extract_build_target_directory(archive_path,
                                        pack_dir,
                                        first_letter,
                                        download_options,
                                        target_directory,
                                        sizeof(target_directory)))
    {
        log_error(LOG_GENERAL, "extract: failed to resolve target directory for '%s'\n", archive_filename);
        return EXTRACT_RESULT_TARGET_PATH_FAILED;
    }

    if (!extract_derive_game_folder_name(archive_filename, game_folder_name, sizeof(game_folder_name)))
    {
        game_folder_name[0] = '\0';
    }

    detected_type = detect_archive_type_from_filename(archive_filename);
    if (detected_type == ARCHIVE_TYPE_UNKNOWN)
    {
        log_warning(LOG_GENERAL,
                    "extract: unsupported archive extension for '%s', skipping\n",
                    archive_filename);
        printf("Skipping unsupported archive extension for '%s'.\n", archive_filename);
        return EXTRACT_RESULT_SKIPPED_UNSUPPORTED_ARCHIVE;
    }

    if ((detected_type == ARCHIVE_TYPE_LHA &&
         extract_get_top_level_directory_from_lha(archive_path, game_folder_name, sizeof(game_folder_name))) ||
        (detected_type == ARCHIVE_TYPE_LZX &&
         extract_get_top_level_directory_from_lzx(archive_path, game_folder_name, sizeof(game_folder_name))))
    {
        log_debug(LOG_GENERAL,
                  "extract: using archive-listed top-level folder '%s' for '%s'\n",
                  game_folder_name,
                  archive_filename);
    }
    else if (game_folder_name[0] != '\0')
    {
        log_debug(LOG_GENERAL,
                  "extract: using filename heuristic folder '%s' for '%s'\n",
                  game_folder_name,
                  archive_filename);
    }
    else
    {
        log_error(LOG_GENERAL, "extract: failed to derive folder name from '%s'\n", archive_filename);
        return EXTRACT_RESULT_INVALID_INPUT;
    }

    if (download_options->skip_existing_extractions == TRUE && download_options->force_extract == FALSE)
    {
        if (extract_read_archive_name_from_metadata(target_directory,
                                                    game_folder_name,
                                                    existing_archive_name,
                                                    sizeof(existing_archive_name)))
        {
            if (strcmp(existing_archive_name, archive_filename) == 0)
            {
                log_info(LOG_GENERAL,
                         "extract: skipped '%s' because ArchiveName.txt already matches\n",
                         archive_filename);
                printf("Skip extract (same archive): %s\n", archive_filename);
                return EXTRACT_RESULT_OK;
            }

            log_debug(LOG_GENERAL,
                      "extract: metadata mismatch for '%s' (found '%s'), re-extracting\n",
                      archive_filename,
                      existing_archive_name);
        }
    }
    else if (download_options->force_extract == TRUE)
    {
        log_debug(LOG_GENERAL, "extract: FORCEEXTRACT active for '%s'\n", archive_filename);
    }

    extract_prepare_existing_target_folder(target_directory, game_folder_name);

    extract_result = extract_archive_with_dispatch(archive_path, archive_filename, target_directory);
    if (extract_result != 0)
    {
        return extract_result;
    }

    extract_apply_whdload_folder_icon(target_directory, game_folder_name, download_options);

    if (!extract_write_archive_metadata(target_directory,
                                        game_folder_name,
                                        category_name,
                                        archive_filename))
    {
        return EXTRACT_RESULT_METADATA_FAILED;
    }

    if (!extract_index_update(target_directory, archive_filename, game_folder_name))
    {
        log_warning(LOG_GENERAL,
                    "extract-index: failed to update index for '%s' in '%s'\n",
                    archive_filename,
                    target_directory);
    }

    if (!extract_delete_archive_if_needed(archive_path, download_options))
    {
        return EXTRACT_RESULT_DELETE_FAILED;
    }

    return EXTRACT_RESULT_OK;
}
