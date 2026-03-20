#include "platform/platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "phases/dat_phase.h"
#include "cli_utilities.h"
#include "gamefile_parser.h"
#include "linecounter.h"
#include "log/log.h"
#include "utilities.h"

static int should_include_game(const game_metadata *game)
{
    if (strcmp(game->machineType, "AGA") == 0 && skip_AGA == 1)
    {
        return 0;
    }

    if (((strcmp(game->mediaFormat, "CD32") == 0) ||
         (strcmp(game->mediaFormat, "CDTV") == 0)) &&
        skip_CD == 1)
    {
        return 0;
    }

    if (strcmp(game->videoFormat, "NTSC") == 0 && skip_NTSC == 1)
    {
        return 0;
    }

    if (skip_NonEnglish == 1 && strcmp(game->language, "English") != 0)
    {
        return 0;
    }

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

BOOL extract_Zip_file_and_rename(const char *zipPath,
                                 struct whdload_pack_def WHDLoadPackDefs[],
                                 int size_WHDLoadPackDef,
                                 int verboseMode)
{
    struct FileInfoBlock *fib;
    BOOL found = FALSE;
    STRPTR fileName;
    LONG nameLen;
    BPTR lock;
    char unzipCommand[256] = {0};
    char datFileName[256] = {0};
    char zipFileDate[11] = {0};
    int counter = 0;
    int i;

    if (!does_file_or_folder_exist(zipPath, 0))
    {
        printf("Zipfile '%s' not found!\n", zipPath);
        return FALSE;
    }

    lock = Lock(zipPath, ACCESS_READ);
    if (!lock)
        return FALSE;

    fib = amiga_malloc(sizeof(struct FileInfoBlock));
    if (!fib)
    {
        printf("Failed to allocate memory for FileInfoBlock.\n");
        UnLock(lock);
        return FALSE;
    }

    if (!Examine(lock, fib))
    {
        amiga_free(fib);
        UnLock(lock);
        return FALSE;
    }

    while (ExNext(lock, fib))
    {
        if (fib->fib_DirEntryType >= 0)
            continue;

        fileName = fib->fib_FileName;
        nameLen = strlen(fileName);

        if (nameLen <= 3 || stricmp_custom(&fileName[nameLen - 4], ".zip") != 0)
            continue;

        for (i = 0; i < size_WHDLoadPackDef; i++)
        {
            if (!strstr(fileName, WHDLoadPackDefs[i].filter_dat_files))
                continue;
            if (WHDLoadPackDefs[i].user_requested_download != 1)
                continue;
            if (WHDLoadPackDefs[i].updated_dat_downloaded != 1)
                continue;

            counter++;
            extract_date_from_filename(fileName, zipFileDate, sizeof(zipFileDate));

            if (WHDLoadPackDefs[i].filter_dat_files[strlen(WHDLoadPackDefs[i].filter_dat_files) - 1] == '(')
            {
                sprintf(datFileName, "%s/%s%s).xml", DIR_DAT_FILES, WHDLoadPackDefs[i].filter_dat_files, zipFileDate);
            }
            else
            {
                sprintf(datFileName, "%s/%s(%s).xml", DIR_DAT_FILES, WHDLoadPackDefs[i].filter_dat_files, zipFileDate);
            }

            found = TRUE;

            if (verboseMode == 1)
            {
                sprintf(unzipCommand, "unzip -a -p  \"%s/%s\" >\"%s\"", DIR_ZIP_FILES, fileName, datFileName);
            }
            else
            {
                sprintf(unzipCommand, "unzip -qq -a -p  \"%s/%s\" >\"%s\"", DIR_ZIP_FILES, fileName, datFileName);
            }

            SetVar("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 29, GVF_LOCAL_ONLY);

            printf("Extracting DATs from %s (%s)...                    ", WHDLoadPackDefs[i].full_text_name_of_pack, zipFileDate);
            fflush(stdout);

            SystemTagList(unzipCommand, NULL);
            ClearLine_SetCursorToStartOfLine();
        }
    }

    printf("Extracted DAT files from %d zip files.       \n", counter);

    amiga_free(fib);
    UnLock(lock);
    return found;
}

BOOL process_and_archive_WHDLoadDat_Files(struct whdload_pack_def whdload_pack_defs[], int num_packs)
{
    struct FileInfoBlock *fib;
    BOOL has_file_been_processed = FALSE;
    STRPTR file_name;
    LONG name_len;
    BPTR directory_lock;
    char file_path[256];
    char output_file_path[256];
    int i;

    directory_lock = Lock(DIR_DAT_FILES, ACCESS_READ);
    if (!directory_lock)
    {
        printf("Failed to lock the directory: %s\n", DIR_DAT_FILES);
        return FALSE;
    }

    fib = amiga_malloc(sizeof(struct FileInfoBlock));
    if (!fib)
    {
        printf("Failed to allocate memory for FileInfoBlock.\n");
        UnLock(directory_lock);
        return FALSE;
    }

    if (Examine(directory_lock, fib))
    {
        while (ExNext(directory_lock, fib))
        {
            if (fib->fib_DirEntryType < 0)
            {
                file_name = fib->fib_FileName;
                name_len = strlen(file_name);

                if (name_len > 4 && stricmp_custom(file_name + name_len - 4, ".xml") == 0)
                {
                    for (i = 0; i < num_packs; i++)
                    {
                        if (strstr(file_name, whdload_pack_defs[i].filter_dat_files) &&
                            whdload_pack_defs[i].user_requested_download == 1 &&
                            whdload_pack_defs[i].updated_dat_downloaded == 1)
                        {
                            char temp_file_name[256];
                            has_file_been_processed = TRUE;

                            sprintf(file_path, "%s/%s", DIR_DAT_FILES, file_name);

                            strncpy(temp_file_name, file_name, name_len - 4);
                            temp_file_name[name_len - 4] = '\0';
                            sprintf(output_file_path, "%s/%s.txt", DIR_DAT_FILES, temp_file_name);

                            whdload_pack_defs[i].file_count = extract_and_save_rom_names_from_XML(
                                file_path,
                                output_file_path,
                                whdload_pack_defs[i].full_text_name_of_pack);

                            DeleteFile(file_path);
                        }
                    }
                }
            }
        }
    }

    amiga_free(fib);
    UnLock(directory_lock);

    return has_file_been_processed;
}

int extract_and_save_rom_names_from_XML(char *input_file_path,
                                        char *output_file_path,
                                        const char *pack_name)
{
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    char line[1024];
    char *rom_start = NULL;
    char *name_attr_start = NULL;
    char *name_attr_end = NULL;
    char name_value[1024];
    int name_length = 0;
    game_metadata game;
    int file_count = 0;
    long line_count_of_file = 0;
    long line_count = 0;
    time_t last_update_time = 0;
    time_t current_time;
    char percent_str[32];
    char size_value[32];
    char crc_value[64];

    DeleteFile(output_file_path);

    line_count_of_file = CountLines(input_file_path);

    input_file = fopen(input_file_path, "r");
    output_file = fopen(output_file_path, "w");

    if (input_file == NULL || output_file == NULL)
    {
        perror("Error opening file");
        if (input_file != NULL)
            fclose(input_file);
        if (output_file != NULL)
            fclose(output_file);
        return 0;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    line_count = 0;
    last_update_time = time(NULL);

    while (fgets(line, sizeof(line), input_file) != NULL)
    {
        rom_start = strstr(line, "<rom");
        if (rom_start != NULL)
        {
            name_attr_start = strstr(rom_start, "name=\"");
            if (name_attr_start != NULL)
            {
                name_attr_start += 6;
                name_attr_end = strchr(name_attr_start, '"');

                if (name_attr_end != NULL)
                {
                    name_length = name_attr_end - name_attr_start;
                    strncpy(name_value, name_attr_start, name_length);
                    name_value[name_length] = '\0';

                    remove_all_occurrences(name_value, "amp;");

                    extract_game_info_from_filename(name_value, &game);

                    if (should_include_game(&game))
                    {
                        BOOL has_size;
                        BOOL has_crc;

                        size_value[0] = '\0';
                        crc_value[0] = '\0';
                        has_size = extract_xml_attribute_value(rom_start, "size", size_value, sizeof(size_value));
                        has_crc = extract_xml_attribute_value(rom_start, "crc", crc_value, sizeof(crc_value));

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

        line_count++;

        current_time = time(NULL);
        if (current_time - last_update_time >= 1)
        {
            sprintf(percent_str, "\r%s progress: %d %%",
                    pack_name, (int)((line_count * 100) / line_count_of_file));
            printf("%s", percent_str);
            last_update_time = current_time;
        }
    }

    printf("\r%s complete      \n", pack_name);

    fclose(input_file);
    fclose(output_file);

    return file_count;
}
