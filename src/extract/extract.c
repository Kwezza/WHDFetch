#include <stdio.h>
#include <string.h>

#include "extract.h"
#include "log/log.h"
#include "platform/platform.h"
#include "utilities.h"

#define EXTRACT_LISTING_FILE "T:whdd_lha_listing.txt"

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
        char *slash = strchr(line, '/');
        if (slash != NULL)
        {
            size_t len = (size_t)(slash - line);
            if (len > 0 && len < out_folder_name_size)
            {
                strncpy(out_folder_name, line, len);
                out_folder_name[len] = '\0';
                remove_CR_LF_from_string(out_folder_name);
                trim(out_folder_name);
                sanitize_amiga_file_path(out_folder_name);
                fclose(listing_file);
                DeleteFile(EXTRACT_LISTING_FILE);
                return (out_folder_name[0] != '\0') ? TRUE : FALSE;
            }
        }
    }

    fclose(listing_file);
    DeleteFile(EXTRACT_LISTING_FILE);
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
                 "c:lha -T -M -N -m x \"%s\" \"%s/\"",
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
    char metadata_archive_name[EXTRACT_MAX_PATH] = {0};

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

    if (extract_derive_game_folder_name(archive_filename,
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

    if (extract_get_top_level_directory_from_lha(archive_path, game_folder_name, sizeof(game_folder_name)))
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
                printf("Skipped extraction for '%s' (ArchiveName.txt match).\n", archive_filename);
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

    extract_result = extract_archive_with_lha(archive_path, target_directory);
    if (extract_result != 0)
    {
        return extract_result;
    }

    if (!extract_write_archive_metadata(target_directory,
                                        game_folder_name,
                                        category_name,
                                        archive_filename))
    {
        return EXTRACT_RESULT_METADATA_FAILED;
    }

    if (!extract_delete_archive_if_needed(archive_path, download_options))
    {
        return EXTRACT_RESULT_DELETE_FAILED;
    }

    return EXTRACT_RESULT_OK;
}
