#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "display_message.h"
#include "amiga_download.h" /* Added for error code definitions */

/* ANSI escape codes for terminal control */
#define CLI_CLEARLINE "\x1b[2K\r"     /* Clear line escape sequence */
#define CLI_MOVE_UP_ONE_LINE "\x1b[A" /* Move cursor up one line escape sequence */

/**
 * @brief Displays a message on the screen with appropriate formatting
 *
 * @param msg_type Type of message (MSG_ERROR, MSG_INFO, MSG_DEBUG, etc.)
 * @param file Source file where the message is generated (__FILE__)
 * @param line Line number where the message is generated (__LINE__)
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 */
void display_message(int msg_type, const char *file, int line, const char *format, ...)
{
    va_list args;
    char buffer[256];            /* Buffer for formatted message */
    const char *filename = file; /* Pointer to extract filename from path */

    /* Extract filename from path by finding the last path separator */
    if (file != NULL)
    {
        const char *last_slash = strrchr(file, '/');
        const char *last_backslash = strrchr(file, '\\');

        /* Find the last directory separator (either slash or backslash) */
        if (last_slash != NULL && (last_backslash == NULL || last_slash > last_backslash))
        {
            filename = last_slash + 1;
        }
        else if (last_backslash != NULL)
        {
            filename = last_backslash + 1;
        }
        /* If no separator found, filename remains pointing to the original string */
    }

#ifdef DEBUG
    {
        /* Only display debug messages if DEBUG is defined */
        /* Include filename and line information for debug messages */
        char debug_prefix[128]; /* Buffer for debug prefix */
        sprintf(debug_prefix, "[%s:%d]: ", filename, line);
        PutStr(debug_prefix);
    }
#endif

    /* Format the message */
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    /* Handle different message types */
    switch (msg_type)
    {
    case MSG_ERROR:
        /* Display error messages with "ERROR: " prefix */
        PutStr("ERROR: ");
        PutStr(buffer);
        PutStr("\n");
        break;

    case MSG_INFO:
        /* Display informational messages as-is */
        PutStr(buffer);
        break;

    case MSG_DEBUG:
#ifdef DEBUG
        /* Only display debug messages if DEBUG is defined */
        /* Include filename, function, and line information for debug messages */

        PutStr(buffer);
        PutStr("\n");

#endif
        break;

    case MSG_PROGRESS:
        /* Progress updates clear the line and stay on the same line */
        PutStr(CLI_CLEARLINE);
        PutStr(buffer);
        PutStr("\r");
        break;

    case MSG_CLEAR:
        /* Just clear the line */
        PutStr(CLI_CLEARLINE);
        break;
    }

    /* Flush output to ensure immediate display */
    Flush(Output());
}

/**
 * @brief Displays a standardized progress bar with percentage and optional message
 *
 * @param percent Percentage complete (0-100)
 * @param message Additional message to display (speed, etc.), can be NULL
 * @param prefix Text to display before the progress bar (e.g. "Downloading: ")
 */
void display_progress_bar(int percent, const char *message, const char *prefix)
{
    char progress_text[256]; /* Complete progress text buffer */
    char bar_buffer[80];     /* Just the progress bar portion */
    int bar_width = 40;      /* Width of the progress bar in characters */
    int i, pos = 0;

    /* Ensure percent is in valid range */
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    /* Add prefix if provided */
    if (prefix && *prefix)
    {
        strcpy(progress_text, prefix);
    }
    else
    {
        progress_text[0] = '\0';
    }

    /* Create the progress bar */
    bar_buffer[pos++] = '[';

    /* Fill the bar with appropriate characters */
    for (i = 0; i < bar_width; i++)
    {
        if (i < (bar_width * percent / 100))
        {
            bar_buffer[pos++] = '#';
        }
        else
        {
            bar_buffer[pos++] = ' ';
        }
    }

    /* Close the bar */
    bar_buffer[pos++] = ']';
    bar_buffer[pos++] = ' ';
    bar_buffer[pos] = '\0';

    /* Add the percentage */
    sprintf(bar_buffer + pos, "%3d%%", percent);

    /* Add the bar to the progress text */
    strcat(progress_text, bar_buffer);

    /* Add the message if provided */
    if (message && *message)
    {
        strcat(progress_text, " ");
        strcat(progress_text, message);
    }

    /* Display the complete progress bar */
    display_message(MSG_PROGRESS, __FILE__, __LINE__, "%s", progress_text);
}

/**
 * @brief Displays a descriptive error message based on the download error code
 *
 * This function takes an error code from the download library and prints a
 * user-friendly description of the error to the standard output.
 *
 * @param error_code The error code returned by DownloadHTTPFile or other functions
 */
void ad_print_download_error(int error_code)
{
    char *error_message; /* Pointer to the error message string */

    /* Determine the appropriate error message based on the error code */
    switch (error_code)
    {
    case AD_NOT_INITIALIZED:
        error_message = "Download library not initialized";
        break;

    case AD_INVALID_URL:
        error_message = "Invalid URL format (must start with http://)";
        break;

    case AD_HOST_NOT_FOUND:
        error_message = "Host not found (DNS lookup failure)";
        break;

    case AD_SOCKET_ERROR:
        error_message = "Socket error (network connection failed)";
        break;

    case AD_CONNECT_ERROR:
        error_message = "Could not connect to server";
        break;

    case AD_SEND_ERROR:
        error_message = "Failed to send HTTP request";
        break;

    case AD_RECEIVE_ERROR:
        error_message = "Failed to receive data from server";
        break;

    case AD_HEADER_ERROR:
        error_message = "Error parsing HTTP headers";
        break;

    case AD_FILE_ERROR:
        error_message = "File I/O error (disk full or write protected?)";
        break;

    case AD_MEMORY_ERROR:
        error_message = "Memory allocation failed";
        break;

    case AD_TIMEOUT:
        error_message = "Connection timed out";
        break;

    case AD_BAD_REQUEST:
        error_message = "Server reported bad request (HTTP 400)";
        break;

    case AD_UNAUTHORIZED:
        error_message = "Authentication required (HTTP 401)";
        break;

    case AD_FORBIDDEN:
        error_message = "Access forbidden (HTTP 403)";
        break;

    case AD_NOT_FOUND:
        error_message = "File not found on server (HTTP 404)";
        break;

    case AD_SERVER_ERROR:
        error_message = "Internal server error (HTTP 500)";
        break;

    case AD_SERVICE_UNAVAILABLE:
        error_message = "Service temporarily unavailable (HTTP 503)";
        break;

    case AD_CRC_ERROR:
        error_message = "CRC verification failed";
        break;

    case AD_CRC_FORMAT_ERROR:
        error_message = "Invalid CRC format";
        break;

    case AD_REQUEST_TOO_LONG:
        error_message = "HTTP request exceeds buffer size";
        break;

    default:
        /* For HTTP status codes or other unknown errors */
        if (error_code >= 400 && error_code < 600)
        {
            /* Format a message for HTTP status codes */
            static char http_error_buffer[40]; /* Buffer for formatted HTTP error */
            sprintf(http_error_buffer, "HTTP error %d", error_code);
            error_message = http_error_buffer;
        }
        else
        {
            error_message = "Unknown error";
        }
        break;
    }

    /* Display the error message with the code */
    display_message(MSG_ERROR, __FILE__, __LINE__, "%s (error code: %d)", error_message, error_code);
} /* ad_print_download_error */
