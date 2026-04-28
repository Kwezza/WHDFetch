#ifndef VH_CSV_H
#define VH_CSV_H

typedef struct VhCsvEntry {
    int id;
    char *token;
    char *description;
    int is_default;
} VhCsvEntry;

typedef struct VhCsvFile {
    VhCsvEntry *entries;
    int count;
    int capacity;
} VhCsvFile;

typedef struct VhCsvResult {
    int id;
    const char *canonical;
    const char *description;
} VhCsvResult;

int vh_csv_load(VhCsvFile *csv, const char *path);
void vh_csv_free(VhCsvFile *csv);
int vh_csv_lookup_token(const VhCsvFile *csv, const char *token, VhCsvResult *out);
int vh_csv_get_default(const VhCsvFile *csv, VhCsvResult *out);

#endif
