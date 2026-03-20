#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#include "phases/html_phase.h"
#include "download/amiga_download.h"
#include "log/log.h"
#include "utilities.h"

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

static int html_phase_compare_and_decide_dat_file_download(char *fileName,
                                                           const char *searchText,
                                                           const char *dir_zip_files)
{
    char tempDateStr[11];
    char longDate[50];
    long oldFileDate = 0;
    long newFileDate = 0;
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
    Get_latest_filename_from_directory(dir_zip_files, searchText, tempFileName);
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

        printf("Already downloaded, skipping download.\n");
        log_info(LOG_GENERAL, "dat-check: local file is up-to-date, skipping download\n");
        return 0;
    }

    printf("Local file not found, downloading now...\n");
    log_info(LOG_GENERAL, "dat-check: no local file found for filter='%s', will download\n", searchText);
    return 1;
}

static int html_phase_download_packs_from_links(const char *bufferm,
                                                struct whdload_pack_def WHDLoadPackDefs[],
                                                int size_WHDLoadPackDef,
                                                const char *dir_zip_files)
{
    char *startPos;
    char link[MAX_LINK_LENGTH];
    char fileName[128] = {0};
    int downloadFile = 0;
    int i;
    int any_pack_match = 0;
    int download_result;
    char file_path_to_save[256];
    char download_address[256];

    startPos = strstr(bufferm, "<a href=\"");
    if (startPos != NULL)
    {
        char *endPos = strstr(startPos, "\">");
        if (endPos != NULL)
        {
            int linkLength = endPos - (startPos + 9);
            if (linkLength < MAX_LINK_LENGTH)
            {
                strncpy(link, startPos + 9, linkLength);
                link[linkLength] = '\0';
                g_html_scan_stats.links_parsed++;
                log_debug(LOG_GENERAL, "html: found href link='%s'\n", link);

                if (HTML_LINK_PREFIX_FILTER != NULL && strstr(link, HTML_LINK_PREFIX_FILTER) != NULL)
                {
                    g_html_scan_stats.prefix_matches++;
                    if (HTML_LINK_CONTENT_FILTER != NULL && strstr(link, HTML_LINK_CONTENT_FILTER))
                    {
                        int cleanup_index;
                        char date_buf[11] = {0};

                        g_html_scan_stats.content_matches++;

                        remove_all_occurrences(link, "amp;");
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
                        downloadFile = 0;
                        for (i = 0; i < size_WHDLoadPackDef; i++)
                        {
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
                                    downloadFile = html_phase_compare_and_decide_dat_file_download(fileName,
                                                                                                    WHDLoadPackDefs[i].filter_dat_files,
                                                                                                    dir_zip_files);
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
                            sprintf(file_path_to_save, "%s/%s", dir_zip_files, fileName);
                            sprintf(download_address, "%s/%s", DOWNLOAD_WEBSITE, link);
                            g_html_scan_stats.downloads_triggered++;
                            log_info(LOG_GENERAL, "html: downloading dat zip from '%s' to '%s'\n", download_address, file_path_to_save);
                            download_result = ad_download_http_file_with_retries(download_address, file_path_to_save, TRUE);
                            log_info(LOG_GENERAL, "html: download result=%ld file='%s'\n", (long)download_result, fileName);
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

void html_phase_extract_and_validate_links(struct whdload_pack_def WHDLoadPackDefs[],
                                           int size_WHDLoadPackDef,
                                           const char *dir_temp,
                                           const char *dir_zip_files)
{
    FILE *file;
    char buffer[BUFFER_SIZE];
    char file_path_to_html_file[256];

    if (dir_temp == NULL || dir_zip_files == NULL)
    {
        return;
    }

    sprintf(file_path_to_html_file, "%s/index.html", dir_temp);

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

    while (fgets(buffer, BUFFER_SIZE, file) != NULL)
    {
        g_html_scan_stats.lines_scanned++;
        if (strstr(buffer, "<a href=\"") == NULL)
        {
            continue;
        }

        g_html_scan_stats.href_lines_found++;
        html_phase_download_packs_from_links(buffer, WHDLoadPackDefs, size_WHDLoadPackDef, dir_zip_files);
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
