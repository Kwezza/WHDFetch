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