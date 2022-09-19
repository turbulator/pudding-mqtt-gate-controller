#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <api_os.h>
#include <api_event.h>
#include <api_call.h>
#include <api_debug.h>
#include <api_sms.h>
#include <api_hal_gpio.h>
#include <api_hal_pm.h>
#include <api_charset.h>


#include "time.h"

#include "main_task.h"
#include "timer.h"
#include "config.h"
#include "command.h"


#define MAIN_TASK_STACK_SIZE    (2048 * 4)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "Main Task"

static HANDLE mainTaskHandle = NULL;

bool time_sync_flag = false;
uint8_t boot_cause = 0xFF;
uint8_t CSQ = 0xFF;
uint8_t balance_str[80] = "";

GPIO_config_t switch_gpio_config = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN0,
    .defaultLevel = GPIO_LEVEL_HIGH
};


void OnCallReceived(uint8_t *number)
{
    if(is_number_exists(number) >= 0) {
        GPIO_SetLevel(switch_gpio_config, GPIO_LEVEL_LOW);
        OS_Sleep(1000);
        GPIO_SetLevel(switch_gpio_config, GPIO_LEVEL_HIGH);
        OS_Sleep(3000);
        CALL_Answer();
        OS_Sleep(2000);
        CALL_HangUp();
    }
}



void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id) {
        case API_EVENT_ID_POWER_ON:
            Trace(1,"Power on, cause:0x%02x", pEvent->param1);
            boot_cause = pEvent->param1;
            break;

        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(2,"Network register success");
            break;

       case API_EVENT_ID_SIGNAL_QUALITY:
            Trace(2,"CSQ: %d", pEvent->param1);
            CSQ = pEvent->param1;
            break;

        case API_EVENT_ID_CALL_INCOMING:   //param1: phone number type, pParam1: phone number
            Trace(2,"Receive a call, number:%s, number type:%d", pEvent->pParam1, pEvent->param1);
            OnCallReceived(pEvent->pParam1);
            break;

        case API_EVENT_ID_CALL_ANSWER:  
            Trace(2,"Answer success");
            break;

        case API_EVENT_ID_SMS_RECEIVED:
            Trace(2,"Received message");
            // Trace(2,"message header:%s", pEvent->pParam1);
            if(pEvent->param1 == SMS_ENCODE_TYPE_ASCII) {
                OnSMSReceived(pEvent->pParam1 + 2, pEvent->pParam2);
            }
            break;

        case API_EVENT_ID_SMS_SENT:
            Trace(2, "Send message success");
            break;

        case API_EVENT_ID_SMS_ERROR:
            Trace(10,"SMS error occured! cause:%d", pEvent->param1);
            break;


        case API_EVENT_ID_SMS_LIST_MESSAGE:
            {
                SMS_Message_Info_t* messageInfo = (SMS_Message_Info_t*)pEvent->pParam1;
                Trace(1,"message header index:%d, status:%d, number type:%d, number:%s, time:\"%u/%02u/%02u, %02u:%02u:%02u+%02d\"", messageInfo->index, messageInfo->status,
                                                                                            messageInfo->phoneNumberType, messageInfo->phoneNumber,
                                                                                            messageInfo->time.year, messageInfo->time.month, messageInfo->time.day,
                                                                                            messageInfo->time.hour, messageInfo->time.minute, messageInfo->time.second,
                                                                                            messageInfo->time.timeZone);
                //need to free data here
                SMS_DeleteMessage(messageInfo->index, messageInfo->status, SMS_STORAGE_SIM_CARD);

                OS_Free(messageInfo->data);
                break;
            }

        case API_EVENT_ID_NETWORK_GOT_TIME:
            time_sync_flag = true;
            break;

        case API_EVENT_ID_USSD_SEND_SUCCESS:
            {
                Trace(1,"ussd execute success");
                uint8_t *buffer;
                uint32_t i, j, bufferLen;
                USSD_Type_t* result = (USSD_Type_t*)pEvent->pParam1;
                Unicode2LocalLanguage(result->usdString, result->usdStringSize, CHARSET_UTF_8, &buffer, &bufferLen);
                bufferLen = strlen(buffer);
                for(j = i = 0; i < bufferLen; i++) {
                    //Trace(1,"%02X", buffer[i]);
                    if(buffer[i] >= '0' && buffer[i] <= '9') {
                        balance_str[j++] = buffer[i];
                    } else if (buffer[i] == '.' || buffer[i] == ',' || buffer[i] == '\n' || buffer[i] == '\r') {
                        break;
                    }
                }
                balance_str[j++] = '\0';
                Trace(1,"balance: %s", balance_str);
                OS_Free(buffer);
                break;
            }

        case API_EVENT_ID_USSD_SEND_FAIL:
            Trace(1,"ussd exec fail, error code:%x,%d",pEvent->param1, pEvent->param2);
            break;

        default:
            break;
    }
}


/**
 * The Main task
 *
 * Init HW and run MQTT task. 
 *
 * @param  pData Parameter passed in when this function is called
 */
void app_MainTask(void *pData)
{
    API_Event_t* event = NULL;

    Trace(1, "Main Task started");

    /* HW init */
    TIME_SetIsAutoUpdateRtcTime(true);       // Sync time from GSM/GPRS network when attach success
    PM_PowerEnable(POWER_TYPE_VPAD, true);   // GPIO0  ~ GPIO7  and GPIO25 ~ GPIO36    2.8V
    PM_SetSysMinFreq(PM_SYS_FREQ_312M);      // Rigth freq for propper UART baud rate
    GPIO_Init(switch_gpio_config);           // GPIO for switch

    /* Services init */
    timer_init();
    config_init();
    command_init();


    // Wait and process system events
    while(1) {
        if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER)) {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

/**
 * The entry point of application. Just create the main task.
 */
void app_Main(void)
{
    mainTaskHandle = OS_CreateTask(app_MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}
