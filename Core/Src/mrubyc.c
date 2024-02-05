#include <stdio.h>
#include "mrubyc.h"
#include "vm.h"
#include "rrt0.h"
#include "main.h"
#include "class.h"

extern const uint8_t mrbbuf[];

#define MRBC_MEMORY_SIZE (1024*30)
static uint8_t memory_pool[MRBC_MEMORY_SIZE];

static void c_object_led(mrb_vm *vm, mrb_value v[], int argc)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, v[1].i);
}


void start_mrubyc(void){
	mrbc_init(memory_pool, MRBC_MEMORY_SIZE);

	mrbc_class *cls = mrbc_get_class_by_name("Object");  // 追加
	mrbc_define_method(0, cls, "led", c_object_led);     // 追加

	if( mrbc_create_task(mrbbuf, 0) != NULL ){
		mrbc_run();
	}
}


