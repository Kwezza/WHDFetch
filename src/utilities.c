#include <stdio.h>
#include <graphics/text.h>
#include <exec/types.h>
#include <graphics/rastport.h>
#include <stddef.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dos.h>
#include <math.h>
#include <exec/memory.h>
#include <string.h>

#include <stdlib.h>
#include <ctype.h>

#include "utilities.h"
#include "platform/platform.h"
#include "log/log.h"

extern const char *DIR_GAME_DOWNLOADS;

#define UTIL_FILE_SCAN_BUFFER_SIZE 1024


#define OUTPUT_BUFFER_SIZE 256 /*used in the RunCommandAndGetFirstLineAlloc function*/
#define TEMP_FILE_NAME "T:cmd_output.txt" /*used in the RunCommandAndGetFirstLineAlloc function*/

/* Function to get Workbench version (requires Workbench 3.0+) */
int get_workbench_version(void) {
    struct Library *DOSBase;
    int WBversion;
    int libVersion;
    int libRevision;
    /* Open the DOS library for Workbench 2.0 and higher */
    DOSBase = OpenLibrary("dos.library", 0);
    if (!DOSBase) {
        return -1000; /* Failed to open dos.library */
    }

    /* Ensure SysBase is defined and accessible */
    if (!SysBase) {
        CloseLibrary(DOSBase);
        return -1000;
    }

    /* Get the system version from SysBase */
    libVersion = SysBase->LibNode.lib_Version;
    libRevision = SysBase->LibNode.lib_Revision;

    /* Combine version and revision into a single integer */
    /* Ensuring revision is always treated as a three-digit number */
    WBversion = libVersion * 1000 + libRevision * 10;

    /* Close the DOS library */
    CloseLibrary(DOSBase);


    return WBversion;
}

void LookupWorkbenchVersion(int revision, char *versionString) {
    /* Ensure versionString is valid */
    if (!versionString) {
        return;
    }

    /* Clear the versionString */
    versionString[0] = '\0';

    /* Map the revision to the Workbench version */
    switch (revision) {
        case 30000:
            strcpy(versionString, "1.0");
            break;
        case 31334:
            strcpy(versionString, "1.1");
            break;
        case 33460:
        case 33470:
        case 33560:
        case 33590:
        case 33610:
            strcpy(versionString, "1.2");
            break;
        case 34200:
        case 34280:
        case 34340:
        case 34100: /* A2024 specific */
            strcpy(versionString, "1.3");
            break;
        case 36680:
            strcpy(versionString, "2.0");
            break;
        case 37670:
            strcpy(versionString, "2.04");
            break;
        case 37710:
        case 37720:
            strcpy(versionString, "2.05");
            break;
        case 38360:
            strcpy(versionString, "2.1");
            break;
        case 39290:
            strcpy(versionString, "3.0");
            break;
        case 40420:
            strcpy(versionString, "3.1");
            break;
        case 44200:
        case 44400:
        case 44500:
            strcpy(versionString, "3.5");
            break;
        case 45100:
        case 45200:
        case 45300:
            strcpy(versionString, "3.9");
            break;
        case 45194:
            strcpy(versionString, "3.1.4");
            break;
        case 47100:
            strcpy(versionString, "3.2");
            break;
        case 47200:
            strcpy(versionString, "3.2.1");
            break;
        case 47300:
            strcpy(versionString, "3.2.2");
            break;
        case 47400:
            strcpy(versionString, "3.2.2.1");
            break;
        case 50000:
            strcpy(versionString, "4.0");
            break;
        case 51000:
            strcpy(versionString, "4.1");
            break;
        case 51100:
            strcpy(versionString, "4.1 Update 1");
            break;
        case 51200:
            strcpy(versionString, "4.1 Update 2");
            break;
        case 51300:
            strcpy(versionString, "4.1 Update 3");
            break;
        case 51400:
            strcpy(versionString, "4.1 Update 4");
            break;
        case 51500:
            strcpy(versionString, "4.1 Update 5");
            break;
        case 51600:
            strcpy(versionString, "4.1 Final Edition");
            break;
        default:
            sprintf(versionString, "Unknown Workbench version %d", revision);
            break;
    }
}

char* convertWBVersionWithDot(int number) {
    char buffer[16]; /* Buffer to hold the number as a string */
    char *result; /* Pointer to the result string */

    /* Convert the integer to a string */
    sprintf(buffer, "%d", number);

    /* Allocate memory for the result string */
    result = (char*)amiga_malloc(strlen(buffer) + 2); /* +2 for the dot and null terminator */

    /* Check if memory allocation was successful */
    if (result == NULL) {
        return NULL;
    }

    /* Insert the dot after the first two digits */
    strncpy(result, buffer, 2);
    result[2] = '.'; 
    strcpy(result + 3, buffer + 2);

    return result;
}

int Compare(const void *a, const void *b)
{
    return stricmp_custom(*(const char **)a, *(const char **)b);
}

int stricmp_custom(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        int d = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (d != 0) return d;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp_custom(const char *s1, const char *s2, size_t n)
{
    while (n-- > 0 && *s1 != '\0' && *s2 != '\0')
    {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);

        if (c1 != c2)
        {
            return c1 - c2;
        }

        s1++;
        s2++;
    }

    return 0;
}

archive_type detect_archive_type_from_filename(const char *filename)
{
    const char *dot;

    if (filename == NULL)
    {
        return ARCHIVE_TYPE_UNKNOWN;
    }

    dot = strrchr(filename, '.');
    if (dot == NULL)
    {
        return ARCHIVE_TYPE_UNKNOWN;
    }

    if (stricmp_custom(dot, ".lha") == 0)
    {
        return ARCHIVE_TYPE_LHA;
    }

    if (stricmp_custom(dot, ".lzx") == 0)
    {
        return ARCHIVE_TYPE_LZX;
    }

    return ARCHIVE_TYPE_UNKNOWN;
}

BOOL does_file_or_folder_exist(const char *filename, int appendWorkingDirectory)
{
    BPTR lock;
    BOOL exists = FALSE;
    /* LONG errorCode = 0; */
    char currentDir[256];
    char newFilePath[512] = {0}; /* Ensure buffer is initially empty */
    
    /* Retrieve current directory */
    if (GetCurrentDirName(currentDir, sizeof(currentDir)) == DOSFALSE)
    {
        printf("Error getting current directory\n");
    }

    /* Construct new file path if required */
    if (appendWorkingDirectory == 1)
    {
        if (!AddPart(currentDir, filename, sizeof(currentDir)))
        {
            printf("Failed to append filename to current directory\n");
            return FALSE;
        }
        strncpy(newFilePath, currentDir, sizeof(newFilePath) - 1);
        lock = Lock(newFilePath, ACCESS_READ);
        
        #ifdef DEBUGLocks
        append_to_log("Locking directory (does_file_or_folder_exist): %s\n", newFilePath);
        #endif

        if (!lock)
        {
            /* do nothing - no lock means the file doesnt exist or there is maybe some kind of issue.  Assume it cant be accessed either way. */
        }
    }
    else
    {
        strncpy(newFilePath, filename, sizeof(newFilePath) - 1);
        lock = Lock(newFilePath, ACCESS_READ);
        
        #ifdef DEBUGLocks
        append_to_log("Locking directory (does_file_or_folder_exist): %s\n", newFilePath);
        #endif

        if (!lock)
        {
         /* do nothing - no lock means the file doesnt exist or there is maybe some kind of issue.  Assume it cant be accessed either way.' */
           
        }
    }

    /* Check if file lock was successful */
    if (lock)
    {
        exists = TRUE;
        #ifdef DEBUGLocks
        append_to_log("Unlocking directory: %s\n", newFilePath);
        #endif
        UnLock(lock);
    }

    return exists;
}

void trim(char *str)
{
    char *start, *end;

    /* Trim leading space */
    for (start = str; *start != '\0' && isspace((unsigned char)*start); start++)
    {
        /* Intentionally left blank */
    }

    /* All spaces? */
    if (*start == '\0')
    {
        *str = '\0';
        return;
    }

    /* Trim trailing space */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
    {
        end--;
    }

    /* Write new null terminator */
    *(end + 1) = '\0';

    /* Move trimmed string */
    memmove(str, start, end - start + 2);
}

ULONG clamp_timeout_seconds(ULONG requested)
{
    if (requested < TIMEOUT_SECONDS_MIN)
    {
        return TIMEOUT_SECONDS_MIN;
    }

    if (requested > TIMEOUT_SECONDS_MAX)
    {
        return TIMEOUT_SECONDS_MAX;
    }

    return requested;
}

BOOL parse_timeout_seconds(const char *value, ULONG *out_seconds, ULONG *raw_value)
{
    char *endptr;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0' || out_seconds == NULL)
    {
        return FALSE;
    }

    parsed = strtoul(value, &endptr, 10);
    if (endptr == value || *endptr != '\0')
    {
        return FALSE;
    }

    if (parsed == 0)
    {
        return FALSE;
    }

    if (raw_value != NULL)
    {
        *raw_value = (ULONG)parsed;
    }

    *out_seconds = clamp_timeout_seconds((ULONG)parsed);
    return TRUE;
}

int extract_date_from_filename(const char *filename, char *buffer, int bufSize)
{
    char *start = NULL;
    char *end = NULL;

    start = strchr(filename, '(');
    if (start != NULL)
    {
        end = strchr(start, ')');
        if (end != NULL && (end - start - 1 == 10) && bufSize >= 11)
        {
            strncpy(buffer, start + 1, end - start - 1);
            buffer[end - start - 1] = '\0';
            return 1;
        }
    }

    return 0;
}

long convert_string_date_to_int(const char *date)
{
    int year;
    int month;
    int day;

    if (date == NULL)
    {
        return 0;
    }

    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3)
    {
        return 0;
    }

    return (long)(year * 10000 + month * 100 + day);
}

int compare_dates_greater_then_date2(const char *date1, const char *date2)
{
    long date1Int = convert_string_date_to_int(date1);
    long date2Int = convert_string_date_to_int(date2);

    return (date2Int > date1Int) ? 1 : 0;
}

void create_day_with_suffix(int day, char *buffer)
{
    switch (day)
    {
    case 1:
    case 21:
    case 31:
        sprintf(buffer, "%dst", day);
        break;
    case 2:
    case 22:
        sprintf(buffer, "%dnd", day);
        break;
    case 3:
    case 23:
        sprintf(buffer, "%drd", day);
        break;
    default:
        sprintf(buffer, "%dth", day);
        break;
    }
}

const char *get_month_name(int month)
{
    const char *months[] = {
        "January", "February", "March",
        "April", "May", "June",
        "July", "August", "September",
        "October", "November", "December"};

    if (month < 1 || month > 12)
    {
        return "Unknown";
    }

    return months[month - 1];
}

void convert_date_to_long_style(const char *date, char *result)
{
    int day;
    int month;
    int year;
    char daySuffix[5];
    const char *monthName;

    if (date == NULL || result == NULL)
    {
        return;
    }

    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3)
    {
        strcpy(result, "Unknown date");
        return;
    }

    create_day_with_suffix(day, daySuffix);
    monthName = get_month_name(month);
    sprintf(result, "%s %s %d", daySuffix, monthName, year);
}

int Get_latest_filename_from_directory(const char *directory, const char *searchText, char *latestFileName)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    char dateStr[11];
    long newestDate = 0;
    char newestFile[256] = {0};
    char fullPath[512];
    int found = 0;

    if (!(lock = Lock(directory, ACCESS_READ)))
    {
        printf("Failed to lock directory: %s\n", directory);
        return 0;
    }

    if (!(fib = AllocDosObject(DOS_FIB, NULL)))
    {
        UnLock(lock);
        printf("Failed to allocate FileInfoBlock\n");
        return 0;
    }

    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            if (fib->fib_DirEntryType < 0 && strstr(fib->fib_FileName, searchText))
            {
                extract_date_from_filename(fib->fib_FileName, dateStr, sizeof(dateStr));
                if (strlen(dateStr) > 1)
                {
                    long fileDate = convert_string_date_to_int(dateStr);
                    if (fileDate > newestDate)
                    {
                        newestDate = fileDate;
                        strcpy(newestFile, fib->fib_FileName);
                        found = 1;
                    }
                }
            }
        }
    }
    else
    {
        printf("Failed to examine directory: %s\n", directory);
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return 0;
    }

    if (found)
    {
        strcpy(latestFileName, newestFile);
        sprintf(fullPath, "%s/%s", directory, newestFile);
        if (Examine(lock, fib))
        {
            while (ExNext(lock, fib))
            {
                if (fib->fib_DirEntryType < 0 && strstr(fib->fib_FileName, searchText) && strcmp(fib->fib_FileName, newestFile) != 0)
                {
                    if (extract_date_from_filename(fib->fib_FileName, dateStr, sizeof(dateStr)))
                    {
                        long fileDate = convert_string_date_to_int(dateStr);
                        if (fileDate < newestDate)
                        {
                            sprintf(fullPath, "%s/%s", directory, fib->fib_FileName);
                            DeleteFile(fullPath);
                        }
                    }
                }
            }
        }
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return found;
}

void wait_char(void)
{   
    /* getchar(); */

    /* The BPTR to the input file handle */
    BPTR inputHandle;
    struct FileHandle *fh;
    UBYTE ch;

    /* Get the input handle for the current process */
    inputHandle = Input();
    fh = (struct FileHandle *)(BADDR(inputHandle));

    /* Set the console to raw mode */
    SetMode(inputHandle, 1);

    /* Wait for a single character */
    Read(inputHandle, &ch, 1);

    /* Restore the console to normal mode */
    SetMode(inputHandle, 0);
}

void remove_CR_LF_from_string(char *str)
{
    char *src = str;
    char *dst = str;

    while (*src)
    {
        if (*src != '\r' && *src != '\n')
        {
            *dst++ = *src;
        }
        src++;
    }

    *dst = '\0'; /* Null-terminate the modified string */
}

int endsWithInfo(const char *filePath) {
    const char *extension = ".info";
    size_t pathLength = strlen(filePath);
    size_t extLength = strlen(extension);
    const char *pathPtr;
    const char *extPtr;
    char c1, c2;

    /* Check if the filePath is shorter than the extension */
    if (pathLength < extLength) {
        return FALSE;
    }

    /* Set pointers to the end of filePath and extension */
    pathPtr = filePath + pathLength - extLength;
    extPtr = extension;

    /* Compare each character in the extension with the corresponding character in the filePath */
    while (*extPtr) {
        c1 = *pathPtr++;
        c2 = *extPtr++;

        /* Convert c1 to lowercase if it's an uppercase letter */
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = c1 + ('a' - 'A');
        }

        /* Convert c2 to lowercase if it's an uppercase letter */
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = c2 + ('a' - 'A');
        }

        /* Compare the lowercase characters */
        if (c1 != c2) {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * Removes a specified prefix from the start of a string.
 * If the prefix is found at the beginning, the function returns a new string
 * with the prefix removed. Otherwise, it returns a copy of the original string.
 *
 * The returned string is allocated using amiga_malloc(), so the caller must free it
 * with amiga_free().
 *
 * @param str The original string.
 * @param prefix The prefix to remove.
 * @return A new string with the prefix removed, or a copy of the original if no match.
 */
char *removeTextFromStartOfString(const char *str, const char *prefix) {
    size_t strLen;
    size_t prefixLen;
    char *copy;
    char *newStr;

    /* Validate input */
    if (!str || !prefix) {
        return NULL;
    }

    strLen = strlen(str);
    prefixLen = strlen(prefix);

    /* Check if the prefix matches */
    if (prefixLen > strLen || strncmp(str, prefix, prefixLen) != 0) {
        /* Prefix not found, return a copy of the original string */
        copy = (char *)amiga_malloc(strLen + 1);
        if (copy) {
            strcpy(copy, str);
        }
        return copy;
    }

    /* Create new string with prefix removed */
    newStr = (char *)amiga_malloc(strLen - prefixLen + 1);
    if (newStr) {
        strcpy(newStr, str + prefixLen);
    }

    return newStr;
}

/* Returns: Pointer to amiga_malloc()'d buffer, or NULL on failure.
   Caller must amiga_free() the result when done. */
char *run_dos_command_and_get_first_line(const char *cmd)
{
    char full_cmd[OUTPUT_BUFFER_SIZE + 64];
    BPTR file;
    char *result;
    LONG len, i;

    if (cmd == NULL || strlen(cmd) > OUTPUT_BUFFER_SIZE - 16)
        return NULL;

    /* Allocate result buffer using AllocVec */
    result = (char *)amiga_malloc(OUTPUT_BUFFER_SIZE);
    if (result == NULL)
        return NULL;

    /* Compose CLI command with redirection */
    strcpy(full_cmd, cmd);
    strcat(full_cmd, " >" TEMP_FILE_NAME);

    if (System(full_cmd, NULL) == -1)
    {
        amiga_free(result);
        return NULL;
    }

    file = Open(TEMP_FILE_NAME, MODE_OLDFILE);
    if (!file)
    {
        amiga_free(result);
        return NULL;
    }

    len = Read(file, result, OUTPUT_BUFFER_SIZE - 1);
    Close(file);
    DeleteFile(TEMP_FILE_NAME);

    if (len <= 0)
    {
        amiga_free(result);
        return NULL;
    }

    result[len] = '\0';
    for (i = 0; i < len; i++)
    {
        if (result[i] == '\n' || result[i] == '\r')
        {
            result[i] = '\0';
            break;
        }
    }

    return result;
}

void Format_text_split_by_Caps(const char *original, char *buffer, size_t buffer_len)
{
    const char *ptr = original;
    size_t buf_index = 0;

    if (original == NULL || buffer == NULL || buffer_len == 0)
    {
        return;
    }

    memset(buffer, 0, buffer_len);

    while (*ptr != '\0' && buf_index < buffer_len - 1)
    {
        if (*ptr == '.' || *ptr == '_')
        {
            break;
        }

        if ((isupper((unsigned char)*ptr) || *ptr == '&') && ptr != original)
        {
            if (buf_index < buffer_len - 2)
            {
                buffer[buf_index++] = ' ';
            }
        }

        buffer[buf_index++] = *ptr++;

        if (buf_index == buffer_len - 1)
        {
            break;
        }
    }

    buffer[buf_index] = '\0';
}

void turn_filename_into_text_with_spaces(const char *filename, char *outputArray)
{
    int i = 0;
    int j = 0;
    int startAppending = 0;

    while (filename[i] != '\0')
    {
        if (filename[i] == '_')
        {
            if (filename[i + 1] == '.' && (filename[i + 2] == 'l' || filename[i + 2] == 'L'))
            {
                break;
            }

            if (startAppending > 0)
            {
                outputArray[j] = ' ';
                j++;
            }
            startAppending = 1;
        }
        else if (startAppending)
        {
            if (filename[i] == '.' && (filename[i + 1] == 'l' || filename[i + 1] == 'L'))
            {
                break;
            }
            outputArray[j++] = filename[i];
        }
        i++;
    }
    outputArray[j] = '\0';
}

void get_folder_name_from_character(char *c)
{
    if (isalpha((unsigned char)*c))
    {
        *c = toupper((unsigned char)*c);
    }
    else if (isdigit((unsigned char)*c))
    {
        *c = '0';
    }
    else
    {
        *c = '0';
    }
}

BOOL is_file_locked(const char *filePath)
{
    BPTR lock = Lock(filePath, ACCESS_READ);

    if (lock)
    {
        UnLock(lock);
        return FALSE;
    }

    return TRUE;
}

BOOL append_string_to_file(const char *target_filename, const char *append_text, BOOL create_new_file)
{
    BPTR file_handle;
    LONG file_open_mode = (create_new_file == 1) ? MODE_NEWFILE : MODE_READWRITE;
    size_t textLength = strlen(append_text);

    if (create_new_file == 1)
    {
        if (!DeleteFile(target_filename))
        {
            printf("Error: Unable to delete existing file %s. IoErr: %ld\n", target_filename, (long)IoErr());
            return FALSE;
        }
    }

    {
        BOOL needsNewline = (append_text != NULL && textLength > 0 && append_text[textLength - 1] != '\n');

        file_handle = Open((CONST_STRPTR)target_filename, file_open_mode);
        if (!file_handle)
        {
            printf("Error: Unable to open file %s. IoErr: %ld\n", target_filename, (long)IoErr());
            return FALSE;
        }

        if (!create_new_file)
        {
            Seek(file_handle, 0, OFFSET_END);
        }

        if (FPuts(file_handle, (CONST_STRPTR)append_text) == EOF)
        {
            Close(file_handle);
            return FALSE;
        }

        if (needsNewline)
        {
            if (FPuts(file_handle, "\n") == EOF)
            {
                Close(file_handle);
                printf("Error: Unable to write newline to file %s. IoErr: %ld\n", target_filename, (long)IoErr());
                return FALSE;
            }
        }

        Close(file_handle);
    }

    return TRUE;
}

void delete_all_files_in_dir(const char *directory)
{
    struct FileInfoBlock *file_info_block;
    BPTR directory_lock;
    BOOL do_more_entries_exist;
    char clean_path_buffer[256];

    strcpy(clean_path_buffer, directory);
    sanitize_amiga_file_path(clean_path_buffer);

    if (!(file_info_block = AllocDosObject(DOS_FIB, NULL)))
    {
        printf("Failed to allocate memory for FileInfoBlock\n");
        return;
    }

    if (!(directory_lock = Lock(clean_path_buffer, ACCESS_READ)))
    {
        printf("Failed to lock the directory: %s\n", clean_path_buffer);
        printf("Error message: (%ld) \n", (long)IoErr());
        FreeDosObject(DOS_FIB, file_info_block);
        return;
    }

    if (!Examine(directory_lock, file_info_block))
    {
        printf("Failed to examine the directory: %s\n", clean_path_buffer);
        UnLock(directory_lock);
        FreeDosObject(DOS_FIB, file_info_block);
        return;
    }

    do_more_entries_exist = ExNext(directory_lock, file_info_block);
    while (do_more_entries_exist)
    {
        if (file_info_block->fib_DirEntryType < 0)
        {
            char filePath[256];
            sprintf(filePath, "%s/%s", clean_path_buffer, file_info_block->fib_FileName);

            if (!DeleteFile(filePath))
            {
                printf("Failed to delete file: %s\n", filePath);
            }
        }
        do_more_entries_exist = ExNext(directory_lock, file_info_block);
    }

    if (IoErr() != ERROR_NO_MORE_ENTRIES)
    {
        printf("Error reading directory\n");
    }

    UnLock(directory_lock);
    FreeDosObject(DOS_FIB, file_info_block);
}

void sanitize_amiga_file_path(char *path)
{
    ULONG path_len;
    char *sanitized_path;
    int src_index;
    int dest_index;

    if (path == NULL)
    {
        return;
    }

    path_len = strlen(path) + 1;
    sanitized_path = (char *)amiga_malloc(path_len);
    if (sanitized_path == NULL)
    {
        return;
    }

    src_index = 0;
    dest_index = 0;

    while (path[src_index] != '\0')
    {
        if (path[src_index] == ':')
        {
            sanitized_path[dest_index++] = path[src_index++];

            while (path[src_index] == '/')
            {
                src_index++;
            }
        }
        else if (path[src_index] == '/' && path[src_index + 1] == '/')
        {
            src_index++;
        }
        else if (strchr("*?#|<>\"{}", path[src_index]) != NULL)
        {
            sanitized_path[dest_index++] = '_';
            src_index++;
        }
        else if ((unsigned char)path[src_index] < 32)
        {
            src_index++;
        }
        else
        {
            sanitized_path[dest_index++] = path[src_index++];
        }
    }

    if (dest_index > 0 && sanitized_path[dest_index - 1] == '/')
    {
        dest_index--;
    }

    sanitized_path[dest_index] = '\0';
    remove_CR_LF_from_string(sanitized_path);
    trim(sanitized_path);
    strcpy(path, sanitized_path);

    amiga_free(sanitized_path);
}

static BOOL is_existing_directory(const char *path)
{
    BPTR existing_lock;
    struct FileInfoBlock *fib;
    BOOL is_directory = FALSE;

    if (path == NULL || path[0] == '\0')
    {
        return FALSE;
    }

    existing_lock = Lock(path, ACCESS_READ);
    if (!existing_lock)
    {
        return FALSE;
    }

    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL)
    {
        if (Examine(existing_lock, fib))
        {
            if (fib->fib_DirEntryType >= 0)
            {
                is_directory = TRUE;
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }

    UnLock(existing_lock);
    return is_directory;
}

BOOL create_Directory_and_unlock(const char *dirName)
{
    char path_copy[512];
    char *slash;
    BPTR lock;

    if (dirName == NULL || dirName[0] == '\0')
    {
        log_error(LOG_GENERAL, "dir: invalid directory name\n");
        return FALSE;
    }

    if (is_existing_directory(dirName))
    {
        log_debug(LOG_GENERAL, "dir: already exists '%s'\n", dirName);
        return TRUE;
    }

    strncpy(path_copy, dirName, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    slash = strchr(path_copy, '/');
    while (slash != NULL)
    {
        BPTR step_lock;

        *slash = '\0';
        if (path_copy[0] != '\0' && does_file_or_folder_exist(path_copy, 0) == 0)
        {
            step_lock = CreateDir(path_copy);
            if (step_lock)
            {
                UnLock(step_lock);
                log_info(LOG_GENERAL, "dir: created intermediate '%s'\n", path_copy);
            }
            else
            {
                LONG err = IoErr();
                if (err != ERROR_OBJECT_EXISTS)
                {
                    if (is_existing_directory(path_copy))
                    {
                        log_debug(LOG_GENERAL, "dir: intermediate already exists '%s'\n", path_copy);
                        *slash = '/';
                        slash = strchr(slash + 1, '/');
                        continue;
                    }

                    log_error(LOG_GENERAL,
                              "dir: failed to create intermediate '%s' (IoErr=%ld)\n",
                              path_copy,
                              (long)err);
                    return FALSE;
                }
                log_debug(LOG_GENERAL, "dir: intermediate already exists '%s'\n", path_copy);
            }
        }
        *slash = '/';
        slash = strchr(slash + 1, '/');
    }

    lock = CreateDir(dirName);
    if (lock)
    {
        UnLock(lock);
        log_info(LOG_GENERAL, "dir: created '%s'\n", dirName);
        return TRUE;
    }
    else
    {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_EXISTS)
        {
            log_debug(LOG_GENERAL, "dir: already exists '%s'\n", dirName);
            return TRUE;
        }

        if (is_existing_directory(dirName))
        {
            log_debug(LOG_GENERAL, "dir: already exists '%s'\n", dirName);
            return TRUE;
        }

        log_error(LOG_GENERAL, "dir: failed to create '%s' (IoErr=%ld)\n", dirName, (long)err);
        return FALSE;
    }
}

void remove_all_occurrences(char *source_string, const char *toRemove)
{
    int lenToRemove = strlen(toRemove);
    char *p = strstr(source_string, toRemove);
    while (p != NULL)
    {
        *p = '\0';
        strcat(source_string, p + lenToRemove);
        p = strstr(source_string, toRemove);
    }
}

void ensure_time_zone_set(void)
{
    CONST_STRPTR name = "TZ";
    CONST_STRPTR value = "GMT0BST,M3.5.0/01,M10.5.0/02";
    LONG size = strlen(value) + 1;
    LONG flags = GVF_GLOBAL_ONLY;
    char *tz = getenv("TZ");

    if (tz == NULL || strlen(tz) == 0)
    {
        SetVar(name, value, size, flags);
    }
}

int get_bsdSocket_version(void)
{
    struct Library *socket_base;
    int version = 0;

    socket_base = OpenLibrary("bsdsocket.library", 0);
    if (socket_base == NULL)
    {
        return -1;
    }

    version = socket_base->lib_Version;
    CloseLibrary(socket_base);

    return version;
}

void create_directory_based_on_filename(const char *parentDir, const char *fileName)
{
    size_t dirLen = strlen(parentDir);
    char downloadFirstLetter[2];
    char fileStore[256];
    char cleanParentDir[256];

    strcpy(cleanParentDir, parentDir);

    if (cleanParentDir[dirLen - 1] == '/')
    {
        cleanParentDir[dirLen - 1] = '\0';
    }

    create_Directory_and_unlock(DIR_GAME_DOWNLOADS);
    sprintf(fileStore, "%s/%s", DIR_GAME_DOWNLOADS, cleanParentDir);
    sanitize_amiga_file_path(fileStore);

    create_Directory_and_unlock(fileStore);
    sprintf(downloadFirstLetter, "%c", fileName[0]);
    get_folder_name_from_character(downloadFirstLetter);
    sprintf(fileStore, "%s/%s/%s", DIR_GAME_DOWNLOADS, cleanParentDir, downloadFirstLetter);
    sanitize_amiga_file_path(fileStore);

    create_Directory_and_unlock(fileStore);
}

char *concatStrings(const char *s1, const char *s2)
{
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char *result = amiga_malloc(len1 + len2 + 1);

    if (result == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char *get_executable_version(const char *file_path)
{
    BPTR file;
    UBYTE buffer[UTIL_FILE_SCAN_BUFFER_SIZE];
    char *version = NULL;
    char *clean_version = NULL;
    char *error_msg = NULL;
    LONG bytes_read;
    int i;
    int start;
    int end;
    int version_len;
    int clean_len;
    const int tag_len = 5;

    if (!does_file_or_folder_exist(file_path, 0))
    {
        error_msg = amiga_malloc(128);
        if (error_msg != NULL)
        {
            sprintf(error_msg, "[File not found: %s]", file_path);
        }
        return error_msg;
    }

    file = Open((CONST_STRPTR)file_path, MODE_OLDFILE);
    if (!file)
    {
        error_msg = amiga_malloc(64);
        if (error_msg != NULL)
        {
            strcpy(error_msg, "[Unable to open file]");
        }
        return error_msg;
    }

    while ((bytes_read = Read(file, buffer, UTIL_FILE_SCAN_BUFFER_SIZE)) > 0)
    {
        for (i = 0; i <= bytes_read - tag_len; ++i)
        {
            if (buffer[i] == '$' &&
                buffer[i + 1] == 'V' &&
                buffer[i + 2] == 'E' &&
                buffer[i + 3] == 'R' &&
                buffer[i + 4] == ':')
            {
                start = i;
                end = i + tag_len;
                while (end < bytes_read && buffer[end] >= 0x20 && buffer[end] <= 0x7E)
                {
                    ++end;
                }

                version_len = end - start;
                version = amiga_malloc(version_len + 1);

                if (version != NULL)
                {
                    CopyMem(&buffer[start], version, version_len);
                    version[version_len] = '\0';

                    if (version_len > tag_len)
                    {
                        clean_len = version_len - tag_len;
                        clean_version = amiga_malloc(clean_len + 1);

                        if (clean_version != NULL)
                        {
                            CopyMem(version + tag_len, clean_version, clean_len);
                            clean_version[clean_len] = '\0';
                        }
                    }

                    amiga_free(version);
                }

                Close(file);
                return clean_version;
            }
        }
    }

    Close(file);

    error_msg = amiga_malloc(64);
    if (error_msg != NULL)
    {
        strcpy(error_msg, "[No version info found]");
    }
    return error_msg;
}

/*------------------------------------------------------------------------*/
/* Download marker helpers                                                */
/*                                                                        */
/* A zero-byte "<archive>.downloading" file is created next to the .lha   */
/* before download starts and deleted on success.  If the program is      */
/* interrupted, the marker survives and the next run knows to delete the  */
/* partial archive and re-download it.                                    */
/*------------------------------------------------------------------------*/

#define DOWNLOAD_MARKER_SUFFIX ".downloading"

static void build_marker_path(const char *archive_path, char *out, size_t out_size)
{
    strncpy(out, archive_path, out_size - sizeof(DOWNLOAD_MARKER_SUFFIX));
    out[out_size - sizeof(DOWNLOAD_MARKER_SUFFIX)] = '\0';
    strcat(out, DOWNLOAD_MARKER_SUFFIX);
}

BOOL create_download_marker(const char *archive_path)
{
    char marker_path[512];
    BPTR fh;

    build_marker_path(archive_path, marker_path, sizeof(marker_path));
    fh = Open(marker_path, MODE_NEWFILE);
    if (!fh)
    {
        return FALSE;
    }
    Close(fh);
    return TRUE;
}

void delete_download_marker(const char *archive_path)
{
    char marker_path[512];

    build_marker_path(archive_path, marker_path, sizeof(marker_path));
    DeleteFile(marker_path);
}

BOOL has_download_marker(const char *archive_path)
{
    char marker_path[512];
    BPTR lock;

    build_marker_path(archive_path, marker_path, sizeof(marker_path));
    lock = Lock(marker_path, ACCESS_READ);
    if (lock)
    {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}