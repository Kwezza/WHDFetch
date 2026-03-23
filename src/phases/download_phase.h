#ifndef DOWNLOAD_PHASE_H
#define DOWNLOAD_PHASE_H

#include "main.h"

LONG count_total_queued_entries_for_selected_packs(struct whdload_pack_def whdload_pack_defs[],
                                                   int num_packs);

ULONG sum_total_queued_kb_for_selected_packs(struct whdload_pack_def whdload_pack_defs[],
                                             int num_packs);

char *get_first_matching_fileName(const char *fileNameToFind);

LONG download_roms_if_file_exists(struct whdload_pack_def *WHDLoadPackDefs,
                                  const struct download_option *download_options,
                                  int replaceFiles,
                                  download_progress_state *progress_state);

LONG extract_existing_archives_if_file_exists(struct whdload_pack_def *WHDLoadPackDefs,
                                              const struct download_option *download_options);

void sum_queued_kb_per_pack(struct whdload_pack_def whdload_pack_defs[],
                            int num_packs,
                            ULONG *out_kb_per_pack);

void print_space_estimate(struct whdload_pack_def whdload_pack_defs[],
                          int num_packs,
                          const ULONG *kb_per_pack);

LONG extract_existing_archives_from_file(const char *filename,
                                         struct whdload_pack_def *WHDLoadPackDefs,
                                         const struct download_option *download_options);

LONG download_roms_from_file(const char *filename,
                             struct whdload_pack_def *WHDLoadPackDefs,
                             const struct download_option *download_options,
                             int replaceFiles,
                             download_progress_state *progress_state);

LONG execute_archive_download_command(const char *downloadWHDFile,
                                      ULONG archive_size_bytes,
                                      const char *archive_crc,
                                      struct whdload_pack_def *WHDLoadPackDefs,
                                      const struct download_option *download_options,
                                      int replaceFiles,
                                      download_progress_state *progress_state);

#endif /* DOWNLOAD_PHASE_H */
