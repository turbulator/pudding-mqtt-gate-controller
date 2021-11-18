#ifndef __TIMER_H_
#define __TIMER_H_

#define TIMER_TASK_STACK_SIZE       (2048 * 4)
#define TIMER_TASK_PRIORITY         1
#define TIMER_TASK_NAME             "Timer Task"

extern uint32_t uptime(void);
extern void timer_init(void);

#endif /* __TIMER_H_ */
