#ifndef EXTRACT_H
#define EXTRACT_H

#include <exec/types.h>
#include <stddef.h>

#include "main.h"

#define EXTRACT_MAX_PATH 512
#define EXTRACT_MAX_NAME 256

#define EXTRACT_RESULT_OK 0
#define EXTRACT_RESULT_INVALID_INPUT 30
#define EXTRACT_RESULT_TARGET_PATH_FAILED 31
#define EXTRACT_RESULT_METADATA_FAILED 32
#define EXTRACT_RESULT_DELETE_FAILED 33
#define EXTRACT_RESULT_SKIPPED_TOOL_MISSING 34
#define EXTRACT_RESULT_SKIPPED_UNSUPPORTED_ARCHIVE 35

BOOL extract_should_delete_archive(const download_option *download_options);

BOOL extract_derive_game_folder_name(const char *archive_filename,
                                     char *out_folder_name,
                                     size_t out_folder_name_size);

BOOL extract_build_target_directory(const char *archive_path,
                                    const char *pack_dir,
                                    const char *first_letter,
                                    const download_option *download_options,
                                    char *out_target_directory,
                                    size_t out_target_directory_size);

LONG extract_archive_with_lha(const char *archive_path, const char *target_directory);

BOOL extract_write_archive_metadata(const char *target_directory,
                                    const char *game_folder_name,
                                    const char *category_name,
                                    const char *archive_filename);

LONG extract_process_downloaded_archive(const char *archive_path,
                                        const char *archive_filename,
                                        const char *pack_dir,
                                        const char *first_letter,
                                        const char *category_name,
                                        const download_option *download_options);

BOOL extract_is_archive_already_extracted(const char *archive_path,
                                          const char *archive_filename,
                                          const char *pack_dir,
                                          const char *first_letter,
                                          const download_option *download_options,
                                          char *out_match_folder_path,
                                          size_t out_match_folder_path_size);

BOOL extract_index_load(const char *target_directory);

BOOL extract_index_lookup(const char *target_directory,
                          const char *archive_filename,
                          char *out_folder_name,
                          size_t out_folder_name_size);

BOOL extract_index_find_by_title(const char *title,
                                 char *out_old_archive,
                                 size_t out_size);

BOOL extract_index_find_update_candidate(const char *new_archive_filename,
                                         char *out_old_archive,
                                         size_t out_size);

BOOL extract_index_update(const char *target_directory,
                          const char *archive_filename,
                          const char *folder_name);

void extract_index_flush(void);

#endif /* EXTRACT_H */
