#ifndef INC_MRUBYC_HAL_H_
#define INC_MRUBYC_HAL_H_


#define MRBC_TICK_UNIT 10    // 10 [ms]
#define MRBC_TIMESLICE_TICK_COUNT 10

void hal_init(void);
void hal_enable_irq(void);
void hal_disable_irq(void);
void hal_idle_cpu(void);

int hal_write(int fd, const void *buf, int nbytes);
int hal_flush(int fd);
void hal_abort(const char *s);
void alarm_init();


#endif /* INC_MRUBYC_HAL_H_ */

