#include "platform/platform.h"

#include "config/app_defaults.h"

void setup_app_defaults(struct whdload_pack_def WHDLoadPackDefs[],
                        struct download_option *downloadOptions)
{
    if (downloadOptions != NULL)
    {
        downloadOptions->download_games = 0;
        downloadOptions->download_games_beta = 0;
        downloadOptions->download_demos = 0;
        downloadOptions->download_demos_beta = 0;
        downloadOptions->download_magazines = 0;
        downloadOptions->download_all = 0;
        downloadOptions->no_skip_messages = 0;
        downloadOptions->verbose_output = 0;
        downloadOptions->extract_archives = TRUE;
        downloadOptions->skip_existing_extractions = TRUE;
        downloadOptions->force_extract = FALSE;
        downloadOptions->skip_download_if_extracted = TRUE;
        downloadOptions->force_download = FALSE;
        downloadOptions->extract_path = NULL;
        downloadOptions->delete_archives_after_extract = TRUE;
        downloadOptions->purge_archives = FALSE;
        downloadOptions->extract_existing_only = 0;
        downloadOptions->use_custom_icons = TRUE;
        downloadOptions->unsnapshot_icons = TRUE;
        downloadOptions->disable_counters = FALSE;
        downloadOptions->crc_check = FALSE;
        downloadOptions->timeout_seconds = 30;
    }

    WHDLoadPackDefs[DEMOS_BETA].count_existing_files_skipped = 0;
    WHDLoadPackDefs[DEMOS_BETA].count_new_files_downloaded = 0;
    WHDLoadPackDefs[DEMOS_BETA].download_url = WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].extracted_pack_dir = WHDLOAD_DIR_DEMOS_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].full_text_name_of_pack = WHDLOAD_TEXT_NAME_DEMOS_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[DEMOS_BETA].updated_dat_downloaded = 0;
    WHDLoadPackDefs[DEMOS_BETA].user_requested_download = 0;
    WHDLoadPackDefs[DEMOS_BETA].file_count = 0;

    WHDLoadPackDefs[DEMOS].count_existing_files_skipped = 0;
    WHDLoadPackDefs[DEMOS].count_new_files_downloaded = 0;
    WHDLoadPackDefs[DEMOS].download_url = WHDLOAD_DOWNLOAD_DEMOS;
    WHDLoadPackDefs[DEMOS].extracted_pack_dir = WHDLOAD_DIR_DEMOS;
    WHDLoadPackDefs[DEMOS].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_DEMOS;
    WHDLoadPackDefs[DEMOS].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_DEMOS;
    WHDLoadPackDefs[DEMOS].full_text_name_of_pack = WHDLOAD_TEXT_NAME_DEMOS;
    WHDLoadPackDefs[DEMOS].updated_dat_downloaded = 0;
    WHDLoadPackDefs[DEMOS].user_requested_download = 0;
    WHDLoadPackDefs[DEMOS].file_count = 0;

    WHDLoadPackDefs[GAMES_BETA].count_existing_files_skipped = 0;
    WHDLoadPackDefs[GAMES_BETA].count_new_files_downloaded = 0;
    WHDLoadPackDefs[GAMES_BETA].download_url = WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].extracted_pack_dir = WHDLOAD_DIR_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].full_text_name_of_pack = WHDLOAD_TEXT_NAME_GAMES_BETA_AND_UNRELEASED;
    WHDLoadPackDefs[GAMES_BETA].updated_dat_downloaded = 0;
    WHDLoadPackDefs[GAMES_BETA].user_requested_download = 0;
    WHDLoadPackDefs[GAMES_BETA].file_count = 0;

    WHDLoadPackDefs[GAMES].count_existing_files_skipped = 0;
    WHDLoadPackDefs[GAMES].count_new_files_downloaded = 0;
    WHDLoadPackDefs[GAMES].download_url = WHDLOAD_DOWNLOAD_GAMES;
    WHDLoadPackDefs[GAMES].extracted_pack_dir = WHDLOAD_DIR_GAMES;
    WHDLoadPackDefs[GAMES].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_GAMES;
    WHDLoadPackDefs[GAMES].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_GAMES;
    WHDLoadPackDefs[GAMES].full_text_name_of_pack = WHDLOAD_TEXT_NAME_GAMES;
    WHDLoadPackDefs[GAMES].updated_dat_downloaded = 0;
    WHDLoadPackDefs[GAMES].user_requested_download = 0;
    WHDLoadPackDefs[GAMES].file_count = 0;

    WHDLoadPackDefs[MAGAZINES].count_existing_files_skipped = 0;
    WHDLoadPackDefs[MAGAZINES].count_new_files_downloaded = 0;
    WHDLoadPackDefs[MAGAZINES].download_url = WHDLOAD_DOWNLOAD_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].extracted_pack_dir = WHDLOAD_DIR_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].filter_dat_files = WHDLOAD_FILE_FILTER_DAT_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].filter_zip_files = WHDLOAD_FILE_FILTER_ZIP_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].full_text_name_of_pack = WHDLOAD_TEXT_NAME_MAGAZINES;
    WHDLoadPackDefs[MAGAZINES].updated_dat_downloaded = 0;
    WHDLoadPackDefs[MAGAZINES].user_requested_download = 0;
    WHDLoadPackDefs[MAGAZINES].file_count = 0;
}
