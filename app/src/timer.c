#include <api_os.h>
#include <api_debug.h>
#include <api_hal_watchdog.h>

#include <time.h>


#include "main_task.h"
#include "command.h"
#include "timer.h"

static uint32_t boot_time = 0;
static uint32_t info_sms_time = 0;
static HANDLE timerTaskHandle = NULL;

uint32_t uptime(void) {
    if(boot_time == 0) {
        return 0;
    }

    return TIME_GetTime() - boot_time;
}

static void timer_task(void* pData)
{

    WatchDog_Open(WATCHDOG_SECOND_TO_TICK(300));

    while(!time_sync_flag) {
        OS_Sleep(1000);
    }

    boot_time = TIME_GetTime();

    while(1) {
        /* Once a week send SMS with an status info */
        if(TIME_GetTime() - info_sms_time > (604800)) {  
            info_sms_time = TIME_GetTime();
            send_status_info(true);
        }

        WatchDog_KeepAlive();

        OS_Sleep(60000);
    }
}


void timer_init(void) {
    timerTaskHandle = OS_CreateTask(timer_task,
                                    NULL, NULL, TIMER_TASK_STACK_SIZE, TIMER_TASK_PRIORITY, 0, 0, TIMER_TASK_NAME);
}
