/* console_output.h - Conditional Console Output
 *
 * Macros for console output that can be conditionally compiled.
 * When ENABLE_CONSOLE is defined a console window opens and printf fires.
 * When not defined all macros become no-ops - no console on Workbench.
 *
 * Build:
 *   make CONSOLE=1   - enable console output (debug)
 *   make             - release build (no console)
 */

#ifndef CONSOLE_OUTPUT_H
#define CONSOLE_OUTPUT_H

#include <stdio.h>

#ifdef ENABLE_CONSOLE

    #define CONSOLE_PRINTF(...)   printf(__VA_ARGS__)
    #define CONSOLE_ERROR(...)    printf(__VA_ARGS__)
    #define CONSOLE_WARNING(...)  printf(__VA_ARGS__)
    #define CONSOLE_STATUS(...)   printf(__VA_ARGS__)
    #define CONSOLE_BANNER(...)   printf(__VA_ARGS__)
    #define CONSOLE_NEWLINE()     printf("\n")
    #define CONSOLE_SEPARATOR()   printf("===============================================\n")

    #ifdef DEBUG
        #define CONSOLE_DEBUG(...) printf(__VA_ARGS__)
    #else
        #define CONSOLE_DEBUG(...) ((void)0)
    #endif

#else

    #define CONSOLE_PRINTF(...)   do {} while (0)
    #define CONSOLE_ERROR(...)    do {} while (0)
    #define CONSOLE_WARNING(...)  do {} while (0)
    #define CONSOLE_STATUS(...)   do {} while (0)
    #define CONSOLE_BANNER(...)   do {} while (0)
    #define CONSOLE_NEWLINE()     do {} while (0)
    #define CONSOLE_SEPARATOR()   do {} while (0)
    #define CONSOLE_DEBUG(...)    do {} while (0)

#endif /* ENABLE_CONSOLE */

#endif /* CONSOLE_OUTPUT_H */
