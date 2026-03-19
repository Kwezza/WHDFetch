#ifndef REPORT_H
#define REPORT_H

void report_init(void);

void report_add(const char *archive_name,
                const char *pack_name,
                const char *old_archive_name);

void report_add_local_cache_reuse(const char *archive_name,
                                  const char *pack_name);

void report_add_extraction_skip(const char *archive_name,
                                const char *pack_name,
                                const char *reason);

void report_add_download_failure(const char *archive_name,
                                 const char *pack_name,
                                 const char *reason);

void report_write(void);
void report_cleanup(void);

#endif /* REPORT_H */