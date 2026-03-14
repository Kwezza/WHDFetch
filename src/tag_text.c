/*
 * Amiga CLI Tagged Text Formatter
 * ====================
 * 
 * This module provides a text formatting system that supports HTML-like tags
 * for text styling and layout control. The formatter processes text with
 * embedded tags and outputs formatted text to the console with proper
 * word wrapping, indentation, and styling.
 * 
 * Requires at least a Commodore Amiga workbench 2.04 or later.  Compiled using
 * SAS/C on the Amiga.  C89 standard (mostly).
 *
 * Author: Kwezza
 * Date: 2025-04-07
 * Version: 1.0
 *
 * Tag Syntax
 * ---------
 * Tags are enclosed in angle brackets < > and come in pairs (start/end) or
 * as single tags. All tags are case-sensitive.
 * 
 * To output a literal < character, use << as an escape sequence.
 * Example: "a<<b" will output "a<b"
 * 
 * Text Styling Tags:
 *   <b>text</b>      - Bold text
 *   <i>text</i>      - Italic text
 *   <u>text</u>      - Underlined text
 *   <r>text</r>      - Reverse/inverted text
 *   <ut>             - Underline current line with tildes (~).  Needs to be placed at the end of the line as it will print a newline.
 * 
 * Color Tags:
 *   <cw>text</cw>    - White text
 *   <cb>text</cb>    - Blue text
 *   <cg>text</cg>    - Grey text
 *   <c>text</c>      - Black text
 * 
 * Layout Tags:
 *   <exNN>text</ex>  - Indent text by NN spaces (e.g. <ex08> for 8 spaces)
 *   <exNNd>text</ex> - Indent text by NN dots (e.g. <ex08d> for 8 dots)
 *                     NN must be a two-digit number (01-99)
 *   <jf>text</jf>    - Justify text
 * 
 * Usage Example:
 * -------------
 * const char *text[] = {
 *     "<b>Title</b><ut>",
 *     "start<ex08>This is indented by 8 spaces</ex>",
 *     "Normal text with <i>italic</i> and <b>bold</b> words.",
 *     "<ex10d>This is indented with 10 dots</ex>",
 *     NULL
 * };
 * print_tagged_text(text);
 * 
 * Notes:
 * -----
 * - Tags can be nested but should be closed in reverse order
 * - Text will automatically wrap to fit the console width
 * - Indentation values should be positive and less than console width
 * - Long text will prompt for key press between pages
 * - TextBuilder automatically grows as needed
 * - Remember to call free_text_builder() when done
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <exec/memory.h>
#include <stdarg.h>

#include "cli_utilities.h"
#include "tag_text.h"
#include "platform/platform.h"

#define textWhite "\x1B[32m"
#define textBlack "\x1B[31m"
#define textBlue "\x1B[33m"
#define textGrey "\x1B[30m"
#define textBold "\x1B[1m"
#define textBoldOff "\x1B[22m"
#define textItalic "\x1B[3m"
#define textItalicOff "\x1B[23m"
#define textReverse "\x1B[7m"
#define textReverseOff "\x1B[27m"
#define textUnderline "\x1B[4m"
#define textUnderlineOff "\x1B[24m"
#define textReset "\x1B[0m"

#define MAX_LINE_LEN 1024  /* Max formatted line length */

extern ConsoleSize getConsoleSize();

void justify_line(const char *text, int width, int indentVal);
int count_spaces_to_justify(const char *str, int indent);
void wait_char(void);

char *pad_number_with_dots(int number, int total_length)
{
    char num_str[32];
    char *result;
    int num_len, dots_needed;

    // Convert number to string
    sprintf(num_str, "%d", number);
    num_len = strlen(num_str);

    // Calculate how many dots we need
    dots_needed = total_length - num_len;
    if (dots_needed < 0)
        dots_needed = 0;

    // Allocate memory for result (including null terminator)
    result = amiga_malloc(dots_needed + num_len + 1);
    if (!result)
        return NULL;

    // Fill with dots
    memset(result, '.', dots_needed);

    // Append the number
    strcat(result, num_str);

    return result;
}

void print_tagged_text(const char **tagged_text_array)
{
    // Console and display variables
    ConsoleSize cs;
    int columns, rows;
    int line_counter;
    char buffer[1024];        // Buffer to build each line of text
    int buf_len;             // Current length of buffer
    int current_col;         // Current column position
    int indented;            // Flag indicating if text should be indented
    int indent = 8;          // Number of columns to indent by
    int pending_spaces;      // Number of spaces waiting to be added
    const char *p;           // Pointer to current position in text
    char tag[20];           // Buffer for tag names
    char word[100];         // Buffer for collecting words
    int i, j, len, spaces, word_len, spaces_needed, justified;
    char *end;
    int text_idx;           // Index for tagged_text_array

    /* Initialize console size and variables */
    cs = getConsoleSize();
    cs.rows = cs.rows - 1;  // Reserve one row for "Press any key" message
    columns = cs.columns;
    rows = cs.rows;
    if (columns < indent)
        indent = 0;         // Disable indentation if console is too narrow
    line_counter = 0;

    /* Process each string in the tagged_text_array */
    for (text_idx = 0; tagged_text_array[text_idx] != NULL; text_idx++)
    {
        p = tagged_text_array[text_idx];  // Get current string
        buf_len = 0;
        buffer[0] = '\0';
        current_col = 0;
        indented = 0;
        pending_spaces = 0;
        justified = 0;

        /* Handle empty lines */
        if (strcmp(p, "") == 0)
        {
            printf("\n");
            line_counter++;
            if (line_counter >= rows)
            {
                printf(textReverse textItalic "Press any key to continue..." textItalicOff textReverseOff " ");
                fflush(stdout);
                wait_char();
                printf("\r                            \r");
                line_counter = 0;
            }
            continue;
        }
        else
        {
            /* Process the string character by character */
            while (*p)
            {
                /* Handle HTML-like tags */
                if (*p == '<')
                {
                    /* Check for << escape sequence first */
                    if (*(p+1) == '<')
                    {
                        if (buf_len < 1023)
                        {
                            buffer[buf_len++] = '<';  /* Add single < to output */
                            buffer[buf_len] = '\0';
                            current_col++;
                        }
                        p += 2;  /* Skip both < characters */
                        continue;
                    }

                    end = strchr(p, '>');
                    if (end)
                    {
                        len = end - p + 1;
                        if (len < 20)
                        {
                            strncpy(tag, p, len);
                            tag[len] = '\0';
                            p = end + 1;

                            if (strncmp(tag, "<ex", 3) == 0) /* Indent text from this point on.  Word wrap (if needed) will be done to this column until the text ends.*/
                            {
                                int use_dots = 0;
                                int tag_len = len;

                                /* Check if tag ends with 'd' */
                                if (tag[tag_len-2] == 'd' && tag[tag_len-1] == '>') {
                                    use_dots = 1;
                                    //tag_len -= 2; /* Adjust length to exclude 'd>' */
                                }

                                //printf("tag3: %c tag4: %c tag_len: %d\n", tag[3], tag[4], tag_len   );
                                /* Always require two digits after <ex */
                                if (tag_len >= 6 && isdigit(tag[3]) && isdigit(tag[4]) )
                                {
                                    /* Convert two digits to number */
                                    int new_indent = (tag[3] - '0') * 10 + (tag[4] - '0');
                                    if (new_indent > 0 && new_indent < columns)
                                    {
                                        indent = new_indent;
                                    }
                                }
                                else
                                {
                                    /* Invalid format - use default indent */
                                    indent = 8;
                                }

                                //printf("Indent: %d\n", indent); 

                                if (current_col < indent)
                                {
                                    spaces_needed = indent - current_col;
                                    for (i = 0; i < spaces_needed; i++)
                                    {
                                        if (buf_len < 1023)
                                            buffer[buf_len++] = use_dots ? '.' : ' ';
                                    }
                                    buffer[buf_len] = '\0';
                                    current_col += spaces_needed;
                                }
                                indented = 1;
                            }
                            else if (strcmp(tag, "</ex>") == 0)  // Disable indentation
                            {
                                indented = 0;
                                /* Print the current buffer content */
                                printf("%s", buffer);
                                /* Reset buffer for next line */
                                buf_len = 0;
                                buffer[0] = '\0';
                                current_col = 0;
                            }
                            else if (strcmp(tag, "<jf>") == 0)  // Enable justification
                            {
                                justified = 1;
                            }
                            else if (strcmp(tag, "<b>") == 0)  /* Bold */
                            {
                                strcpy(buffer + buf_len, textBold);
                                buf_len += strlen(textBold);
                            }
                            else if (strcmp(tag, "</b>") == 0)  /* Bold Off */
                            {
                                strcpy(buffer + buf_len, textBoldOff);
                                buf_len += strlen(textBoldOff);
                            }
                            else if (strcmp(tag, "<i>") == 0)  /* Italic */
                            {
                                strcpy(buffer + buf_len, textItalic);
                                buf_len += strlen(textItalic);
                            }
                            else if (strcmp(tag, "</i>") == 0)  /* Italic Off */
                            {
                                strcpy(buffer + buf_len, textItalicOff);
                                buf_len += strlen(textItalicOff);
                            }
                            else if (strcmp(tag, "<u>") == 0)  /* Underline */
                            {
                                strcpy(buffer + buf_len, textUnderline);
                                buf_len += strlen(textUnderline);
                            }
                            else if (strcmp(tag, "</u>") == 0)  /* Underline Off */
                            {
                                strcpy(buffer + buf_len, textUnderlineOff);
                                buf_len += strlen(textUnderlineOff);
                            }
                            else if (strcmp(tag, "<r>") == 0)  /* Reverse (inverted) text*/
                            {
                                strcpy(buffer + buf_len, textReverse);
                                buf_len += strlen(textReverse);
                            }
                            else if (strcmp(tag, "</r>") == 0)  /* Reverse Off */
                            {
                                strcpy(buffer + buf_len, textReverseOff);
                                buf_len += strlen(textReverseOff);
                            }
                            else if (strcmp(tag, "<cw>") == 0)  /* Text colour white */
                            {
                                strcpy(buffer + buf_len, textWhite);
                                buf_len += strlen(textWhite);
                            }
                            else if (strcmp(tag, "<cb>") == 0)  /* Text colour blue */
                            {
                                strcpy(buffer + buf_len, textBlue);
                                buf_len += strlen(textBlue);
                            }
                            else if (strcmp(tag, "<cg>") == 0)  /* Text colour grey */
                            {
                                strcpy(buffer + buf_len, textGrey);
                                buf_len += strlen(textGrey);
                            }
                            else if (strcmp(tag, "<c>") == 0)  /* Text colour black */
                            {
                                strcpy(buffer + buf_len, textBlack);
                                buf_len += strlen(textBlack);
                            }
                            else if (strcmp(tag, "<ut>") == 0)  /* Underline with tildes */
                            {
                                int visible_len = 0;
                                int in_escape = 0;
                                
                                /* First print the current line */
                                printf("%s", buffer);
                                
                                /* Calculate visible length of current line (excluding ANSI codes) */
                                for (i = 0; i < buf_len; i++) {
                                    if (in_escape) {
                                        if (((buffer[i] >= 'A') && (buffer[i] <= 'Z')) ||
                                            ((buffer[i] >= 'a') && (buffer[i] <= 'z'))) {
                                            in_escape = 0;
                                        }
                                    } else {
                                        if (buffer[i] == '\033' && (i + 1 < buf_len) && buffer[i + 1] == '[') {
                                            in_escape = 1;
                                        } else if (!in_escape) {
                                            visible_len++;
                                        }
                                    }
                                }

                                /* Print newline and tildes */
                                printf("\n");
                                for (i = 0; i < visible_len; i++) {
                                    putchar('~');
                                }
                                printf("\n");
                                line_counter += 2;  /* Account for the two newlines */
                                
                                /* Reset buffer for next line */
                                buf_len = 0;
                                buffer[0] = '\0';
                                current_col = 0;
                            }
                        }
                        else
                        {
                            p = end + 1;  // Skip tags that are too long
                        }
                    }
                    else
                    {
                        p++;  // Skip if no closing '>'
                    }
                }
                /* Handle whitespace */
                else if (isspace(*p))
                {
                    spaces = 0;
                    while (*p && isspace(*p))
                    {
                        spaces++;
                        p++;
                    }
                    pending_spaces = spaces;
                }
                /* Handle words */
                else
                {
                    /* Collect word characters */
                    i = 0;
                    while (*p && !isspace(*p) && *p != '<' && i < 99)
                    {
                        word[i++] = *p++;
                    }
                    word[i] = '\0';
                    word_len = i;

                    /* Check if word fits on current line, wrap if necessary */
                    if (current_col + pending_spaces + word_len > columns)
                    {
                        if(justified)
                        {
                            justify_line(buffer, columns, indented ? indent : 0);
                        }
                        else
                        {
                            printf("%s\n", buffer);
                        }

                        line_counter++;
                        if (line_counter >= rows)
                        {
                            printf(textReverse textItalic "Press any key to continue..." textItalicOff textReverseOff " ");
                            fflush(stdout);
                            wait_char();
                            printf("\r                            \r");
                            line_counter = 0;
                        }
                        buf_len = 0;
                        buffer[0] = '\0';
                        /* Handle indentation for new line */
                        if (indented)
                        {
                            for (j = 0; j < indent; j++)
                            {
                                if (buf_len < 1023)
                                    buffer[buf_len++] = ' ';
                            }
                            buffer[buf_len] = '\0';
                            current_col = indent;
                        }
                        else
                        {
                            current_col = 0;
                        }
                        strcpy(buffer + buf_len, word);
                        buf_len += word_len;
                        current_col += word_len;
                    }
                    else
                    {
                        /* Add pending spaces and word to current line */
                        for (j = 0; j < pending_spaces; j++)
                        {
                            if (buf_len < 1023)
                                buffer[buf_len++] = ' ';
                        }
                        buffer[buf_len] = '\0';
                        current_col += pending_spaces;
                        strcpy(buffer + buf_len, word);
                        buf_len += word_len;
                        current_col += word_len;
                    }
                    pending_spaces = 0;
                }
            }
        }
        /* Output any remaining buffer content */
        if (buf_len > 0)
        {
            printf("%s\n", buffer);
            line_counter++;
            if (line_counter >= rows)
            {
                printf(textReverse textItalic "Press any key to continue..." textItalicOff textReverseOff " ");
                fflush(stdout);
                wait_char();
                printf("\r                            \r");
                line_counter = 0;
            }
        }
    }

    /* Reset formatting at the end */
    printf("%s", textReset);
}

/* Function: count_spaces_to_justify
 * ---------------------------------
 * Returns the number of spaces to add after the left-aligned text
 * (which may contain ANSI escape sequences) so that its visible
 * length becomes equal to the specified indent.
 *
 * Parameters:
 *   str    - input text (may contain ANSI codes)
 *   indent - target number of visible characters (e.g., 15)
 *
 * Returns:
 *   The number of spaces needed, or 0 if the visible text is already too long.
 */
int count_spaces_to_justify(const char *str, int indent)
{
    int visible_len = 0;
    const char *p = str;

    while (*p != '\0')
    {
        /* If we encounter an ESC character followed by '[',
           skip until we reach the terminating 'm'. */
        if (*p == '\x1B' && *(p + 1) == '[')
        {
            p += 2; /* Skip ESC and '[' */
            while (*p != '\0' && *p != 'm')
            {
                p++;
            }
            if (*p == 'm')
            {
                p++; /* Skip the 'm' */
            }
        }
        else
        {
            /* Count visible character */
            visible_len++;
            p++;
        }
    }

    /* If visible length is already greater than or equal to indent,
       no spaces should be added. */
    if (visible_len >= indent)
    {
        return 0;
    }
    else
    {
        return indent - visible_len;
    }
}

void justify_line(const char *text, int width, int indentVal)
{
    char buffer[1024];
    char justify_buffer[1024];
    int buffer_pos, justify_pos;
    int visible_len;
    int space_pos[100];
    int space_count;
    int in_escape;
    const char *p;
    int extra_spaces, base_extra, extra_remainder;
    int i, start, end;
    int indent_number_of_spaces;

    /* --- Step 1: Process Left-Aligned Portion --- */
    /* Calculate extra spaces needed to pad left text to indentVal. */
    indent_number_of_spaces = count_spaces_to_justify(text, indentVal);

    /* Copy the left portion (which may include ANSI codes) until we have indentVal visible characters. */
    visible_len = 0;
    buffer_pos = 0;
    in_escape = 0;
    p = text;
    while (*p && visible_len < indentVal)
    {
        buffer[buffer_pos] = *p;
        if (in_escape)
        {
            if (((*p >= 'A') && (*p <= 'Z')) ||
                ((*p >= 'a') && (*p <= 'z')))
            {
                in_escape = 0;
            }
        }
        else
        {
            if (*p == '\033' && *(p + 1) == '[')
            {
                in_escape = 1;
            }
            else
            {
                visible_len++;
            }
        }
        buffer_pos++;
        p++;
    }
    buffer[buffer_pos] = '\0';

    /* Output the left portion */
    printf("%s", buffer);

    /* Insert the extra spaces needed so that the visible count reaches indentVal */
    for (i = 0; i < indent_number_of_spaces; i++)
    {
        putchar(' ');
    }

    /* --- Step 2: Process the Right Portion for Justification --- */
    /* Now continue processing from where we left off */
    justify_pos = 0;
    visible_len = 0;
    space_count = 0;
    in_escape = 0;

    /* We assume the available width for justification is from indentVal to width */
    while (*p && visible_len < (width - indentVal))
    {
        justify_buffer[justify_pos] = *p;
        if (in_escape)
        {
            if (((*p >= 'A') && (*p <= 'Z')) ||
                ((*p >= 'a') && (*p <= 'z')))
            {
                in_escape = 0;
            }
        }
        else
        {
            if (*p == '\033' && *(p + 1) == '[')
            {
                in_escape = 1;
            }
            else
            {
                visible_len++;
                if (*p == ' ')
                {
                    space_pos[space_count] = justify_pos;
                    space_count++;
                }
            }
        }
        justify_pos++;
        p++;
    }
    justify_buffer[justify_pos] = '\0';

    /* If no spaces were found in the right portion, just output it as-is */
    if (space_count == 0)
    {
        printf("%s\n", justify_buffer);
        return;
    }

    /* Calculate extra spaces needed to fill to the target width. */
    extra_spaces = (width - indentVal) - visible_len;
    base_extra = extra_spaces / space_count;
    extra_remainder = extra_spaces % space_count;

    /* --- Step 3: Output the Justified Right Portion --- */
    start = 0;
    for (i = 0; i < space_count; i++)
    {
        end = space_pos[i];
        /* Print segment up to and including the space */
        printf("%.*s", end - start + 1, justify_buffer + start);
        /* Add extra spaces: distribute remainder among the first few gaps */
        {
            int spaces_to_insert = base_extra + (i < extra_remainder ? 1 : 0);
            while (spaces_to_insert > 0)
            {
                putchar(' ');
                spaces_to_insert--;
            }
        }
        start = end + 1;
    }
    /* Output the final segment */
    printf("%s\n", justify_buffer + start);
}

BOOL init_text_builder(TextBuilder *tb)
{
    tb->count = 0;
    tb->capacity = INITIAL_LINES;
    tb->lines = (char **)amiga_malloc(sizeof(char *) * tb->capacity);
    return (BOOL)(tb->lines != NULL);
}

void add_line(TextBuilder *tb, const char *line)
{
    if (tb->count >= tb->capacity) {
        int new_cap = tb->capacity * 2;
        char **new_lines = (char **)amiga_malloc(sizeof(char *) * new_cap);
        if (!new_lines) return;
        memcpy(new_lines, tb->lines, sizeof(char *) * tb->count);
        amiga_free(tb->lines);
        tb->lines = new_lines;
        tb->capacity = new_cap;
    }
    tb->lines[tb->count++] = (char *)line; // cast ok since you're pointing to constant strings
}

void finalize_text_builder(TextBuilder *tb)
{
    add_line(tb, NULL); // NULL-terminate
}

void free_text_builder(TextBuilder *tb)
{
    amiga_free(tb->lines);
}

BOOL addf_line(TextBuilder *tb, const char *fmt, ...)
{
    va_list args;
    char buffer[MAX_LINE_LEN];
    char *copy;

    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    copy = (char *)amiga_malloc(strlen(buffer) + 1);
    if (!copy)
        return FALSE;

    strcpy(copy, buffer);
    add_line(tb, copy);
    return TRUE;
}