#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <api_os.h>
#include <api_hal_uart.h>
#include <api_sms.h>
#include <api_ss.h>
#include <api_charset.h>



#include "main_task.h"
#include "command.h"
#include "config.h"
#include "timer.h"
#include "crc32.h"

#define CMD_ARG_MAX        5
#define CMDLINE_SIZE       80
#define REPLY_BUFFER_SIZE  160
#define USSD_BALANCE_CMD   "*100#"

/* Configs */

UART_Config_t uart2_config = {
    .baudRate = UART_BAUD_RATE_9600,
    .dataBits = UART_DATA_BITS_8,
    .stopBits = UART_STOP_BITS_1,
    .parity   = UART_PARITY_NONE,
    .rxCallback = OnUartReceivedData,
    .useEvent = false,
};

SMS_Parameter_t smsParam = {
    .fo = 17,
    .vp = 167,
    .pid = 0,
    .dcs = 8,  // 0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
};


/* Handlers */

void read_handler(uint8_t *entry_no_str, uint8_t *reply_buffer)
{
    int entry_no = -1;

    if(entry_no_str) {
        entry_no = atoi(entry_no_str);
    }

    if(entry_no >= 0 && entry_no < VALUES_COUNT) {
        if(strlen(master.number[entry_no]) > 0) {
            snprintf(reply_buffer, REPLY_BUFFER_SIZE, "DB entry[%d] = %s", entry_no, master.number[entry_no]);
        } else {
            snprintf(reply_buffer, REPLY_BUFFER_SIZE, "DB entry[%d] is empty", entry_no);
        }
    } else {
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, "Invalid entry number");
    }

}

void write_handler(uint8_t *entry_no_str, uint8_t *entry_str, uint8_t *reply_buffer)
{
    int entry_no = -1, entry_len = -1;

    if(entry_no_str) {
        entry_no = atoi(entry_no_str);
    }

    if(entry_no >= 0 && entry_no < VALUES_COUNT) {
        if(entry_str) {
            entry_len = strlen(entry_str);
        }

        if(entry_len > 0 && entry_len < VALUE_SIZE) {
            if(strlen(master.number[entry_no]) == 0) {
                master.entries_count++;
            }
            strcpy(master.number[entry_no], entry_str);
            master.crc32 = 0;
            crc32((uint8_t*)master.number, sizeof(master.number), &master.crc32);
            config_save();
            snprintf(reply_buffer, REPLY_BUFFER_SIZE, "DB entry[%d] = %s", entry_no, entry_str);
        } else {
            snprintf(reply_buffer, REPLY_BUFFER_SIZE, "Invalid phone number");
        }
    } else {
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, "Invalid entry number");
    }
}

void clear_handler(uint8_t *entry_no_str, uint8_t *reply_buffer)
{
    int entry_no = -1;

    if(entry_no_str) {
        entry_no = atoi(entry_no_str);
    }

    if(entry_no >= 0 && entry_no < VALUES_COUNT) {
        if(strlen(master.number[entry_no]) > 0) {
            master.entries_count--;
        }
        memset(master.number[entry_no], 0x00, VALUE_SIZE);
        master.crc32 = 0;
        crc32((uint8_t*)master.number, sizeof(master.number), &master.crc32);
        config_save();
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, "DB entry[%d]", entry_no);
    } else {
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, "Invalid entry number");
    }
}

void search_handler(uint8_t *entry_str, uint8_t *reply_buffer)
{
    int entry_no = -1, entry_len = -1;


    if(entry_str) {
        entry_len = strlen(entry_str);
    }

    if(entry_len > 0 && entry_len < VALUE_SIZE) {
        entry_no = is_number_exists(entry_str);
        if(entry_no >= 0) {
            snprintf(reply_buffer, REPLY_BUFFER_SIZE, "DB entry[%d] = %s", entry_no, entry_str);
        } else {
            snprintf(reply_buffer, REPLY_BUFFER_SIZE, "%s not found", entry_str);
        }
    } else {
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, "Invalid phone number");
    }
}

void info_handler(uint8_t *reply_buffer)
{
    static bool first = true;
    uint8_t uptime_str[80] = "";
    uint32_t seconds = uptime(), days = 0, hours = 0, minutes = 0;

    if((days = seconds / 86400) > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%d day(s)", days);
    } else if((hours = seconds / 3600) > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%d hour(s)", hours);
    } else if((minutes = seconds / 60) > 0) {
        snprintf(uptime_str, sizeof(uptime_str), "%d minute(s)", minutes);
    } else {
        snprintf(uptime_str, sizeof(uptime_str), "%d second(s)", seconds);
    }

    if(first) {
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, 
                "Boot cause: %d\r\nUptime: %s\r\nCSQ: %d\r\nDatabase: %d/%d (%s)", 
                boot_cause, uptime_str, CSQ, master.entries_count, VALUES_COUNT, master.is_valid ? "fine" : "currupded");
        first = false;
    } else {
        snprintf(reply_buffer, REPLY_BUFFER_SIZE, 
                "Uptime: %s\r\nCSQ: %d\r\nDatabase: %d entries (%s)", 
                uptime_str, CSQ, master.entries_count, master.is_valid ? "fine" : "currupded");
    }

    if(strlen(balance_str) > 0) {
        strcat(reply_buffer, "\r\nBalance: ");
        strcat(reply_buffer, balance_str);
        strcat(reply_buffer, "₽");
    }
}

/* Commands engine */

void parse_command(void(*reply_callback)(uint8_t *), uint8_t *cmdline)
{
    int argc = 0;
    uint8_t *argv[CMD_ARG_MAX];
    uint8_t *token;
    char cmd = '\0';
    uint8_t reply_buffer[REPLY_BUFFER_SIZE] = "";

    memset(argv, 0x00, sizeof(argv));

    if ((token = strsep((char **)(&cmdline), "#")) != NULL) {
        cmd = token[0];
        while((token = strsep((char **)(&cmdline), "#")) != NULL) {
            argv[argc++] = token;
            if(argc >= CMD_ARG_MAX) // don't process excess params 
                break; 
        }
    }

    switch(cmd) {
        case 'r':
        case 'R':
            read_handler(argv[0], reply_buffer);
            break;
        
        case 'w':
        case 'W':
            write_handler(argv[0], argv[1], reply_buffer);
            break;

        case 'c':
        case 'C':
            clear_handler(argv[0], reply_buffer);
            break;

        case 's':
        case 'S':
            search_handler(argv[0], reply_buffer);
            break;

        case 'd':
        case 'D':
            /* default_handler(reply_buffer); */
            break;

        case 'i':
        case 'I':
            info_handler(reply_buffer);
            break;

        case '?':
        case 'h':
        case 'H':
            /* help_handler(reply_buffer); */
            break;

        default:
            break;
    }

    if(strlen(reply_buffer) > 0) {
        reply_callback(reply_buffer);
        Trace(2, "%s", reply_buffer);
    }
}

void uart_reply(uint8_t *data)
{
    strcat(data, "\r\n");
    UART_Write(UART2, data, strlen(data));
}


void OnUartReceivedData(UART_Callback_Param_t param)
{
    static char cmdline[CMDLINE_SIZE] = "";
    int i;

    /* TODO: check length */
    strncat(cmdline, param.buf, param.length);
    // Trace(1,"%s: data:%s length:%d buf:%s", __FUNCTION__, param.buf, param.length, cmdline);

    UART_Write(UART2, param.buf, param.length); // local echo

    for(i = 0; i < strlen(cmdline); i++) {
        if(cmdline[i] == '\r' || cmdline[i] == '\n') {
            cmdline[i] = '\0';
            UART_Write(UART2, "\n", 1);
            parse_command(uart_reply, cmdline);
            cmdline[0] = '\0';
        }
    }
}

void sms_reply(uint8_t *data)
{   
    uint8_t admin_number[80] = "+";
    uint8_t *unicode = NULL;
    uint32_t unicodeLen;

    if(strlen(master.number[0]) == 0) {
        return;
    }

    strcat(admin_number, master.number[0]);

    if(!SMS_LocalLanguage2Unicode(data, strlen(data), CHARSET_UTF_8, &unicode, &unicodeLen)) {
        Trace(1, "local to unicode fail!");
        return;
    }

    if(!SMS_SendMessage(admin_number, unicode, unicodeLen, SIM0)) {
        Trace(1, "sms send message fail");
    }

    OS_Free(unicode);
}

void OnSMSReceived(uint8_t *number, uint8_t *cmdline)
{
    number[11] = '\0';  //  number: 7XXXXXXXXXX`\0` <-- cut here
    if(strcmp(master.number[0], number) == 0) {
        parse_command(sms_reply, cmdline);    
    }
}

void sms_init(void)
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT, SIM0)) {
        Trace(1,"sms set format error");
        return;
    }
    
    if(!SMS_SetParameter(&smsParam, SIM0)) {
        Trace(1,"sms set parameter error");
        return;
    }

    if(!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD)) {
        Trace(1,"sms set message storage fail");
        return;
    }
}


void update_balance()
{
    int tries = 5, encodeLen;
    uint8_t buffer[50];
    USSD_Type_t ussd;

    memset(balance_str, 0x00, sizeof(balance_str));
   
    encodeLen = GSM_8BitTo7Bit(USSD_BALANCE_CMD, buffer, strlen(USSD_BALANCE_CMD));
    ussd.usdString = buffer;
    ussd.usdStringSize = encodeLen;
    ussd.option = 3;
    ussd.dcs = 0x0f;
    SS_SendUSSD(ussd);

    while(strlen(balance_str) == 0 && tries-- > 0) {
        OS_Sleep(1000);
    }
}


void send_status_info(bool sms)
{
    uint8_t cmdline[8] = "I";

    update_balance();

    if (sms) {
        parse_command(sms_reply, cmdline);    
    } else {
        parse_command(uart_reply, cmdline);    
    }
}

void command_init(void)
{
    UART_Init(UART2, uart2_config); 
    sms_init();
}

