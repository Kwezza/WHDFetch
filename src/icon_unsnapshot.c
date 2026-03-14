/*
 * icon_unsnapshot.c - Standalone helper to clear Workbench icon snapshot position
 */

#include "icon_unsnapshot.h"

#include <string.h>
#include <ctype.h>

#include <libraries/dos.h>
#include <dos/dos.h>
#include <workbench/icon.h>

#include <proto/dos.h>
#include <proto/icon.h>

#define ICON_PATH_MAX 1024

static BOOL has_info_suffix(const char *path)
{
    size_t len;

    if (path == NULL)
    {
        return FALSE;
    }

    len = strlen(path);
    if (len < 5)
    {
        return FALSE;
    }

    return (tolower((unsigned char)path[len - 5]) == '.' &&
            tolower((unsigned char)path[len - 4]) == 'i' &&
            tolower((unsigned char)path[len - 3]) == 'n' &&
            tolower((unsigned char)path[len - 2]) == 'f' &&
            tolower((unsigned char)path[len - 1]) == 'o');
}

static BOOL build_icon_paths(const char *input_path, char *base_path, size_t base_size,
                             char *info_path, size_t info_size)
{
    size_t input_len;

    if (input_path == NULL || base_path == NULL || info_path == NULL)
    {
        return FALSE;
    }

    input_len = strlen(input_path);
    if (input_len == 0)
    {
        return FALSE;
    }

    if (has_info_suffix(input_path))
    {
        size_t base_len = input_len - 5;

        if (base_len == 0 || input_len >= info_size || base_len >= base_size)
        {
            return FALSE;
        }

        strncpy(info_path, input_path, info_size - 1);
        info_path[info_size - 1] = '\0';

        strncpy(base_path, input_path, base_len);
        base_path[base_len] = '\0';
    }
    else
    {
        if (input_len >= base_size || (input_len + 5) >= info_size)
        {
            return FALSE;
        }

        strncpy(base_path, input_path, base_size - 1);
        base_path[base_size - 1] = '\0';

        strncpy(info_path, input_path, info_size - 1);
        info_path[info_size - 1] = '\0';
        strcat(info_path, ".info");
    }

    return TRUE;
}

static BOOL get_file_protection_bits(const char *file_path, LONG *protection_bits)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    BOOL ok = FALSE;

    if (file_path == NULL || protection_bits == NULL)
    {
        return FALSE;
    }

    lock = Lock((STRPTR)file_path, ACCESS_READ);
    if (lock == 0)
    {
        return FALSE;
    }

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib == NULL)
    {
        UnLock(lock);
        return FALSE;
    }

    if (Examine(lock, fib) != DOSFALSE)
    {
        *protection_bits = fib->fib_Protection;
        ok = TRUE;
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);

    return ok;
}

BOOL strip_icon_position(const char *icon_path)
{
    char base_path[ICON_PATH_MAX];
    char info_path[ICON_PATH_MAX];
    struct DiskObject *disk_object = NULL;
    LONG original_protection = 0;
    LONG unprotected_bits = 0;
    BOOL protection_changed = FALSE;
    BOOL success = FALSE;

    if (!build_icon_paths(icon_path, base_path, sizeof(base_path), info_path, sizeof(info_path)))
    {
        return FALSE;
    }

    /* Read current protection so we can temporarily clear write-protect and restore it. */
    if (!get_file_protection_bits(info_path, &original_protection))
    {
        return FALSE;
    }

    if ((original_protection & FIBF_WRITE) != 0)
    {
        unprotected_bits = original_protection & ~FIBF_WRITE;
        if (SetProtection((STRPTR)info_path, unprotected_bits) == DOSFALSE)
        {
            return FALSE;
        }
        protection_changed = TRUE;
    }

    disk_object = GetDiskObject((STRPTR)base_path);
    if (disk_object == NULL)
    {
        goto cleanup;
    }

    disk_object->do_CurrentX = NO_ICON_POSITION;
    disk_object->do_CurrentY = NO_ICON_POSITION;

    if (PutDiskObject((STRPTR)base_path, disk_object) != DOSFALSE)
    {
        success = TRUE;
    }

cleanup:
    if (disk_object != NULL)
    {
        FreeDiskObject(disk_object);
    }

    if (protection_changed)
    {
        if (SetProtection((STRPTR)info_path, original_protection) == DOSFALSE)
        {
            success = FALSE;
        }
    }

    return success;
}
