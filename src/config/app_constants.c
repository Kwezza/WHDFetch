#include "platform/platform.h"

#include "main.h"
#include "build_version.h"

const char *DOWNLOAD_WEBSITE = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/";
const char *FILE_PART_TO_REMOVE = "Commodore%20Amiga%20-%20WHDLoad%20-%20";
const char *HTML_LINK_PREFIX_FILTER = "Commodore%20Amiga";
const char *HTML_LINK_CONTENT_FILTER = "WHDLoad";
const char *LINK_CLEANUP_REMOVALS[MAX_LINK_CLEANUP_REMOVALS] = {
    "amp;",
    "%20",
    "CommodoreAmiga-",
    "&amp;",
    "&Unofficial",
    "&Unreleased"
};
int LINK_CLEANUP_REMOVAL_COUNT = 6;

const char *WHDLOAD_DOWNLOAD_DEMOS_BETA_AND_UNRELEASED = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Demos_-_Beta_&_Unofficial/";
const char *WHDLOAD_DOWNLOAD_DEMOS = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Demos/";
const char *WHDLOAD_DOWNLOAD_GAMES_BETA_AND_UNRELEASED = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games_-_Beta_&_Unofficial/";
const char *WHDLOAD_DOWNLOAD_GAMES = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Games/";
const char *WHDLOAD_DOWNLOAD_MAGAZINES = "http://ftp2.grandis.nu/turran/FTP/Retroplay%20WHDLoad%20Packs/Commodore_Amiga_-_WHDLoad_-_Magazines/";

const char *WHDLOAD_TEXT_NAME_DEMOS_BETA_AND_UNRELEASED = "Demos (Beta & Unofficial)";
const char *WHDLOAD_TEXT_NAME_DEMOS = "Demos";
const char *WHDLOAD_TEXT_NAME_GAMES_BETA_AND_UNRELEASED = "Games (Beta & Unofficial)";
const char *WHDLOAD_TEXT_NAME_GAMES = "Games";
const char *WHDLOAD_TEXT_NAME_MAGAZINES = "Magazines";

const char *WHDLOAD_DIR_DEMOS_BETA_AND_UNRELEASED = "Demos Beta/";
const char *WHDLOAD_DIR_DEMOS = "Demos/";
const char *WHDLOAD_DIR_GAMES_BETA_AND_UNRELEASED = "Games Beta/";
const char *WHDLOAD_DIR_GAMES = "Games/";
const char *WHDLOAD_DIR_MAGAZINES = "Magazines/";

const char *WHDLOAD_FILE_FILTER_DAT_BETA_AND_UNRELEASED = "Demos-Beta(";
const char *WHDLOAD_FILE_FILTER_DAT_DEMOS = "Demos(";
const char *WHDLOAD_FILE_FILTER_DAT_GAMES_BETA_AND_UNRELEASED = "Games-Beta";
const char *WHDLOAD_FILE_FILTER_DAT_GAMES = "Games(";
const char *WHDLOAD_FILE_FILTER_DAT_MAGAZINES = "Magazines(";

const char *WHDLOAD_FILE_FILTER_ZIP_BETA_AND_UNRELEASED = "Demos-Beta";
const char *WHDLOAD_FILE_FILTER_ZIP_DEMOS = "Demos(";
const char *WHDLOAD_FILE_FILTER_ZIP_GAMES_BETA_AND_UNRELEASED = "Games-Beta";
const char *WHDLOAD_FILE_FILTER_ZIP_GAMES = "Games(";
const char *WHDLOAD_FILE_FILTER_ZIP_MAGAZINES = "Magazines";

const char *GITHUB_ADDRESS = "https://github.com/Kwezza/Retroplay-WHDLoad-downloader";

const int DEMOS_BETA = 0;
const int DEMOS = 1;
const int GAMES_BETA = 2;
const int GAMES = 3;
const int MAGAZINES = 4;

#define PROGRAM_NAME_LITERAL "Retroplay WHD Downloader"
#define VERSION_STRING_LITERAL "0.9b"

const char PROGRAM_NAME[] = PROGRAM_NAME_LITERAL;
const char VERSION_STRING[] = VERSION_STRING_LITERAL;
const char version[] = "$VER: " PROGRAM_NAME_LITERAL " " VERSION_STRING_LITERAL " (" APP_BUILD_DATE_DMY ")";

/* const char *DIR_GAME_DOWNLOADS = "GameFiles/"; */
const char *DIR_DAT_FILES = "temp/Dat files";
const char *DIR_GAME_DOWNLOADS = "GameFiles";
const char *DIR_HOLDING = "temp/holding";
const char *DIR_TEMP = "temp";
const char *DIR_ZIP_FILES = "temp/Zip files";
