#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "download/amiga_download.h"
#include "download/download_retry.h"
#include "extract/extract.h"
#include "gamefile_parser.h"
#include "linecounter.h"
#include "log/log.h"
#include "phases/download_phase.h"
#include "report/report.h"
#include "utilities.h"

static BOOL is_nonfatal_extract_skip(LONG extract_code)
{
    return (extract_code == EXTRACT_RESULT_SKIPPED_TOOL_MISSING ||
            extract_code == EXTRACT_RESULT_SKIPPED_UNSUPPORTED_ARCHIVE) ? TRUE : FALSE;
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

LONG count_total_queued_entries_for_selected_packs(struct whdload_pack_def whdload_pack_defs[],
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

ULONG sum_total_queued_kb_for_selected_packs(struct whdload_pack_def whdload_pack_defs[],
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

void sum_queued_kb_per_pack(struct whdload_pack_def whdload_pack_defs[],
                            int num_packs,
                            ULONG *out_kb_per_pack)
{
    int i;

    if (whdload_pack_defs == NULL || out_kb_per_pack == NULL || num_packs <= 0)
    {
        return;
    }

    for (i = 0; i < num_packs; i++)
    {
        char list_path[256] = {0};

        out_kb_per_pack[i] = 0;

        if (whdload_pack_defs[i].user_requested_download != 1)
        {
            continue;
        }

        if (!build_pack_text_list_path(&whdload_pack_defs[i], list_path, sizeof(list_path)))
        {
            log_warning(LOG_GENERAL,
                        "estimate: no DAT list found for pack '%s'\n",
                        whdload_pack_defs[i].full_text_name_of_pack);
            continue;
        }

        out_kb_per_pack[i] = sum_queued_bytes_from_list_file(list_path);
    }
}

void print_space_estimate(struct whdload_pack_def whdload_pack_defs[],
                          int num_packs,
                          const ULONG *kb_per_pack)
{
    ULONG total_archive_kb  = 0;
    ULONG total_extracted_kb = 0;
    ULONG keep_kb, keep_mb_whole, keep_mb_tenths;
    ULONG tot_arch_mb_whole, tot_arch_mb_tenths;
    ULONG tot_ext_mb_whole, tot_ext_mb_tenths;
    int i;

    if (whdload_pack_defs == NULL || kb_per_pack == NULL || num_packs <= 0)
    {
        return;
    }

    printf("\n" textReset textBold "=== Disk Space Estimate ===" textReset "\n\n");

    for (i = 0; i < num_packs; i++)
    {
        ULONG archive_kb, extracted_kb;
        ULONG arch_mb_whole, arch_mb_tenths;
        ULONG ext_mb_whole,  ext_mb_tenths;

        if (whdload_pack_defs[i].user_requested_download != 1)
        {
            continue;
        }

        archive_kb   = kb_per_pack[i];
        extracted_kb = archive_kb + (archive_kb / 2UL); /* 1.5x estimate */

        arch_mb_whole  = archive_kb   / 1024UL;
        arch_mb_tenths = ((archive_kb   % 1024UL) * 10UL) / 1024UL;
        ext_mb_whole   = extracted_kb  / 1024UL;
        ext_mb_tenths  = ((extracted_kb  % 1024UL) * 10UL) / 1024UL;

        printf(textBold "%s:" textReset "\n", whdload_pack_defs[i].full_text_name_of_pack);
        printf("  Archive size:           %ld.%ld MB\n",
               (long)arch_mb_whole, (long)arch_mb_tenths);
        printf("  Extracted (estimate):   %ld.%ld MB\n\n",
               (long)ext_mb_whole,  (long)ext_mb_tenths);

        if ((ULONG)(0xFFFFFFFFUL - total_archive_kb) < archive_kb)
            total_archive_kb = 0xFFFFFFFFUL;
        else
            total_archive_kb += archive_kb;

        if ((ULONG)(0xFFFFFFFFUL - total_extracted_kb) < extracted_kb)
            total_extracted_kb = 0xFFFFFFFFUL;
        else
            total_extracted_kb += extracted_kb;
    }

    tot_arch_mb_whole  = total_archive_kb  / 1024UL;
    tot_arch_mb_tenths = ((total_archive_kb  % 1024UL) * 10UL) / 1024UL;
    tot_ext_mb_whole   = total_extracted_kb / 1024UL;
    tot_ext_mb_tenths  = ((total_extracted_kb % 1024UL) * 10UL) / 1024UL;

    printf("------------------------------------------------------------\n");
    printf(textBold "Total archive size:       %ld.%ld MB\n" textReset,
           (long)tot_arch_mb_whole, (long)tot_arch_mb_tenths);
    printf(textBold "Total extracted (est.):   %ld.%ld MB\n\n" textReset,
           (long)tot_ext_mb_whole,  (long)tot_ext_mb_tenths);

    keep_kb = total_archive_kb;
    if ((ULONG)(0xFFFFFFFFUL - keep_kb) < total_extracted_kb)
        keep_kb = 0xFFFFFFFFUL;
    else
        keep_kb += total_extracted_kb;

    keep_mb_whole  = keep_kb / 1024UL;
    keep_mb_tenths = ((keep_kb % 1024UL) * 10UL) / 1024UL;

    printf("Extracted size is estimated at 1.5x the archive size (rough guide).\n");
    printf("If using KEEPARCHIVES, you will need %ld.%ld MB total (archives + extracted).\n\n",
           (long)keep_mb_whole, (long)keep_mb_tenths);

    /* Write the same summary to the log file so it appears in PROGDIR:logs/. */
    log_info(LOG_GENERAL, "estimate: archive total %ld.%ld MB  extracted-est %ld.%ld MB  keeparchives-total %ld.%ld MB\n",
             (long)tot_arch_mb_whole, (long)tot_arch_mb_tenths,
             (long)tot_ext_mb_whole,  (long)tot_ext_mb_tenths,
             (long)keep_mb_whole,     (long)keep_mb_tenths);

    for (i = 0; i < num_packs; i++)
    {
        ULONG arch_kb, ext_kb, arch_mw, arch_mt, ext_mw, ext_mt;

        if (whdload_pack_defs[i].user_requested_download != 1)
        {
            continue;
        }

        arch_kb = kb_per_pack[i];
        ext_kb  = arch_kb + (arch_kb / 2UL);
        arch_mw = arch_kb / 1024UL;
        arch_mt = ((arch_kb % 1024UL) * 10UL) / 1024UL;
        ext_mw  = ext_kb  / 1024UL;
        ext_mt  = ((ext_kb  % 1024UL) * 10UL) / 1024UL;

        log_info(LOG_GENERAL, "estimate: pack='%s' archive=%ld.%ld MB extracted-est=%ld.%ld MB\n",
                 whdload_pack_defs[i].full_text_name_of_pack,
                 (long)arch_mw, (long)arch_mt,
                 (long)ext_mw,  (long)ext_mt);
    }
}

char *get_first_matching_fileName(const char *fileNameToFind)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char pattern[256], *fullFilename = NULL;

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
                if (strncasecmp_custom(pattern, fib->fib_FileName, strlen(pattern)) == 0)
                {
                    fullFilename = amiga_malloc(strlen(fib->fib_FileName) + 1);
                    if (fullFilename)
                    {
                        strcpy(fullFilename, fib->fib_FileName);
                    }
                    break;
                }
            }
        }
    }

    UnLock(lock);
    amiga_free(fib);
    return fullFilename;
}

LONG download_roms_if_file_exists(struct whdload_pack_def *WHDLoadPackDefs,
                                  const struct download_option *download_options,
                                  int replaceFiles,
                                  download_progress_state *progress_state)
{
    char foundFilename[256] = {0};
    char *fileNameToRead = get_first_matching_fileName(WHDLoadPackDefs->filter_dat_files);
    LONG returnCode = 0;

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

LONG download_roms_from_file(const char *filename,
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

    if (has_download_marker(fileName))
    {
        printf(textReset "Incomplete download detected, re-downloading: %s\n", downloadWHDFile);
        log_info(LOG_DOWNLOAD,
                 "download: found .downloading marker for '%s', deleting partial file and re-downloading\n",
                 downloadWHDFile);
        DeleteFile((STRPTR)fileName);
        delete_download_marker(fileName);
    }

    if (does_file_or_folder_exist(fileName, 1) &&
        !(download_options != NULL && download_options->force_download == TRUE))
    {
        if (no_skip_messages < 1)
        {
            printf(textReset "Skip download (exists): %s\n", downloadWHDFile);
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
            printf(textReset "Skip dl (extracted): %s\n", downloadWHDFile);
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

    returnCode = download_archive_with_retry(downloadUrl,
                                             fileName,
                                             downloadWHDFile,
                                             download_silent,
                                             (download_options != NULL && download_options->crc_check == TRUE) ? TRUE : FALSE,
                                             archive_crc);

    if (returnCode == 20)
    {
        return 20;
    }

    if (returnCode != AD_SUCCESS)
    {
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
