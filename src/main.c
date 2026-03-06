/* main.c - amiga-base skeleton
 *
 * Minimal ReAction application. Opens a single window with a Close button,
 * initialises logging and optional memory tracking, runs the event loop,
 * and exits cleanly.
 *
 * Replace AMIGA_APP_NAME in the Makefile and build on top of this.
 *
 * Build:
 *   make                 - release (no console, no memtrack)
 *   make CONSOLE=1       - enable printf console window
 *   make MEMTRACK=1      - enable allocation tracking
 *   make CONSOLE=1 MEMTRACK=1
 */

/* AmigaOS stack size - increase if your application needs more */
long __stack = 65536L;

#include "platform/platform.h"
#include "platform/amiga_headers.h"
#include "console_output.h"
#include "log/log.h"

/* ReAction classes */
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>          /* Required for BOOPSI DoMethodA() */
#include <reaction/reaction_macros.h>  /* RA_OpenWindow / RA_HandleInput macros */

/*---------------------------------------------------------------------------*/
/* Gadget IDs                                                                */
/*---------------------------------------------------------------------------*/

#define GID_CLOSE 1

/*---------------------------------------------------------------------------*/
/* Library / Class Bases                                                     */
/*---------------------------------------------------------------------------*/

struct Library *WindowBase  = NULL;
struct Library *LayoutBase  = NULL;
struct Library *ButtonBase  = NULL;

static Class *WindowClass  = NULL;
static Class *LayoutClass  = NULL;
static Class *ButtonClass  = NULL;

/*---------------------------------------------------------------------------*/
/* Open / close ReAction classes                                             */
/*---------------------------------------------------------------------------*/

static BOOL open_classes(void) {
    WindowBase = OpenLibrary("window.class",         0);
    LayoutBase = OpenLibrary("gadgets/layout.gadget", 0);
    ButtonBase = OpenLibrary("gadgets/button.gadget", 0);

    if (!WindowBase || !LayoutBase || !ButtonBase) {
        log_error(LOG_GENERAL, "Failed to open required ReAction classes\n");
        return FALSE;
    }

    WindowClass = WINDOW_GetClass();
    LayoutClass = LAYOUT_GetClass();
    ButtonClass = BUTTON_GetClass();

    if (!WindowClass || !LayoutClass || !ButtonClass) {
        log_error(LOG_GENERAL, "Failed to obtain ReAction class pointers\n");
        return FALSE;
    }

    log_info(LOG_GENERAL, "ReAction classes opened OK\n");
    return TRUE;
}

static void close_classes(void) {
    if (ButtonBase) { CloseLibrary(ButtonBase); ButtonBase = NULL; }
    if (LayoutBase) { CloseLibrary(LayoutBase); LayoutBase = NULL; }
    if (WindowBase) { CloseLibrary(WindowBase); WindowBase = NULL; }
}

/*---------------------------------------------------------------------------*/
/* Main Window                                                               */
/*---------------------------------------------------------------------------*/

static BOOL run_main_window(void) {
    Object *window_obj  = NULL;
    Object *layout_obj  = NULL;
    Object *close_btn   = NULL;

    struct Window *window = NULL;
    ULONG signal_mask    = 0;
    ULONG signals;
    ULONG result;
    UWORD code;
    BOOL  done = FALSE;
    BOOL  ok   = FALSE;

    /* --- Build gadget hierarchy --- */

    close_btn = NewObject(ButtonClass, NULL,
        GA_ID,         GID_CLOSE,
        GA_Text,       "Close",
        GA_RelVerify,  TRUE,
        TAG_DONE);

    if (!close_btn) {
        log_error(LOG_GENERAL, "Failed to create Close button\n");
        return FALSE;
    }

    layout_obj = NewObject(LayoutClass, NULL,
        LAYOUT_Orientation,  LAYOUT_ORIENT_VERT,
        LAYOUT_SpaceOuter,   TRUE,
        LAYOUT_SpaceInner,   TRUE,
        LAYOUT_AddChild,     close_btn,
        TAG_DONE);

    if (!layout_obj) {
        log_error(LOG_GENERAL, "Failed to create layout gadget\n");
        DisposeObject(close_btn);
        return FALSE;
    }

    /* --- Build window object --- */

    window_obj = NewObject(WindowClass, NULL,
        WA_Title,          AMIGA_APP_NAME,
        WA_Activate,       TRUE,
        WA_DepthGadget,    TRUE,
        WA_DragBar,        TRUE,
        WA_CloseGadget,    TRUE,
        WA_SizeGadget,     TRUE,
        WA_Width,          320,
        WA_Height,         80,
        WINDOW_Position,   WPOS_CENTERSCREEN,
        WINDOW_Layout,     layout_obj,
        TAG_DONE);

    if (!window_obj) {
        log_error(LOG_GENERAL, "Failed to create window object\n");
        DisposeObject(layout_obj);   /* also disposes close_btn */
        return FALSE;
    }

    /* --- Open the window --- */

    window = (struct Window *)RA_OpenWindow(window_obj);
    if (!window) {
        log_error(LOG_GENERAL, "RA_OpenWindow() failed\n");
        DisposeObject(window_obj);
        return FALSE;
    }

    GetAttr(WINDOW_SigMask, window_obj, &signal_mask);
    log_info(LOG_GENERAL, "Window open - entering event loop\n");

    /* --- Event loop --- */

    while (!done) {
        signals = Wait(signal_mask | SIGBREAKF_CTRL_C);

        if (signals & SIGBREAKF_CTRL_C) {
            log_info(LOG_GENERAL, "Ctrl-C received - exiting\n");
            done = TRUE;
            continue;
        }

        while ((result = RA_HandleInput(window_obj, &code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
                case WMHI_CLOSEWINDOW:
                    log_info(LOG_GENERAL, "Close gadget clicked\n");
                    done = TRUE;
                    break;

                case WMHI_GADGETUP:
                    switch (result & WMHI_GADGETMASK) {
                        case GID_CLOSE:
                            log_info(LOG_GENERAL, "Close button pressed\n");
                            done = TRUE;
                            break;
                    }
                    break;

                default:
                    break;
            }
        }
    }

    ok = TRUE;

    /* --- Cleanup: dispose top-level object (children disposed automatically) --- */
    DisposeObject(window_obj);
    log_info(LOG_GENERAL, "Window closed and disposed\n");

    return ok;
}

/*---------------------------------------------------------------------------*/
/* Entry Point                                                               */
/*---------------------------------------------------------------------------*/

int main(void) {
    int exitCode = RETURN_OK;

    /* Initialise memory tracking (no-op when MEMTRACK=0) */
    amiga_memory_init();

    /* Start logging */
    initialize_log_system(FALSE);

    CONSOLE_BANNER("=== " AMIGA_APP_NAME " starting ===\n");
    log_info(LOG_GENERAL, AMIGA_APP_NAME " starting\n");

    /* Open ReAction classes */
    if (!open_classes()) {
        exitCode = RETURN_FAIL;
        goto cleanup;
    }

    /* Run the main window */
    if (!run_main_window()) {
        exitCode = RETURN_FAIL;
    }

cleanup:
    close_classes();

    /* Memory leak report (no-op when MEMTRACK=0) */
    amiga_memory_report();

    log_info(LOG_GENERAL, AMIGA_APP_NAME " exiting with code %d\n", exitCode);
    shutdown_log_system();

    CONSOLE_BANNER("=== " AMIGA_APP_NAME " done ===\n");
    return exitCode;
}
