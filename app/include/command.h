#ifndef __COMMAND_H_
#define __COMMAND_H_

extern void OnUartReceivedData(UART_Callback_Param_t param);
extern void OnSMSReceived(uint8_t *number, uint8_t *cmdline);
extern void OnCallReceived(uint8_t *number);
extern void send_status_info(bool sms);
extern void command_init(void);

#endif /* __COMMAND_H_ */
