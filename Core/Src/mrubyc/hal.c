#include <stdio.h>
#include "rrt0.h"
#include "hal.h"
#include "main.h"

void hal_init(void){
}

void hal_enable_irq(void){
	__enable_irq();
}

void hal_disable_irq(void){
	__disable_irq();
}

void hal_idle_cpu(void){
	/*HAL_Delay(MRBC_TICK_UNIT);
	mrbc_tick();*/
	HAL_SuspendTick();
	HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
}

extern UART_HandleTypeDef huart2;

int hal_write(int fd, const void *buf, int nbytes){
	HAL_UART_Transmit(&huart2, buf, nbytes, -1);
	return nbytes;
}



