#include <stdio.h>
#include <string.h>
#include <time.h>

#include "report.h"
#include "platform/platform.h"
#include "platform/amiga_headers.h"
#include "utilities.h"

#define REPORT_MAX_FILENAME 256
#define REPORT_INITIAL_CAPACITY 64

typedef struct report_entry {
    char archive_name[REPORT_MAX_FILENAME];
    char old_archive_name[REPORT_MAX_FILENAME];
    char skip_reason[REPORT_MAX_FILENAME];
    const char *pack_name;
    BOOL is_update;
    BOOL is_local_cache_reuse;
    BOOL is_extraction_skip;
} report_entry;

typedef struct session_report {
    report_entry *entries;
    int count;
    int capacity;
} session_report;

static session_report g_session_report = {0};

static BOOL report_ensure_capacity(int required_capacity)
{
    int i;
    int new_capacity;
    report_entry *new_entries;

    if (required_capacity <= g_session_report.capacity)
    {
        return TRUE;
    }

    new_capacity = (g_session_report.capacity > 0) ? g_session_report.capacity : REPORT_INITIAL_CAPACITY;
    while (new_capacity < required_capacity)
    {
        new_capacity *= 2;
    }

    new_entries = amiga_malloc((size_t)new_capacity * sizeof(report_entry));
    if (new_entries == NULL)
    {
        return FALSE;
    }

    memset(new_entries, 0, (size_t)new_capacity * sizeof(report_entry));

    for (i = 0; i < g_session_report.count; i++)
    {
        new_entries[i] = g_session_report.entries[i];
    }

    if (g_session_report.entries != NULL)
    {
        amiga_free(g_session_report.entries);
    }

    g_session_report.entries = new_entries;
    g_session_report.capacity = new_capacity;
    return TRUE;
}

static BOOL report_ensure_updates_dir(void)
{
    BPTR lock;

    if (does_file_or_folder_exist("updates", 0))
    {
        return TRUE;
    }

    lock = CreateDir("updates");
    if (lock == 0)
    {
        return FALSE;
    }

    UnLock(lock);
    return TRUE;
}

void report_init(void)
{
    report_cleanup();
}

void report_add(const char *archive_name,
                const char *pack_name,
                const char *old_archive_name)
{
    report_entry *entry;

    if (archive_name == NULL || archive_name[0] == '\0' ||
        pack_name == NULL || pack_name[0] == '\0')
    {
        return;
    }

    if (!report_ensure_capacity(g_session_report.count + 1))
    {
        return;
    }

    entry = &g_session_report.entries[g_session_report.count];
    memset(entry, 0, sizeof(*entry));

    strncpy(entry->archive_name, archive_name, REPORT_MAX_FILENAME - 1);
    entry->archive_name[REPORT_MAX_FILENAME - 1] = '\0';
    entry->pack_name = pack_name;

    if (old_archive_name != NULL && old_archive_name[0] != '\0')
    {
        strncpy(entry->old_archive_name, old_archive_name, REPORT_MAX_FILENAME - 1);
        entry->old_archive_name[REPORT_MAX_FILENAME - 1] = '\0';
        entry->is_update = TRUE;
    }

    g_session_report.count++;
}

void report_add_local_cache_reuse(const char *archive_name,
                                  const char *pack_name)
{
    report_entry *entry;

    if (archive_name == NULL || archive_name[0] == '\0' ||
        pack_name == NULL || pack_name[0] == '\0')
    {
        return;
    }

    if (!report_ensure_capacity(g_session_report.count + 1))
    {
        return;
    }

    entry = &g_session_report.entries[g_session_report.count];
    memset(entry, 0, sizeof(*entry));

    strncpy(entry->archive_name, archive_name, REPORT_MAX_FILENAME - 1);
    entry->archive_name[REPORT_MAX_FILENAME - 1] = '\0';
    entry->pack_name = pack_name;
    entry->is_local_cache_reuse = TRUE;

    g_session_report.count++;
}

void report_add_extraction_skip(const char *archive_name,
                                const char *pack_name,
                                const char *reason)
{
    report_entry *entry;

    if (archive_name == NULL || archive_name[0] == '\0' ||
        pack_name == NULL || pack_name[0] == '\0' ||
        reason == NULL || reason[0] == '\0')
    {
        return;
    }

    if (!report_ensure_capacity(g_session_report.count + 1))
    {
        return;
    }

    entry = &g_session_report.entries[g_session_report.count];
    memset(entry, 0, sizeof(*entry));

    strncpy(entry->archive_name, archive_name, REPORT_MAX_FILENAME - 1);
    entry->archive_name[REPORT_MAX_FILENAME - 1] = '\0';
    strncpy(entry->skip_reason, reason, REPORT_MAX_FILENAME - 1);
    entry->skip_reason[REPORT_MAX_FILENAME - 1] = '\0';
    entry->pack_name = pack_name;
    entry->is_extraction_skip = TRUE;

    g_session_report.count++;
}

void report_write(void)
{
    FILE *report_file;
    char report_path[128] = {0};
    char timestamp[32] = {0};
    time_t now;
    struct tm *tm_info;
    const char **pack_names;
    int pack_count;
    int i;
    int j;
    int total_new = 0;
    int total_updated = 0;
    int total_local_cache_reuse = 0;
    int total_extraction_skipped = 0;

    if (g_session_report.count < 1)
    {
        printf("No new archives this session.\n");
        return;
    }

    if (!report_ensure_updates_dir())
    {
        printf("Failed to create updates directory; session report was not written.\n");
        return;
    }

    now = time(NULL);
    tm_info = localtime(&now);
    if (tm_info == NULL)
    {
        printf("Failed to generate timestamp; session report was not written.\n");
        return;
    }

    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info) == 0)
    {
        printf("Failed to format timestamp; session report was not written.\n");
        return;
    }

    if (snprintf(report_path, sizeof(report_path), "updates/updates_%s.txt", timestamp) >= (int)sizeof(report_path))
    {
        printf("Failed to create report path; session report was not written.\n");
        return;
    }

    report_file = fopen(report_path, "w");
    if (report_file == NULL)
    {
        printf("Failed to write session report file: %s\n", report_path);
        return;
    }

    pack_names = amiga_malloc((size_t)g_session_report.count * sizeof(char *));
    if (pack_names == NULL)
    {
        fclose(report_file);
        printf("Failed to allocate report memory; session report was not written.\n");
        return;
    }

    memset(pack_names, 0, (size_t)g_session_report.count * sizeof(char *));
    pack_count = 0;

    for (i = 0; i < g_session_report.count; i++)
    {
        BOOL exists = FALSE;

        for (j = 0; j < pack_count; j++)
        {
            if (strcmp(pack_names[j], g_session_report.entries[i].pack_name) == 0)
            {
                exists = TRUE;
                break;
            }
        }

        if (!exists)
        {
            pack_names[pack_count++] = g_session_report.entries[i].pack_name;
        }

        if (g_session_report.entries[i].is_update)
        {
            total_updated++;
        }
        else if (g_session_report.entries[i].is_extraction_skip)
        {
            total_extraction_skipped++;
        }
        else if (g_session_report.entries[i].is_local_cache_reuse)
        {
            total_local_cache_reuse++;
        }
        else
        {
            total_new++;
        }
    }

    fprintf(report_file, "Retroplay WHD Downloader Session Report\n");
    fprintf(report_file, "============================\n");
    fprintf(report_file, "Date: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n\n",
            (long)(tm_info->tm_year + 1900),
            (long)(tm_info->tm_mon + 1),
            (long)tm_info->tm_mday,
            (long)tm_info->tm_hour,
            (long)tm_info->tm_min,
            (long)tm_info->tm_sec);

    for (i = 0; i < pack_count; i++)
    {
        int pack_new = 0;
        int pack_updated = 0;
        int pack_local_cache_reuse = 0;
        int pack_extraction_skipped = 0;

        for (j = 0; j < g_session_report.count; j++)
        {
            if (strcmp(g_session_report.entries[j].pack_name, pack_names[i]) != 0)
            {
                continue;
            }

            if (g_session_report.entries[j].is_update)
            {
                pack_updated++;
            }
            else if (g_session_report.entries[j].is_extraction_skip)
            {
                pack_extraction_skipped++;
            }
            else if (g_session_report.entries[j].is_local_cache_reuse)
            {
                pack_local_cache_reuse++;
            }
            else
            {
                pack_new++;
            }
        }

        if (pack_new > 0 && pack_updated > 0)
        {
            fprintf(report_file, "%s - %ld new, %ld updated", pack_names[i], (long)pack_new, (long)pack_updated);
        }
        else if (pack_updated > 0)
        {
            fprintf(report_file, "%s - %ld updated", pack_names[i], (long)pack_updated);
        }
        else
        {
            fprintf(report_file, "%s - %ld new", pack_names[i], (long)pack_new);
        }

        if (pack_local_cache_reuse > 0)
        {
            fprintf(report_file, ", %ld local-cache", (long)pack_local_cache_reuse);
        }

        if (pack_extraction_skipped > 0)
        {
            fprintf(report_file, ", %ld extract-skipped", (long)pack_extraction_skipped);
        }

        if (pack_local_cache_reuse > 0 || pack_extraction_skipped > 0)
        {
            fprintf(report_file, "\n\n");
        }
        else
        {
            fprintf(report_file, "\n\n");
        }

        if (pack_new > 0)
        {
            fprintf(report_file, "  New:\n");
            for (j = 0; j < g_session_report.count; j++)
            {
                if (strcmp(g_session_report.entries[j].pack_name, pack_names[i]) == 0 &&
                    !g_session_report.entries[j].is_update &&
                    !g_session_report.entries[j].is_local_cache_reuse &&
                    !g_session_report.entries[j].is_extraction_skip)
                {
                    fprintf(report_file, "    %s\n", g_session_report.entries[j].archive_name);
                }
            }
            fprintf(report_file, "\n");
        }

        if (pack_updated > 0)
        {
            fprintf(report_file, "  Updated:\n");
            for (j = 0; j < g_session_report.count; j++)
            {
                if (strcmp(g_session_report.entries[j].pack_name, pack_names[i]) == 0 &&
                    g_session_report.entries[j].is_update)
                {
                    fprintf(report_file,
                            "    %s  (was %s)\n",
                            g_session_report.entries[j].archive_name,
                            g_session_report.entries[j].old_archive_name);
                }
            }
            fprintf(report_file, "\n");
        }

        if (pack_local_cache_reuse > 0)
        {
            fprintf(report_file, "  Local cache reuse (no download):\n");
            for (j = 0; j < g_session_report.count; j++)
            {
                if (strcmp(g_session_report.entries[j].pack_name, pack_names[i]) == 0 &&
                    g_session_report.entries[j].is_local_cache_reuse)
                {
                    fprintf(report_file, "    %s\n", g_session_report.entries[j].archive_name);
                }
            }
            fprintf(report_file, "\n");
        }

        if (pack_extraction_skipped > 0)
        {
            fprintf(report_file, "  Extraction skipped:\n");
            for (j = 0; j < g_session_report.count; j++)
            {
                if (strcmp(g_session_report.entries[j].pack_name, pack_names[i]) == 0 &&
                    g_session_report.entries[j].is_extraction_skip)
                {
                    fprintf(report_file,
                            "    %s  (%s)\n",
                            g_session_report.entries[j].archive_name,
                            g_session_report.entries[j].skip_reason);
                }
            }
            fprintf(report_file, "\n");
        }
    }

    fprintf(report_file,
            "Total: %ld new, %ld updated, %ld local-cache reuse (no download), %ld extraction skipped.\n",
            (long)total_new,
            (long)total_updated,
            (long)total_local_cache_reuse,
            (long)total_extraction_skipped);

    fclose(report_file);
    amiga_free(pack_names);

    printf("Report saved: %s (%ld new, %ld updated, %ld local-cache, %ld extract-skipped)\n",
           report_path,
           (long)total_new,
           (long)total_updated,
           (long)total_local_cache_reuse,
           (long)total_extraction_skipped);
}

void report_cleanup(void)
{
    if (g_session_report.entries != NULL)
    {
        amiga_free(g_session_report.entries);
        g_session_report.entries = NULL;
    }

    g_session_report.count = 0;
    g_session_report.capacity = 0;
}
