#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>

#include "timer_shared.h"

/* Global timer base that will be shared across all files */
struct Device *TimerBase = NULL;

/* Private message port and I/O request */
static struct MsgPort *TimerMP = NULL;
static struct timerequest *TimerIO = NULL;

/**
 * Initialize timer device
 *
 * @return TRUE if successful, FALSE otherwise
 */
BOOL InitTimer(void)
{
    /* Create the message port for timer I/O */
    if ((TimerMP = CreateMsgPort()) != NULL)
    {
        /* Create the timer I/O request */
        if ((TimerIO = (struct timerequest *)CreateIORequest(TimerMP, sizeof(struct timerequest))) != NULL)
        {
            /* Open the timer device */
            if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)TimerIO, 0) == 0)
            {
                /* TimerBase is automatically set up by OpenDevice and proto/timer.h */
                TimerBase = TimerIO->tr_node.io_Device;
                return TRUE;
            }
            /* Failed to open device */
            DeleteIORequest((struct IORequest *)TimerIO);
            TimerIO = NULL;
        }
        /* Failed to create I/O request */
        DeleteMsgPort(TimerMP);
        TimerMP = NULL;
    }

    return FALSE;
}

/**
 * Clean up timer device resources
 */
void CleanupTimer(void)
{
    if (TimerIO)
    {
        CloseDevice((struct IORequest *)TimerIO);
        DeleteIORequest((struct IORequest *)TimerIO);
        TimerIO = NULL;
    }

    if (TimerMP)
    {
        DeleteMsgPort(TimerMP);
        TimerMP = NULL;
    }

    TimerBase = NULL;
}