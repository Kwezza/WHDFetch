#ifndef DISPLAY_MESSAGE_H
#define DISPLAY_MESSAGE_H

/* Output message types */
#define MSG_ERROR 0    /* Error message */
#define MSG_INFO 1     /* Informational message */
#define MSG_DEBUG 2    /* Debug message */
#define MSG_PROGRESS 3 /* Progress update */
#define MSG_CLEAR 4    /* Clear line */

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
void display_message(int msg_type, const char *file, int line, const char *format, ...);

/* Display a progress bar with percentage and optional message
 * @param percent Percentage complete (0-100)
 * @param message Additional message to display (speed, etc.), can be NULL
 * @param prefix Text to display before the progress bar (e.g. "Downloading: ")
 */
void display_progress_bar(int percent, const char *message, const char *prefix);

/**
 * @brief Displays a descriptive error message based on the download error code
 *
 * @param error_code The error code returned by DownloadHTTPFile or other functions
 */
void ad_print_download_error(int error_code);

#endif /* DISPLAY_MESSAGE_H */