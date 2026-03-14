/*
 * gamefile_parser.h
 *
 * This code is designed to take pre-made WHDLoad files created by Retroplay and extract the system requirements for further processing.
 * It is intended to run on the Amiga range of computers.
 *
 * Purpose:
 * The primary purpose of this code is to parse filenames of WHDLoad game files and extract various system requirement details
 * such as the game title, version, machine type, video format, language, media format, memory requirements, number of disks, and special attributes.
 * This extracted information can then be used for filtering, organizing, and further processing of the game files.
 *
 * How it works:
 * - The code reads each filename and splits it into tokens based on underscores ('_').
 * - It processes each token to identify and extract relevant information such as the game title, version, machine type, video format, language, etc.
 * - The extracted information is stored in a struct for easy access and further processing.
 * - Unmatched tokens are stored in a 'special' field to capture any additional attributes not explicitly handled by the parsing logic.
 *
 * The code consists of the following main components:
 * - `formatTitle`: Formats the raw game title by inserting spaces appropriately.
 * - `convertLanguage`: Converts language abbreviations to their full names.
 * - `appendSpecial`: Appends special attributes to the 'special' field.
 * - `appendLanguage`: Appends languages to the 'language' field.
 * - `checkMultipleLanguages`: Checks and processes multiple language tokens.
 * - `isFourDigitNumber`: Checks if a token is a four-digit number.
 * - `parseFilename`: Main function that parses the filename and extracts the system requirements.
 * - `filterGame`: Filters games based on specified criteria.
 *
 */

#include "gamefile_parser.h"
#include "utilities.h"

void formatTitle(const char *rawTitle, char *formattedTitle)
{
    int len, j, i;
    len = strlen(rawTitle); // Get the length of the raw title string
    j = 0;                  // Initialize the index for the formatted title

    for (i = 0; i < len; i++)
    {
        // Check if a space should be inserted before the current character
        if (i > 0)
        {
            // Special case for "3D" to avoid inserting a space between '3' and 'D'
            if (rawTitle[i - 1] == '3' && rawTitle[i] == 'D')
            {
                formattedTitle[j++] = rawTitle[i];
                continue;
            }
            // Insert a space if a digit is followed by a letter (except "3D")
            if (isdigit(rawTitle[i]) && isalpha(rawTitle[i - 1]) && !(rawTitle[i - 1] == '3' && rawTitle[i] == 'D'))
            {
                formattedTitle[j++] = ' ';
            }
            // Insert a space if a letter is followed by a digit (except "3D")
            else if (isalpha(rawTitle[i]) && isdigit(rawTitle[i - 1]) && !(rawTitle[i - 1] == '3' && rawTitle[i] == 'D'))
            {
                formattedTitle[j++] = ' ';
            }
            // Insert a space if an uppercase letter is preceded by a lowercase letter or a digit
            else if (isupper(rawTitle[i]) && (islower(rawTitle[i - 1]) || isdigit(rawTitle[i - 1])))
            {
                formattedTitle[j++] = ' ';
            }
            // Insert a space if an uppercase letter is followed by a lowercase letter
            else if (isupper(rawTitle[i]) && isupper(rawTitle[i - 1]) && islower(rawTitle[i + 1]))
            {
                formattedTitle[j++] = ' ';
            }
        }
        // Handle the '&' character by adding spaces around it if needed
        if (rawTitle[i] == '&')
        {
            // Add a space before '&' if not already present
            if (j > 0 && formattedTitle[j - 1] != ' ')
            {
                formattedTitle[j++] = ' ';
            }
            formattedTitle[j++] = rawTitle[i];
            // Add a space after '&' if not already present
            if (i + 1 < len && rawTitle[i + 1] != ' ')
            {
                formattedTitle[j++] = ' ';
            }
        }
        else
        {
            formattedTitle[j++] = rawTitle[i];
        }
    }
    formattedTitle[j] = '\0'; // Null-terminate the formatted title string
}

/**
 * Converts a language abbreviation to its full name.
 * 
 * @param abbreviation The language abbreviation.
 * @param fullName The full name of the language.
 */
void get_full_language_name(const char* abbreviation, char* fullName) {
    if (strncasecmp_custom(abbreviation, "Cz", 2) == 0) {
        strcpy(fullName, "Czech");
    } else if (strncasecmp_custom(abbreviation, "Dk", 2) == 0) {
        strcpy(fullName, "Danish");
    } else if (strncasecmp_custom(abbreviation, "NL", 2) == 0) {
        strcpy(fullName, "Dutch");
    } else if (strncasecmp_custom(abbreviation, "Fi", 2) == 0) {
        strcpy(fullName, "Finnish");
    } else if (strncasecmp_custom(abbreviation, "Fr", 2) == 0) {
        strcpy(fullName, "French");
    } else if (strncasecmp_custom(abbreviation, "De", 2) == 0) {
        strcpy(fullName, "German");
    } else if (strncasecmp_custom(abbreviation, "Gr", 2) == 0) {
        strcpy(fullName, "Greek");
    } else if (strncasecmp_custom(abbreviation, "It", 2) == 0) {
        strcpy(fullName, "Italian");
    } else if (strncasecmp_custom(abbreviation, "Es", 2) == 0) {
        strcpy(fullName, "Spanish");
    } else if (strncasecmp_custom(abbreviation, "Pl", 2) == 0) {
        strcpy(fullName, "Polish");
    } else {
        strcpy(fullName, "");  /* Set to empty if not found */
    }
}


/**
 * Appends a special attribute to the special field.
 * 
 * @param special The special field.
 * @param newSpecial The new special attribute to append.
 */
void append_to_special(char* special, const char* newSpecial) {
    if (strncasecmp_custom(special, "None", 4) == 0) {
        strcpy(special, newSpecial);
    } else {
        strcat(special, ", ");
        strcat(special, newSpecial);
    }
}

/**
 * Appends a language to the language field.
 * 
 * @param language The language field.
 * @param newLanguage The new language to append.
 */
void addLanguageToString(char* language, const char* newLanguage) {
    if (strncasecmp_custom(language, "English", 7) == 0) {
        strcpy(language, newLanguage);
    } else {
        strcat(language, ", ");
        strcat(language, newLanguage);
    }
}

/**
 * Checks for multiple languages and appends each one.
 * 
 * @param token The token containing multiple languages.
 * @param language The language field.
 */
void extract_languages_from_token(const char* token, char* language) {
    int len, i;
    char abbreviation[3];
    char temp[MAX_LANGUAGE_LEN];

    len = strlen(token);
    for (i = 0; i < len; i += 2) {
        abbreviation[0] = token[i];
        abbreviation[1] = token[i + 1];
        abbreviation[2] = '\0';
        get_full_language_name(abbreviation, temp);
        if (strlen(temp) > 0) {  /* Only append if a valid language was found */
            addLanguageToString(language, temp);
        }
    }
}

/**
 * Checks if a token is a four-digit number.
 * 
 * @param token The token to check.
 * @return 1 if the token is a four-digit number, 0 otherwise.
 */
int check_if_four_digits(const char* token) {
    int len, i;

    len = strlen(token);
    if (len != 4) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)token[i])) {
            return 0;
        }
    }
    return 1;
}


/* 
 * Parses a filename to extract game information.
 * 
 * @param filename The filename to parse.
 * @param game The game_metadata struct to store the extracted information.
 */
void extract_game_info_from_filename(const char* filename, game_metadata* game) {
    char temp[MAX_FILENAME_LEN];
    char* token;
    char* rest;
    char rawTitle[MAX_TITLE_LEN] = "";
    char* lastToken;
    int isValidLanguage;
    char fullLanguage[MAX_LANGUAGE_LEN];
    int i;
    int len;
    char abbreviation[3];

    strcpy(temp, filename);
    rest = temp;
    lastToken = NULL;

    /* Remove the extension */
    token = strrchr(rest, '.');
    if (token != NULL) {
        *token = '\0';
    }

    /* Extract the title */
    token = strtok(rest, "_");
    if (token != NULL) {
        strcpy(rawTitle, token);
        formatTitle(rawTitle, game->title);
    }

    /* Extract the version */
    token = strtok(NULL, "_");
    if (token != NULL) {
        strcpy(game->version, token);
    }

    /* Initialize defaults */
    strcpy(game->machineType, "OCS");
    strcpy(game->videoFormat, "PAL");
    strcpy(game->mediaFormat, "Floppy");
    strcpy(game->language, "English");
    strcpy(game->memory, "Unknown");
    game->numberOfDisks = 1;  /* Default to 1 if not specified */
    strcpy(game->sps, "None");
    strcpy(game->special, "None");

    /* Process each token and classify them */
    while ((token = strtok(NULL, "_")) != NULL) {
        isValidLanguage = 1;  /* Assume the token is a valid language sequence */
        len = strlen(token);

        /* Check if the token is a known language */
        if (len % 2 == 0) {
            for (i = 0; i < len; i += 2) {
                abbreviation[0] = token[i];
                abbreviation[1] = token[i + 1];
                abbreviation[2] = '\0';
                get_full_language_name(abbreviation, fullLanguage);
                if (strlen(fullLanguage) == 0) {
                    isValidLanguage = 0;  /* Not a valid language */
                    break;
                }
            }
        } else {
            isValidLanguage = 0;  /* Length is not even, cannot be a valid sequence of languages */
        }

        /* If the token is a valid language sequence, append each language */
        if (isValidLanguage) {
            for (i = 0; i < len; i += 2) {
                abbreviation[0] = token[i];
                abbreviation[1] = token[i + 1];
                abbreviation[2] = '\0';
                get_full_language_name(abbreviation, fullLanguage);
                addLanguageToString(game->language, fullLanguage);
            }
        } else {
            /* If the token is not a valid language sequence, classify it */
            if (strncasecmp_custom(token, "AGA", 3) == 0) {
                strcpy(game->machineType, "AGA");
            } else if (strncasecmp_custom(token, "NTSC", 4) == 0) {
                strcpy(game->videoFormat, "NTSC");
            } else if (strncasecmp_custom(token, "CD32", 4) == 0) {
                strcpy(game->mediaFormat, "CD32");
            } else if (strncasecmp_custom(token, "CD", 2) == 0 || strncasecmp_custom(token, "CDRom", 5) == 0) {
                strcpy(game->mediaFormat, "CDRom");
            } else if (strncasecmp_custom(token, "CDTV", 4) == 0) {
                strcpy(game->mediaFormat, "CDTV");
            } else if (strncasecmp_custom(token, "Arcadia", 7) == 0) {
                strcpy(game->mediaFormat, "Arcadia");
            } else if (strncasecmp_custom(token, "Chip", 4) == 0 || strncasecmp_custom(token, "Fast", 4) == 0 || strncasecmp_custom(token, "512k", 4) == 0 || 
                       strncasecmp_custom(token, "512KB", 5) == 0 || strncasecmp_custom(token, "1MB", 3) == 0 || strncasecmp_custom(token, "1MBChip", 7) == 0 || 
                       strncasecmp_custom(token, "1Mb", 3) == 0 || strncasecmp_custom(token, "1MbChip", 7) == 0 || strncasecmp_custom(token, "1.5MB", 5) == 0 || 
                       strncasecmp_custom(token, "2MB", 3) == 0 || strncasecmp_custom(token, "8MB", 3) == 0 || strncasecmp_custom(token, "12MB", 4) == 0 || 
                       strncasecmp_custom(token, "15MB", 4) == 0 || strncasecmp_custom(token, "LowMem", 6) == 0 || strncasecmp_custom(token, "SlowMem", 7) == 0) {
                strcpy(game->memory, token);
            } else if (strncasecmp_custom(token, "1Disk", 5) == 0) {
                game->numberOfDisks = 1;
            } else if (strncasecmp_custom(token, "2Disk", 5) == 0) {
                game->numberOfDisks = 2;
            } else if (strncasecmp_custom(token, "3Disk", 5) == 0) {
                game->numberOfDisks = 3;
            } else if (strncasecmp_custom(token, "4Disk", 5) == 0) {
                game->numberOfDisks = 4;
            } else if (strncasecmp_custom(token, "5Disk", 5) == 0) {
                game->numberOfDisks = 5;
            } else if (check_if_four_digits(token)) {
                /* Set the SPS number */
                strcpy(game->sps, token);
            } else if (strstr(token, "&") != NULL) {
                /* Set the SPS number if token has & */
                char* spsToken = strtok(token, "&");
                strcpy(game->sps, spsToken);
                break;
            } else if (strncasecmp_custom(token, "Music", 5) == 0 || strncasecmp_custom(token, "Image", 5) == 0 || strncasecmp_custom(token, "Files", 5) == 0 || strncasecmp_custom(token, "NoMusic", 7) == 0 || strncasecmp_custom(token, "NoVoice", 7) == 0) {
                append_to_special(game->special, token);
            } else {
                append_to_special(game->special, token);
            }
        }
        lastToken = token;
    }

    /* If last token was a four-digit number or a set of four-digit numbers joined by &, discard it */
    if (lastToken && (check_if_four_digits(lastToken) || (strstr(lastToken, "&") != NULL && check_if_four_digits(strtok(lastToken, "&")) && check_if_four_digits(strtok(NULL, "&"))))) {
        *strrchr(rest, '_') = '\0';
    }
}



/**
 * Filters the game based on specified criteria.
 * 
 * @param game The game_metadata struct.
 * @param machineTypeFilter The machine type filter.
 * @param videoFormatFilter The video format filter.
 * @param versionFilter The version filter.
 * @param mediaFormatFilter The media format filter.
 * @return 1 if the game matches the criteria, 0 otherwise.
 */
int filterGame(game_metadata* game, const char* machineTypeFilter, const char* videoFormatFilter, const char* versionFilter, const char* mediaFormatFilter) {
    if (machineTypeFilter != NULL && strncasecmp_custom(game->machineType, machineTypeFilter, strlen(machineTypeFilter)) != 0) {
        return 0;
    }
    if (videoFormatFilter != NULL && strncasecmp_custom(game->videoFormat, videoFormatFilter, strlen(videoFormatFilter)) != 0) {
        return 0;
    }
    if (versionFilter != NULL && strncasecmp_custom(game->version, versionFilter, strlen(versionFilter)) != 0) {
        return 0;
    }
    if (mediaFormatFilter != NULL && strncasecmp_custom(game->mediaFormat, mediaFormatFilter, strlen(mediaFormatFilter)) != 0) {
        return 0;
    }
    return 1;
}

