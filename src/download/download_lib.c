/*------------------------------------------------------------------------*/
/*                                                                        *
 *  $Id: download_lib.c,v 1.0 2025/05/20 13:45:00 user Exp $             *
 *                                                                        */
/*------------------------------------------------------------------------*/

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <utility/tagitem.h>  /* For TagItem structure */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdarg.h>
#include <stdio.h>

#include "amiga_download.h"
#include "display_message.h"
#include "timer_shared.h"

/* Message types from display_message.h */
#ifdef MISSING_DISPLAY_MESSAGE_DEFS
#define MSG_ERROR 0
#define MSG_INFO 1
#define MSG_DEBUG 2
#endif

/* Internal library initialization flags */
static BOOL socket_lib_opened = FALSE;	/* Flag to track if socket library is open */
static BOOL lib_initialized = FALSE;	/* Flag to track overall library initialization */

/* Global socket library base that will be hidden from external users */
struct Library *SocketBase = NULL;

/* Library version information */
static const UBYTE lib_version_string[] = "$VER: amiga_download.library 1.0 (29.10.2023)";
static const UWORD lib_version = 1;
static const UWORD lib_revision = 0;

/*------------------------------------------------------------------------*/

/**
 * @brief Walk a TagItem array and return the data for the first matching tag
 *
 * Replaces GetTagData() from utility.library so that download_lib.c has no
 * dependency on UtilityBase.  This is sufficient because we only ever pass
 * flat, statically-built tag arrays (no ti_Tag == TAG_MORE chains).
 *
 * @param find_tag  Tag to search for
 * @param def       Default value if tag is not found
 * @param tags      Pointer to TAG_DONE-terminated TagItem array
 * @return Data value of the matching tag, or def if not found
 */
static ULONG local_get_tag_data(Tag find_tag, ULONG def, const struct TagItem *tags)
{
	const struct TagItem *ti; /* Current tag item pointer */
	if (!tags) { return def; }
	for (ti = tags; ti->ti_Tag != TAG_DONE; ti++)
	{
		if (ti->ti_Tag == find_tag) { return ti->ti_Data; }
	}
	return def;
} /* local_get_tag_data */

/*------------------------------------------------------------------------*/

/* Default configuration values */
static BOOL default_verbose = FALSE;        /* Default verbose setting */
static ULONG default_timeout = 30;          /* Default 30 second timeout */
static ULONG default_buffer_size = 8192;    /* Default 8K buffer size */
static STRPTR default_user_agent = "AmigaHTTP/1.0"; /* Default user agent */
static ULONG default_max_retries = 3;       /* Default retry count */

/* Current configuration - set during initialization */
static BOOL config_verbose;                 /* Current verbose mode setting */
static ULONG config_timeout;                /* Current timeout in seconds */
static ULONG config_buffer_size;            /* Current buffer size */
static STRPTR config_user_agent;            /* Current user agent string */
static ULONG config_max_retries;            /* Maximum connection retries */

static BOOL ad_is_retryable_download_error(int error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
        case AD_SOCKET_ERROR:
        case AD_CONNECT_ERROR:
        case AD_CONNECTION_ERROR:
            return TRUE;
        default:
            return FALSE;
    }
}

static const char *ad_retry_reason_text(int error_code)
{
    switch (error_code)
    {
        case AD_TIMEOUT:
            return "timeout";
        case AD_SOCKET_ERROR:
            return "socket error";
        case AD_CONNECT_ERROR:
            return "connect error";
        case AD_CONNECTION_ERROR:
            return "connection closed";
        default:
            return "transient error";
    }
}

static const char *ad_path_filename(const char *path)
{
    const char *filename = path;
    const char *cursor;

    if (!path || path[0] == '\0')
    {
        return "(unknown file)";
    }

    for (cursor = path; *cursor != '\0'; cursor++)
    {
        if (*cursor == '/' || *cursor == '\\' || *cursor == ':')
        {
            filename = cursor + 1;
        }
    }

    if (filename[0] == '\0')
    {
        return "(unknown file)";
    }

    return filename;
}

/*------------------------------------------------------------------------*/

/**
 * @brief Initializes the download library
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
BOOL ad_init_download_lib(void)
{
    /* Use a simple empty tag list to get defaults */
    struct TagItem tags[] = {
        {TAG_DONE, 0}
    };
    
    return ad_init_download_lib_tags(tags);
} /* ad_init_download_lib */

/*------------------------------------------------------------------------*/

/**
 * @brief Initializes the download library with specified options
 *
 * @param tags Tag list of initialization options
 * @return TRUE if initialization was successful, FALSE otherwise
 */
BOOL ad_init_download_lib_tags(struct TagItem *tags)
{
    STRPTR custom_agent; /* Custom user agent string if provided */
    
    /* Skip if already initialized */
    if (lib_initialized)
    {
        return TRUE;
    }
    
    /* Set default configuration values first */
    config_verbose = default_verbose;
    config_timeout = default_timeout;
    config_buffer_size = default_buffer_size;
    config_user_agent = default_user_agent;
    config_max_retries = default_max_retries;
    
    /* Process tags if any */
    if (tags)
    {
        config_verbose = (BOOL)local_get_tag_data(ADTAG_Verbose, default_verbose, tags);
        config_timeout = local_get_tag_data(ADTAG_Timeout, default_timeout, tags);
        config_buffer_size = local_get_tag_data(ADTAG_BufferSize, default_buffer_size, tags);
        config_max_retries = local_get_tag_data(ADTAG_MaxRetries, default_max_retries, tags);
        
        /* Custom user agent - careful with string handling */
        custom_agent = (STRPTR)local_get_tag_data(ADTAG_UserAgent, (ULONG)NULL, tags);
        if (custom_agent)
        {
            config_user_agent = custom_agent;
        }
    }
    
    /* Output verbose initialization message if enabled */
    if (config_verbose)
    {
        display_message(MSG_INFO,__FILE__,__LINE__, "Initializing download library (timeout: %lu, buffer: %lu, retries: %lu)",
                      config_timeout, config_buffer_size, config_max_retries);
    }
    
    /* Initialize timer - uses the shared timer implementation in timer_shared.c */
    if (!InitTimer())
    {
        display_message(MSG_ERROR,__FILE__,__LINE__, "Failed to initialize timer device");
        return FALSE;
    }
    
    /* Initialize socket library */
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase)
    {
        display_message(MSG_ERROR,__FILE__,__LINE__, "Could not open bsdsocket.library");
        ad_cleanup_download_lib();
        return FALSE;
    }
    socket_lib_opened = TRUE;
    
    /* Mark library as initialized */
    lib_initialized = TRUE;
    
    return TRUE;
} /* ad_init_download_lib_tags */

/*------------------------------------------------------------------------*/

/**
 * @brief Initializes the download library with variable argument tag list
 *
 * This function provides a simplified varargs implementation compatible with SAS/C 6.58
 *
 * @param tag1 First tag (or TAG_DONE)
 * @return TRUE if initialization was successful, FALSE otherwise
 */
BOOL ad_init_download_lib_taglist(Tag tag1, ...)
{
    BOOL result;                    /* Function result */
    struct TagItem tags[20];        /* Static tag array with reasonable size limit */
    Tag tag;                        /* Current tag being processed */
    ULONG value;                    /* Current value being processed */
    int count = 0;                  /* Number of tags processed */
    va_list ap;                     /* Variable argument list */

    /* Initialize variable argument traversal */
    va_start(ap, tag1);

    /* Initialize with the first tag */
    tag = tag1;

    /* Process all tag/value pairs until TAG_DONE */
    while (tag != TAG_DONE && count < 19) /* Leave room for TAG_DONE */
    {
        /* Get the value for this tag */
        value = va_arg(ap, ULONG);

        /* Store the tag/value pair */
        tags[count].ti_Tag = tag;
        tags[count].ti_Data = value;
        count++;

        /* Get the next tag */
        tag = (Tag)va_arg(ap, ULONG);
    }

    va_end(ap);

    /* End the tag list */
    tags[count].ti_Tag = TAG_DONE;
    tags[count].ti_Data = 0;

    /* Call the main initialization function */
    result = ad_init_download_lib_tags(tags);

    return result;
} /* ad_init_download_lib_taglist */

/*------------------------------------------------------------------------*/

/**
 * @brief Cleans up the download library
 */
void ad_cleanup_download_lib(void)
{
    /* Clean up socket library if opened */
    if (socket_lib_opened && SocketBase)
    {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
        socket_lib_opened = FALSE;
    }
    
    /* Clean up timer using the shared timer implementation */
    CleanupTimer();
    
    /* Mark library as uninitialized */
    lib_initialized = FALSE;
} /* ad_cleanup_download_lib */

/*------------------------------------------------------------------------*/

/**
 * @brief Checks if the download library is initialized
 * 
 * This function allows users to check if the library is properly initialized
 * without triggering any side effects.
 *
 * @return TRUE if the library is initialized, FALSE otherwise
 */
BOOL ad_is_library_initialized(void)
{
    return lib_initialized;
} /* ad_is_library_initialized */

/**
 * @brief Gets the library initialization status
 * 
 * This function is intended for internal library use only.
 *
 * @return TRUE if the library is initialized, FALSE otherwise
 */
BOOL ad_get_lib_initialized_status(void)
{
    return lib_initialized;
} /* ad_get_lib_initialized_status */

/*------------------------------------------------------------------------*/

/**
 * @brief Gets a configuration value from the library settings
 *
 * This function lets users retrieve configuration values after initialization.
 *
 * @param tag The configuration value to get
 * @param storage Pointer to store the result value
 * @return TRUE if the tag was recognized and storage was updated, FALSE otherwise
 */
BOOL ad_get_config_value(ULONG tag, APTR storage)
{
    if (!lib_initialized || !storage)
    {
        return FALSE;
    }
    
    switch (tag)
    {
        case ADTAG_Verbose:
            *((BOOL *)storage) = config_verbose;
            return TRUE;
            
        case ADTAG_Timeout:
            *((ULONG *)storage) = config_timeout;
            return TRUE;
            
        case ADTAG_BufferSize:
            *((ULONG *)storage) = config_buffer_size;
            return TRUE;
            
        case ADTAG_UserAgent:
            *((STRPTR *)storage) = config_user_agent;
            return TRUE;
            
        case ADTAG_MaxRetries:
            *((ULONG *)storage) = config_max_retries;
            return TRUE;
            
        default:
            return FALSE;
    }
} /* ad_get_config_value */

/*------------------------------------------------------------------------*/

/**
 * @brief Downloads a file via HTTP with retry support for transient network failures
 *
 * Retries are limited to timeout/socket/connect/connection errors and use
 * short exponential backoff (1s, 2s, 4s...).
 */
int ad_download_http_file_with_retries(const char *url, const char *output_path, BOOL silent)
{
    ULONG max_retries = 0;
    ULONG attempt = 0;
    ULONG backoff_seconds = 1;
    int result = AD_ERROR;

    if (!lib_initialized)
    {
        return AD_NOT_INITIALIZED;
    }

    if (!ad_get_config_value(ADTAG_MaxRetries, &max_retries))
    {
        max_retries = default_max_retries;
    }

    while (attempt <= max_retries)
    {
        result = ad_download_http_file(url, output_path, silent);
        if (result == AD_SUCCESS)
        {
            if (attempt > 0)
            {
                printf("Download completed after retry %ld/%ld: %s\n",
                       (long)attempt,
                       (long)max_retries,
                       ad_path_filename(output_path));
            }
            return AD_SUCCESS;
        }

        if (!ad_is_retryable_download_error(result))
        {
            return result;
        }

        if (attempt == max_retries)
        {
            return result;
        }

        printf("Retry %ld/%ld after %s in %ld second%s...\n",
               (long)(attempt + 1),
               (long)max_retries,
               ad_retry_reason_text(result),
               (long)backoff_seconds,
               (backoff_seconds == 1) ? "" : "s");

        if (output_path && output_path[0] != '\0')
        {
            DeleteFile((STRPTR)output_path);
        }

        Delay(backoff_seconds * 50); /* Amiga DOS ticks are 1/50 sec */

        if (backoff_seconds < 8)
        {
            backoff_seconds <<= 1;
        }

        attempt++;
    }

    return result;
}

/*------------------------------------------------------------------------*/

/**
 * @brief Returns the library version as a string
 * 
 * @return Pointer to version string
 */
const char *ad_get_version_string(void)
{
    return (const char *)lib_version_string;
} /* ad_get_version_string */

/**
 * @brief Returns the library version number
 * 
 * @param revision Pointer to store revision number (can be NULL)
 * @return Version number
 */
UWORD ad_get_version(UWORD *revision)
{
    if (revision)
    {
        *revision = lib_revision;
    }
    return lib_version;
} /* ad_get_version */

/*------------------------------------------------------------------------*/
/* End of Text */