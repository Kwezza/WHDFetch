#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "file_crc.h"
#include "timer_shared.h"    /* Include the shared timer header */
#include "display_message.h" /* Include the shared message display header */
#include "amiga_download.h"
#include "../platform/platform.h"

/* Size of buffer to use when reading file */
#define CRC_BUFFER_SIZE 4096

/* CRC-32 polynomial constant for calculation */
#define CRC32_POLYNOMIAL 0xEDB88320L

/* Pre-computed CRC table for faster calculation */
static ULONG crc32_table[256];
static BOOL crc32_table_computed = FALSE;

/**
 * Initialize the CRC-32 table for faster calculations
 */
static void init_crc32_table(void)
{
    ULONG c;
    int n, k;

    for (n = 0; n < 256; n++)
    {
        c = (ULONG)n;
        for (k = 0; k < 8; k++)
        {
            if (c & 1)
            {
                c = CRC32_POLYNOMIAL ^ (c >> 1);
            }
            else
            {
                c = c >> 1;
            }
        }
        crc32_table[n] = c;
    }

    crc32_table_computed = TRUE;
}

/**
 * Open the timer device
 *
 * @return TRUE if successful, FALSE otherwise
 */
static BOOL open_timer(void)
{
    /* Timer lifecycle is owned by download_lib initialization.
       CRC verification only borrows the timer when available. */
    return (TimerBase != NULL) ? TRUE : FALSE;
}

/**
 * Close the timer device
 */
static void close_timer(void)
{
    /* No-op: do not tear down shared timer from CRC verification. */
}

/**
 * Calculate CRC-32 value for a block of data
 *
 * @param buf Pointer to the data
 * @param len Length of the data in bytes
 * @param initial_crc Initial CRC value (use 0xFFFFFFFF for new calculation)
 * @return Updated CRC-32 value
 */
static ULONG calculate_crc32(UBYTE *buf, ULONG len, ULONG initial_crc)
{
    ULONG crc = initial_crc;
    ULONG i;

    if (!crc32_table_computed)
    {
        init_crc32_table();
    }

    for (i = 0; i < len; i++)
    {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc;
}

/**
 * Converts a hex character to its integer value
 *
 * @param c The character to convert
 * @return The integer value or -1 if not a valid hex character
 */
static int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1; /* Invalid hex character */
}

/**
 * Converts a hex string to an unsigned long
 *
 * @param hex_str The hex string (with or without "0x" prefix)
 * @param result Pointer to store the resulting value
 * @return 0 on success, -1 on error
 */
static int parse_hex_string(const char *hex_str, ULONG *result)
{
    ULONG value = 0;
    int i, digit;
    int start = 0;
    int len;

    if (!hex_str || !result)
        return -1;

    /* Skip "0x" prefix if present */
    if (hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X'))
        start = 2;

    len = strlen(hex_str);

    /* Process each character */
    for (i = start; i < len; i++)
    {
        digit = hex_char_to_int(hex_str[i]);
        if (digit < 0)
            return -1; /* Invalid hex character */

        value = (value << 4) | digit;
    }

    *result = value;
    return 0;
}

/**
 * Verifies if a file matches the expected CRC-32 checksum
 * 
 * @param filename Path to the file to verify
 * @param expected_crc_str The expected CRC-32 value as a hex string (e.g. "A1B2C3D4")
 * @return AD_SUCCESS if CRC matches, AD_CRC_ERROR if mismatch, other error codes for failures
 */
int ad_verify_file_crc(const char *filename, const char *expected_crc_str)
{
    /* Variables with meaningful initial values - keep these initializations */
    ULONG calculated_crc = 0xFFFFFFFF; /* Initial value for CRC-32 */
    ULONG expected_crc = 0;
    int result = AD_ERROR;
    int last_percent = -1;
    char speed_str[64] = "";
    char message_buffer[256]; /* Buffer for formatted messages */
    
    /* Variables that will be assigned later - don't initialize */
    BPTR file = 0;
    UBYTE *buffer = NULL;
    LONG bytes_read;
    ULONG file_size = 0;
    ULONG bytes_processed = 0;
    int percent_complete;
    struct timeval current_time;
    struct timeval last_update_time;
    ULONG time_diff;
    ULONG processing_speed;
    BOOL have_timer = FALSE;
    
    /* Initialize timeval structure properly since we may use it before setting all fields */
    last_update_time.tv_secs = 0;
    last_update_time.tv_micro = 0;

    /* Parse the expected CRC from hex string */
    if (parse_hex_string(expected_crc_str, &expected_crc) != 0)
    {
        display_message(MSG_ERROR,__FILE__,__LINE__, "Invalid CRC format '%s'", expected_crc_str);
        return AD_CRC_FORMAT_ERROR;
    }

    /* Initialize the CRC table if needed */
    if (!crc32_table_computed)
    {
        init_crc32_table();
    }

    /* Open timer device for progress updates */
    have_timer = open_timer();

    /* Allocate read buffer */
    buffer = (UBYTE *)amiga_malloc(CRC_BUFFER_SIZE);
    if (!buffer)
    {
        display_message(MSG_ERROR,__FILE__,__LINE__, "Out of memory");
        if (have_timer)
        {
            close_timer();
        }
        return AD_MEMORY_ERROR;
    }

    /* Open the file for reading */
    file = Open((STRPTR)filename, MODE_OLDFILE);
    if (!file)
    {
        display_message(MSG_ERROR,__FILE__,__LINE__, "Could not open file '%s'", filename);
        amiga_free(buffer);
        if (have_timer)
        {
            close_timer();
        }
        return AD_FILE_ERROR;
    }

    /* Get file size */
    {
        struct FileInfoBlock *fib;
        fib = (struct FileInfoBlock *)amiga_malloc(sizeof(struct FileInfoBlock));
        if (fib)
        {
            if (ExamineFH(file, fib))
            {
                file_size = fib->fib_Size;
            }
            amiga_free(fib);
        }
    }

    /* Initialize timing */
    if (have_timer)
    {
        GetSysTime(&last_update_time);
    }

    /* Process file in chunks */
    while ((bytes_read = Read(file, buffer, CRC_BUFFER_SIZE)) > 0)
    {
        calculated_crc = calculate_crc32(buffer, bytes_read, calculated_crc);
        bytes_processed += bytes_read;

        /* Calculate percentage complete */
        if (file_size > 0)
        {
            percent_complete = (int)((bytes_processed * 100) / file_size);

            /* Update progress bar if we have timer */
            if (have_timer)
            {
                GetSysTime(&current_time);

                /* Calculate time difference in microseconds */
                time_diff = (current_time.tv_secs - last_update_time.tv_secs) * 1000000 +
                           (current_time.tv_micro - last_update_time.tv_micro);

                /* Only update once per second */
                if (time_diff >= 1000000)
                {
                    /* Calculate processing speed (bytes per second) */
                    processing_speed = (bytes_processed * 1000000) / (time_diff ? time_diff : 1);

                    /* Format speed string */
                    if (processing_speed < 1024)
                    {
                        sprintf(speed_str, "(%lu B/s)", processing_speed);
                    }
                    else if (processing_speed < 1024 * 1024)
                    {
                        sprintf(speed_str, "(%lu KB/s)", processing_speed / 1024);
                    }
                    else
                    {
                        sprintf(speed_str, "(%lu.%02lu MB/s)",
                                processing_speed / (1024 * 1024),
                                ((processing_speed % (1024 * 1024)) * 100) / (1024 * 1024));
                    }

                    /* Display progress using the new shared function */
                    if (percent_complete != last_percent)
                    {
                        /* Use "Verifying: " as the prefix */
                        display_progress_bar(percent_complete, speed_str, "Verifying: ");
                        last_percent = percent_complete;
                    }

                    /* Reset timer */
                    last_update_time = current_time;
                }
            }
            else
            {
                /* No timer available, use simpler updates */
                if (percent_complete != last_percent &&
                    (percent_complete % 10 == 0 || percent_complete == 100))
                {
                    sprintf(message_buffer, "Verifying: %d%% complete", percent_complete);
                    display_message(MSG_PROGRESS,__FILE__,__LINE__, "%s", message_buffer);
                    last_percent = percent_complete;
                }
            }
        }
    }

    /* Complete the CRC calculation */
    calculated_crc = ~calculated_crc;

    /* Final newline after progress display */
    printf(CLI_CLEARLINE);
    printf("\r");

    /* Check if CRC matches */
    if (calculated_crc != expected_crc)
    {
        display_message(MSG_ERROR,__FILE__,__LINE__, "CRC verification failed");
        result = AD_CRC_ERROR;
    }
    else
    {
        result = AD_SUCCESS;
    }

    /* Clean up in proper order - always executed regardless of CRC match */
    if (file)
    {
        Close(file);
    }

    if (buffer)
    {
        amiga_free(buffer);
    }

    if (have_timer)
    {
        close_timer();
    }

    return result;
} /* ad_verify_file_crc */