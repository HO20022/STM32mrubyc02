/*! @file
  @brief
  Realtime multitask monitor for mruby/c

  <pre>
  Copyright (C) 2015- Kyushu Institute of Technology.
  Copyright (C) 2015- Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.
  </pre>
*/

/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
//@cond
#include "vm_config.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
//@endcond


/***** Local headers ********************************************************/
#include "alloc.h"
#include "load.h"
#include "class.h"
#include "global.h"
#include "symbol.h"
#include "vm.h"
#include "console.h"
#include "c_string.h"
#include "c_array.h"
#include "rrt0.h"
#include "hal_selector.h"


/***** Macros ***************************************************************/
#ifndef MRBC_SCHEDULER_EXIT
#define MRBC_SCHEDULER_EXIT 0
#endif

#define VM2TCB(p) ((mrbc_tcb *)((uint8_t *)p - offsetof(mrbc_tcb, vm)))
#define MRBC_MUTEX_TRACE(...) ((void)0)


/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
#define NUM_TASK_QUEUE 4
static mrbc_tcb *task_queue_[NUM_TASK_QUEUE];
#define q_dormant_   (task_queue_[0])
#define q_ready_     (task_queue_[1])
#define q_waiting_   (task_queue_[2])
#define q_suspended_ (task_queue_[3])
static volatile uint32_t tick_;
static volatile uint32_t wakeup_tick_ = (1 << 16); // no significant meaning.


/***** Global variables *****************************************************/
/***** Signal catching functions ********************************************/
/***** Functions ************************************************************/
//================================================================
/*! Insert task(TCB) to task queue

  @param  p_tcb	Pointer to target TCB

  Put the task (TCB) into a queue by each state.
  TCB must be free. (must not be in another queue)
  The queue is sorted in priority_preemption order.
  If the same priority_preemption value is in the TCB and queue,
  it will be inserted at the end of the same value in queue.
*/
static void q_insert_task(mrbc_tcb *p_tcb)
{
  // select target queue pointer.
  //                    state value = 0  1  2  3  4  5  6  7  8
  //                             /2   0, 0, 1, 1, 2, 2, 3, 3, 4
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrbc_tcb **pp_q = &task_queue_[ conv_tbl[ p_tcb->state / 2 ]];

  // in case of insert on top.
  if((*pp_q == NULL) ||
     (p_tcb->priority_preemption < (*pp_q)->priority_preemption)) {
    p_tcb->next = *pp_q;
    *pp_q       = p_tcb;
    return;
  }

  // find insert point in sorted linked list.
  mrbc_tcb *p = *pp_q;
  while( p->next != NULL ) {
    if( p_tcb->priority_preemption < p->next->priority_preemption ) break;
    p = p->next;
  }

  // insert tcb to queue.
  p_tcb->next = p->next;
  p->next     = p_tcb;
}


//================================================================
/*! Delete task(TCB) from task queue

  @param  p_tcb	Pointer to target TCB
*/
static void q_delete_task(mrbc_tcb *p_tcb)
{
  // select target queue pointer. (same as q_insert_task)
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrbc_tcb **pp_q = &task_queue_[ conv_tbl[ p_tcb->state / 2 ]];

  if( *pp_q == p_tcb ) {
    *pp_q       = p_tcb->next;
    p_tcb->next = NULL;
    return;
  }

  mrbc_tcb *p = *pp_q;
  while( p ) {
    if( p->next == p_tcb ) {
      p->next     = p_tcb->next;
      p_tcb->next = NULL;
      return;
    }

    p = p->next;
  }

  assert(!"Not found target task in queue.");
}


//================================================================
/*! preempt running task
*/
inline static void preempt_running_task(void)
{
  for( mrbc_tcb *t = q_ready_; t != NULL; t = t->next ) {
    if( t->state == TASKSTATE_RUNNING ) t->vm.flag_preemption = 1;
  }
}


//================================================================
/*! Tick timer interrupt handler.

*/
void mrbc_tick(void)
{
  tick_++;

  // Decrease the time slice value for running tasks.
  mrbc_tcb *tcb = q_ready_;
  if( (tcb != NULL) && (tcb->timeslice != 0) ) {
    tcb->timeslice--;
    if( tcb->timeslice == 0 ) tcb->vm.flag_preemption = 1;
  }

  // Check the wakeup tick.
  if( (int32_t)(wakeup_tick_ - tick_) < 0 ) {
    int flag_preemption = 0;
    wakeup_tick_ = tick_ + (1 << 16);

    // Find a wake up task in waiting task queue.
    tcb = q_waiting_;
    while( tcb != NULL ) {
      mrbc_tcb *t = tcb;
      tcb = tcb->next;
      if( t->reason != TASKREASON_SLEEP ) continue;

      if( (int32_t)(t->wakeup_tick - tick_) < 0 ) {
        q_delete_task(t);
        t->state  = TASKSTATE_READY;
        t->reason = 0;
        q_insert_task(t);
        flag_preemption = 1;
      } else if( (int32_t)(t->wakeup_tick - wakeup_tick_) < 0 ) {
        wakeup_tick_ = t->wakeup_tick;
      }
    }

    if( flag_preemption ) preempt_running_task();
  }

}


//================================================================
/*! create (allocate) TCB.

  @param  regs_size	num of allocated registers.
  @param  task_state	task initial state.
  @param  priority	task priority.
  @return pointer to TCB or NULL.

<b>Code example</b>
@code
  //  If you want specify default value, see below.
  //    regs_size:  MAX_REGS_SIZE (in vm_config.h)
  //    task_state: MRBC_TASK_DEFAULT_STATE
  //    priority:   MRBC_TASK_DEFAULT_PRIORITY
  mrbc_tcb *tcb;
  tcb = mrbc_tcb_new( MAX_REGS_SIZE, MRBC_TASK_DEFAULT_STATE, MRBC_TASK_DEFAULT_PRIORITY );
  mrbc_create_task( byte_code, tcb );
@endcode
*/
mrbc_tcb * mrbc_tcb_new( int regs_size, enum MrbcTaskState task_state, int priority )
{
  mrbc_tcb *tcb;

  tcb = mrbc_raw_alloc( sizeof(mrbc_tcb) + sizeof(mrbc_value) * regs_size );
  if( !tcb ) return NULL;	// ENOMEM

  memset(tcb, 0, sizeof(mrbc_tcb));
#if defined(MRBC_DEBUG)
  memcpy( tcb->type, "TCB", 4 );
#endif
  tcb->priority = priority;
  tcb->state = task_state;
  tcb->vm.regs_size = regs_size;

  return tcb;
}


//================================================================
/*! specify running VM code.

  @param  byte_code	pointer to VM byte code.
  @param  tcb		Task control block with parameter, or NULL.
  @return Pointer to mrbc_tcb or NULL.
*/
mrbc_tcb * mrbc_create_task(const void *byte_code, mrbc_tcb *tcb)
{
  if( !tcb ) tcb = mrbc_tcb_new( MAX_REGS_SIZE, MRBC_TASK_DEFAULT_STATE, MRBC_TASK_DEFAULT_PRIORITY );
  if( !tcb ) return NULL;	// ENOMEM

  tcb->priority_preemption = tcb->priority;

  // assign VM ID
  if( mrbc_vm_open( &tcb->vm ) == NULL ) {
    mrbc_printf("Error: Can't assign VM-ID.\n");
    return NULL;
  }

  if( mrbc_load_mrb(&tcb->vm, byte_code) != 0 ) {
    mrbc_print_vm_exception( &tcb->vm );
    mrbc_vm_close( &tcb->vm );
    return NULL;
  }
  mrbc_vm_begin( &tcb->vm );

  hal_disable_irq();
  q_insert_task(tcb);
  if( tcb->state & TASKSTATE_READY ) preempt_running_task();
  hal_enable_irq();

  return tcb;
}


//================================================================
/*! set the task name.

  @param  tcb	target task.
  @param  name	task name
*/
void mrbc_set_task_name(mrbc_tcb *tcb, const char *name)
{
  /* (note)
   this is `strncpy( tcb->name, name, MRBC_TASK_NAME_LEN );`
   for to avoid link error when compiling for PIC32 with XC32 v4.21
  */
  for( int i = 0; i < MRBC_TASK_NAME_LEN; i++ ) {
    if( (tcb->name[i] = *name++) == 0 ) break;
  }
}


//================================================================
/*! find task by name

  @param  name		task name
  @return pointer to mrbc_tcb or NULL
*/
mrbc_tcb * mrbc_find_task(const char *name)
{
  mrbc_tcb *tcb = 0;
  hal_disable_irq();

  for( int i = 0; i < NUM_TASK_QUEUE; i++ ) {
    for( tcb = task_queue_[i]; tcb != NULL; tcb = tcb->next ) {
      if( strcmp( tcb->name, name ) == 0 ) goto RETURN_TCB;
    }
  }

 RETURN_TCB:
  hal_enable_irq();
  return tcb;
}


//================================================================
/*! Start execution of dormant task.

  @param  tcb	target task.
  @retval int	zero / no error.
*/
int mrbc_start_task(mrbc_tcb *tcb)
{
  if( tcb->state != TASKSTATE_DORMANT ) return -1;

  hal_disable_irq();

  preempt_running_task();

  q_delete_task(tcb);
  tcb->state = TASKSTATE_READY;
  tcb->reason = 0;
  tcb->priority_preemption = tcb->priority;
  q_insert_task(tcb);

  hal_enable_irq();

  return 0;
}


//================================================================
/*! execute

*/
int mrbc_run(void)
{
  int ret = 0;
#if MRBC_SCHEDULER_EXIT
  if( !q_ready_ && !q_waiting_ && !q_suspended_ ) return ret;
#endif

  while( 1 ) {
    mrbc_tcb *tcb = q_ready_;
    if( tcb == NULL ) {		// no task to run.
      hal_idle_cpu();
      continue;
    }

    /*
      run the task.
    */
    tcb->state = TASKSTATE_RUNNING;   // to execute.
    tcb->timeslice = MRBC_TIMESLICE_TICK_COUNT;

#if !defined(MRBC_NO_TIMER)
    // Using hardware timer.
    tcb->vm.flag_preemption = 0;
    int ret_vm_run = mrbc_vm_run(&tcb->vm);
#else
    // Emulate time slice preemption.
    int ret_vm_run;
    tcb->vm.flag_preemption = 1;
    while( tcb->timeslice != 0 ) {
      ret_vm_run = mrbc_vm_run( &tcb->vm );
      tcb->timeslice--;
      if( ret_vm_run != 0 ) break;
      if( tcb->state != TASKSTATE_RUNNING ) break;
    }
    mrbc_tick();
#endif

    /*
      did the task done?
    */
    if( ret_vm_run != 0 ) {
      hal_disable_irq();
      q_delete_task(tcb);
      tcb->state = TASKSTATE_DORMANT;
      q_insert_task(tcb);
      hal_enable_irq();

      if( ! tcb->vm.flag_permanence ) mrbc_vm_end(&tcb->vm);
      if( ret_vm_run != 1 ) ret = ret_vm_run;   // for debug info.

      // find task that called join.
      for( mrbc_tcb *tcb1 = q_waiting_; tcb1 != NULL; tcb1 = tcb1->next ) {
        if( tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb ) {
          hal_disable_irq();
          q_delete_task(tcb1);
          tcb1->state = TASKSTATE_READY;
          tcb1->reason = 0;
          q_insert_task(tcb1);
          hal_enable_irq();
        }
      }
#if MRBC_SCHEDULER_EXIT
      if( !q_ready_ && !q_waiting_ && !q_suspended_ ) return ret;
#endif
      continue;
    }

    /*
      Switch task.
    */
    if( tcb->state == TASKSTATE_RUNNING ) {
      tcb->state = TASKSTATE_READY;

      hal_disable_irq();
      q_delete_task(tcb);       // insert task on queue last.
      q_insert_task(tcb);
      hal_enable_irq();
    }
    continue;
  }
}


//================================================================
/*! sleep for a specified number of milliseconds.

  @param  tcb	target task.
  @param  ms	sleep milliseconds.
*/
void mrbc_sleep_ms(mrbc_tcb *tcb, uint32_t ms)
{
  hal_disable_irq();
  q_delete_task(tcb);
  tcb->state       = TASKSTATE_WAITING;
  tcb->reason      = TASKREASON_SLEEP;
  tcb->wakeup_tick = tick_ + (ms / MRBC_TICK_UNIT) + !!(ms % MRBC_TICK_UNIT);

  if( (int32_t)(tcb->wakeup_tick - wakeup_tick_) < 0 ) {
    wakeup_tick_ = tcb->wakeup_tick;
  }

  q_insert_task(tcb);
  hal_enable_irq();

  tcb->vm.flag_preemption = 1;
}


//================================================================
/*! Relinquish control to other tasks.

  @param  tcb	target task.
*/
void mrbc_relinquish(mrbc_tcb *tcb)
{
  tcb->timeslice          = 0;
  tcb->vm.flag_preemption = 1;
}


//================================================================
/*! change task priority.

  @param  tcb		target task.
  @param  priority	priority value. between 1 and 255.
*/
void mrbc_change_priority(mrbc_tcb *tcb, int priority)
{
  tcb->priority            = priority;
  tcb->priority_preemption = priority;

  hal_disable_irq();
  q_delete_task(tcb);       // reorder task queue according to priority.
  q_insert_task(tcb);

  if( tcb->state & TASKSTATE_READY ) preempt_running_task();

  hal_enable_irq();
}


//================================================================
/*! Suspend the task.

  @param  tcb		target task.
*/
void mrbc_suspend_task(mrbc_tcb *tcb)
{
  if( tcb->state == TASKSTATE_SUSPENDED ) return;

  hal_disable_irq();
  q_delete_task(tcb);
  tcb->state = TASKSTATE_SUSPENDED;
  q_insert_task(tcb);
  hal_enable_irq();

  tcb->vm.flag_preemption = 1;
}


//================================================================
/*! resume the task

  @param  tcb		target task.
*/
void mrbc_resume_task(mrbc_tcb *tcb)
{
  if( tcb->state != TASKSTATE_SUSPENDED ) return;

  int flag_to_ready_state = (tcb->reason == 0);

  hal_disable_irq();

  if( flag_to_ready_state ) preempt_running_task();

  q_delete_task(tcb);
  tcb->state = flag_to_ready_state ? TASKSTATE_READY : TASKSTATE_WAITING;
  q_insert_task(tcb);

  hal_enable_irq();
}


//================================================================
/*! terminate the task.

  @param  tcb		target task.
  @note
    This API simply ends the task.
    note that this does not affect the lock status of mutex.
*/
void mrbc_terminate_task(mrbc_tcb *tcb)
{
  if( tcb->state == TASKSTATE_DORMANT ) return;

  hal_disable_irq();
  q_delete_task(tcb);
  tcb->state = TASKSTATE_DORMANT;
  q_insert_task(tcb);
  hal_enable_irq();

  tcb->vm.flag_preemption = 1;
}


//================================================================
/*! join the task.

  @param  tcb		target task.
  @param  tcb_join	join task.
*/
void mrbc_join_task(mrbc_tcb *tcb, const mrbc_tcb *tcb_join)
{
  if( tcb->state == TASKSTATE_DORMANT ) return;
  if( tcb_join->state == TASKSTATE_DORMANT ) return;

  hal_disable_irq();
  q_delete_task(tcb);

  tcb->state    = TASKSTATE_WAITING;
  tcb->reason   = TASKREASON_JOIN;
  tcb->tcb_join = tcb_join;

  q_insert_task(tcb);
  hal_enable_irq();

  tcb->vm.flag_preemption = 1;
}



//================================================================
/*! mutex initialize

  @param  mutex		pointer to mrbc_mutex or NULL.
*/
mrbc_mutex * mrbc_mutex_init( mrbc_mutex *mutex )
{
  if( mutex == NULL ) {
    mutex = mrbc_raw_alloc( sizeof(mrbc_mutex) );
    if( mutex == NULL ) return NULL;	// ENOMEM
  }

  static const mrbc_mutex init_val = MRBC_MUTEX_INITIALIZER;
  *mutex = init_val;

  return mutex;
}


//================================================================
/*! mutex lock

  @param  mutex		pointer to mutex.
  @param  tcb		pointer to TCB.
*/
int mrbc_mutex_lock( mrbc_mutex *mutex, mrbc_tcb *tcb )
{
  MRBC_MUTEX_TRACE("mutex lock / MUTEX: %p TCB: %p",  mutex, tcb );

  int ret = 0;
  hal_disable_irq();

  // Try lock mutex;
  if( mutex->lock == 0 ) {      // a future does use TAS?
    mutex->lock = 1;
    mutex->tcb = tcb;
    MRBC_MUTEX_TRACE("  lock OK\n" );
    goto DONE;
  }
  MRBC_MUTEX_TRACE("  lock FAIL\n" );

  // Can't lock mutex
  // check recursive lock.
  if( mutex->tcb == tcb ) {
    ret = 1;
    goto DONE;
  }

  // To WAITING state.
  q_delete_task(tcb);
  tcb->state  = TASKSTATE_WAITING;
  tcb->reason = TASKREASON_MUTEX;
  tcb->mutex = mutex;
  q_insert_task(tcb);
  tcb->vm.flag_preemption = 1;

 DONE:
  hal_enable_irq();

  return ret;
}


//================================================================
/*! mutex unlock

  @param  mutex		pointer to mutex.
  @param  tcb		pointer to TCB.
*/
int mrbc_mutex_unlock( mrbc_mutex *mutex, mrbc_tcb *tcb )
{
  MRBC_MUTEX_TRACE("mutex unlock / MUTEX: %p TCB: %p\n",  mutex, tcb );

  // check some parameters.
  if( !mutex->lock ) return 1;
  if( mutex->tcb != tcb ) return 2;

  hal_disable_irq();

  // wakeup ONE waiting task if exist.
  mrbc_tcb *tcb1;
  for( tcb1 = q_waiting_; tcb1 != NULL; tcb1 = tcb1->next ) {
    if( tcb1->reason == TASKREASON_MUTEX && tcb1->mutex == mutex ) break;
  }
  if( tcb1 ) {
    MRBC_MUTEX_TRACE("SW1: TCB: %p\n", tcb1 );
    mutex->tcb = tcb1;

    q_delete_task(tcb1);
    tcb1->state = TASKSTATE_READY;
    tcb1->reason = 0;
    q_insert_task(tcb1);

    preempt_running_task();
    goto DONE;
  }

  // find ONE mutex locked task in suspended queue.
  for( tcb1 = q_suspended_; tcb1 != NULL; tcb1 = tcb1->next ) {
    if( tcb1->reason == TASKREASON_MUTEX && tcb1->mutex == mutex ) break;
  }
  if( tcb1 ) {
    MRBC_MUTEX_TRACE("SW2: TCB: %p\n", tcb1 );
    mutex->tcb = tcb1;
    tcb1->reason = 0;
    goto DONE;
  }

  // other case, unlock mutex
  MRBC_MUTEX_TRACE("mutex unlock all.\n" );
  mutex->lock = 0;
  mutex->tcb = 0;

 DONE:
  hal_enable_irq();

  return 0;
}


//================================================================
/*! mutex trylock

  @param  mutex		pointer to mutex.
  @param  tcb		pointer to TCB.
*/
int mrbc_mutex_trylock( mrbc_mutex *mutex, mrbc_tcb *tcb )
{
  MRBC_MUTEX_TRACE("mutex try lock / MUTEX: %p TCB: %p",  mutex, tcb );

  int ret;
  hal_disable_irq();

  if( mutex->lock == 0 ) {
    mutex->lock = 1;
    mutex->tcb = tcb;
    ret = 0;
    MRBC_MUTEX_TRACE("  trylock OK\n" );
  }
  else {
    MRBC_MUTEX_TRACE("  trylock FAIL\n" );
    ret = 1;
  }

  hal_enable_irq();
  return ret;
}


//================================================================
/*! clenaup all resources.

*/
void mrbc_cleanup(void)
{
  mrbc_cleanup_vm();
  mrbc_cleanup_symbol();
  mrbc_cleanup_alloc();

  q_dormant_ = 0;
  q_ready_ = 0;
  q_waiting_ = 0;
  q_suspended_ = 0;
}


//================================================================
/*! (method) sleep for a specified number of seconds (CRuby compatible)

*/
static void c_sleep(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb = VM2TCB(vm);

  if( argc == 0 ) {
    mrbc_suspend_task(tcb);
    return;
  }

  switch( mrbc_type(v[1]) ) {
  case MRBC_TT_INTEGER:
  {
    mrbc_int_t sec;
    sec = mrbc_integer(v[1]);
    SET_INT_RETURN(sec);
    mrbc_sleep_ms(tcb, sec * 1000);
    break;
  }

#if MRBC_USE_FLOAT
  case MRBC_TT_FLOAT:
  {
    mrbc_float_t sec;
    sec = mrbc_float(v[1]);
    SET_INT_RETURN(sec);
    mrbc_sleep_ms(tcb, (mrbc_int_t)(sec * 1000));
    break;
  }
#endif

  default:
    break;
  }
}


//================================================================
/*! (method) sleep for a specified number of milliseconds.

*/
static void c_sleep_ms(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb = VM2TCB(vm);

  mrbc_int_t sec = mrbc_integer(v[1]);
  SET_INT_RETURN(sec);
  mrbc_sleep_ms(tcb, sec);
}



/*
  Task class
*/
//================================================================
/*! (method) get task

  Task.get()           -> Task
  Task.get("TaskName") -> Task|nil
*/
static void c_task_get(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb = NULL;

  if( v[0].tt != MRBC_TT_CLASS ) goto RETURN_NIL;

  // in case of Task.get()
  if( argc == 0 ) {
    tcb = VM2TCB(vm);
  }

  // in case of Task.get("TasName")
  else if( v[1].tt == MRBC_TT_STRING ) {
    tcb = mrbc_find_task( mrbc_string_cstr( &v[1] ) );
  }

  if( tcb ) {
    mrbc_value ret = mrbc_instance_new(vm, v->cls, sizeof(mrbc_tcb *));
    *(mrbc_tcb **)ret.instance->data = tcb;
    SET_RETURN(ret);
    return;             // normal return.
  }

 RETURN_NIL:
  SET_NIL_RETURN();
}


//================================================================
/*! (method) task list

  Task.list() -> Array[Task]
*/
static void c_task_list(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ret = mrbc_array_new(vm, 1);

  hal_disable_irq();

  for( int i = 0; i < NUM_TASK_QUEUE; i++ ) {
    for( mrbc_tcb *tcb = task_queue_[i]; tcb != NULL; tcb = tcb->next ) {
      mrbc_value task = mrbc_instance_new(vm, v->cls, sizeof(mrbc_tcb *));
      *(mrbc_tcb **)task.instance->data = VM2TCB(vm);
      mrbc_array_push( &ret, &task );
    }
  }

  hal_enable_irq();

  SET_RETURN(ret);
}


//================================================================
/*! (method) task name list

  Task.name_list() -> Array[String]
*/
static void c_task_name_list(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ret = mrbc_array_new(vm, 1);

  hal_disable_irq();

  for( int i = 0; i < NUM_TASK_QUEUE; i++ ) {
    for( mrbc_tcb *tcb = task_queue_[i]; tcb != NULL; tcb = tcb->next ) {
      mrbc_value s = mrbc_string_new_cstr(vm, tcb->name);
      mrbc_array_push( &ret, &s );
    }
  }

  hal_enable_irq();

  SET_RETURN(ret);
}


//================================================================
/*! (method) name setter.

  Task.name = "MyName"
*/
static void c_task_set_name(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if( v[1].tt != MRBC_TT_STRING ) {
    mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
    return;
  }

  mrbc_tcb *tcb;

  if( v[0].tt == MRBC_TT_CLASS ) {
    tcb = VM2TCB(vm);
  } else {
    tcb = *(mrbc_tcb **)v[0].instance->data;
  }
  mrbc_set_task_name( tcb, mrbc_string_cstr(&v[1]) );
}


//================================================================
/*! (method) name getter

  Task.name() -> String    # get current task name
  task.name() -> String
*/
static void c_task_name(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ret;

  if( v[0].tt == MRBC_TT_CLASS ) {
    ret = mrbc_string_new_cstr( vm, VM2TCB(vm)->name );
  } else {
    mrbc_tcb *tcb = *(mrbc_tcb **)v[0].instance->data;
    ret = mrbc_string_new_cstr(vm, tcb->name );
  }

  SET_RETURN(ret);
}


//================================================================
/*! (method) suspend task

  Task.suspend()        # suspend current task.
  task.suspend()        # suspend other task.
*/
static void c_task_suspend(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb;

  if( v[0].tt == MRBC_TT_CLASS ) {
    tcb = VM2TCB(vm);
  } else {
    tcb = *(mrbc_tcb **)v[0].instance->data;
  }

  mrbc_suspend_task(tcb);
}


//================================================================
/*! (method) resume task

  task.resume()
*/
static void c_task_resume(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if( v[0].tt == MRBC_TT_CLASS ) return;

  mrbc_tcb *tcb = *(mrbc_tcb **)v[0].instance->data;

  mrbc_resume_task(tcb);
}


//================================================================
/*! (method) terminate task

  task.terminate()
*/
static void c_task_terminate(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb;

  if( v[0].tt == MRBC_TT_CLASS ) {
    tcb = VM2TCB(vm);
  } else {
    tcb = *(mrbc_tcb **)v[0].instance->data;
  }

  mrbc_terminate_task(tcb);
}


//================================================================
/*! (method) join task

  task.join()
*/
static void c_task_join(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if( v[0].tt == MRBC_TT_CLASS ) return;

  mrbc_tcb *tcb_me = VM2TCB(vm);
  mrbc_tcb *tcb_join = *(mrbc_tcb **)v[0].instance->data;

  mrbc_join_task(tcb_me, tcb_join);
}


//================================================================
/*! (method) task pass

  Task.pass()
*/
static void c_task_pass(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if( v[0].tt != MRBC_TT_CLASS ) return;

  mrbc_tcb *tcb = VM2TCB(vm);
  mrbc_relinquish(tcb);
}


//================================================================
/*! (method) task priority setter

  Task.priority = n  # n = 0(high) .. 255(low)
  task.priority = n
*/
static void c_task_set_priority(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb;

  if( v[0].tt == MRBC_TT_CLASS ) {
    tcb = VM2TCB(vm);
  } else {
    tcb = *(mrbc_tcb **)v[0].instance->data;
  }

  if( v[1].tt != MRBC_TT_INTEGER ) {
    mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
    return;
  }
  int n = mrbc_integer( v[1] );
  if( n < 0 || n > 255 ) {
    mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
    return;
  }

  mrbc_change_priority( tcb, n );
}


//================================================================
/*! (method) task priority getter

  task.priority() -> Integer
*/
static void c_task_priority(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_tcb *tcb;

  if( v[0].tt == MRBC_TT_CLASS ) {
    tcb = VM2TCB(vm);
  } else {
    tcb = *(mrbc_tcb **)v[0].instance->data;
  }

  SET_INT_RETURN( tcb->priority );
}


//================================================================
/*! (method) status

  task.status() -> String
*/
static void c_task_status(mrbc_vm *vm, mrbc_value v[], int argc)
{
  static const char *status_name[] =
    { "DORMANT", "READY", "WAITING ", "", "SUSPENDED" };
  static const char *reason_name[] =
    { "", "SLEEP", "MUTEX", "", "JOIN" };

  if( v[0].tt == MRBC_TT_CLASS ) return;

  const mrbc_tcb *tcb = *(mrbc_tcb **)v[0].instance->data;
  mrbc_value ret = mrbc_string_new_cstr( vm, status_name[tcb->state / 2] );

  if( tcb->state == TASKSTATE_WAITING ) {
    mrbc_string_append_cstr( &ret, reason_name[tcb->reason] );
  }

  SET_RETURN(ret);
}


/* MRBC_AUTOGEN_METHOD_TABLE

  CLASS("Task")
  FILE("_autogen_class_rrt0.h")

  METHOD( "get", c_task_get )
  METHOD( "current", c_task_get )
  METHOD( "list", c_task_list )
  METHOD( "name_list", c_task_name_list )
  METHOD( "name=", c_task_set_name )
  METHOD( "name", c_task_name )
  METHOD( "suspend", c_task_suspend )
  METHOD( "resume", c_task_resume )
  METHOD( "terminate", c_task_terminate )
  METHOD( "join", c_task_join )
  METHOD( "pass", c_task_pass )
  METHOD( "priority=", c_task_set_priority )
  METHOD( "priority", c_task_priority )
  METHOD( "status", c_task_status )
*/


/*
  Mutex class
*/
//================================================================
/*! (method) mutex constructor

*/
static void c_mutex_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  *v = mrbc_instance_new(vm, v->cls, sizeof(mrbc_mutex));
  if( !v->instance ) return;

  mrbc_mutex_init( (mrbc_mutex *)(v->instance->data) );
}


//================================================================
/*! (method) mutex lock

*/
static void c_mutex_lock(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int r = mrbc_mutex_lock( (mrbc_mutex *)v->instance->data, VM2TCB(vm) );
  if( r == 0 ) return;  // return self

  // raise ThreadError
  assert(!"Mutex recursive lock.");
}


//================================================================
/*! (method) mutex unlock

*/
static void c_mutex_unlock(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int r = mrbc_mutex_unlock( (mrbc_mutex *)v->instance->data, VM2TCB(vm) );
  if( r == 0 ) return;  // return self

  // raise ThreadError
  assert(!"Mutex unlock error. not owner or not locked.");
}


//================================================================
/*! (method) mutex trylock

*/
static void c_mutex_trylock(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int r = mrbc_mutex_trylock( (mrbc_mutex *)v->instance->data, VM2TCB(vm) );
  SET_BOOL_RETURN( r == 0 );
}


//================================================================
/*! (method) mutex locked?

*/
static void c_mutex_locked(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_mutex *mutex = (mrbc_mutex *)v->instance->data;
  SET_BOOL_RETURN( mutex->lock != 0 );
}


//================================================================
/*! (method) mutex owned?

*/
static void c_mutex_owned(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_mutex *mutex = (mrbc_mutex *)v->instance->data;
  SET_BOOL_RETURN( mutex->lock != 0 && mutex->tcb == VM2TCB(vm) );
}


/* MRBC_AUTOGEN_METHOD_TABLE

  CLASS("Mutex")
  APPEND("_autogen_class_rrt0.h")

  METHOD( "new", c_mutex_new  )
  METHOD( "lock", c_mutex_lock  )
  METHOD( "unlock", c_mutex_unlock  )
  METHOD( "try_lock", c_mutex_trylock  )
  METHOD( "locked?", c_mutex_locked  )
  METHOD( "owned?", c_mutex_owned  )
*/



//================================================================
/*! (method) get tick counter
*/
static void c_vm_tick(mrbc_vm *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(tick_);
}

/* MRBC_AUTOGEN_METHOD_TABLE

  CLASS("VM")
  APPEND("_autogen_class_rrt0.h")

  METHOD( "tick", c_vm_tick )
*/
#include "_autogen_class_rrt0.h"



//================================================================
/*! initialize

  @param  heap_ptr	heap memory buffer.
  @param  size		its size.
*/
void mrbc_init(void *heap_ptr, unsigned int size)
{
  hal_init();
  mrbc_init_alloc(heap_ptr, size);
  mrbc_init_global();
  mrbc_init_class();

  mrbc_value cls = {.tt = MRBC_TT_CLASS, .cls = MRBC_CLASS(Task)};
  mrbc_set_const( MRBC_SYM(Task), &cls );

  cls.cls = MRBC_CLASS(Mutex);
  mrbc_set_const( MRBC_SYM(Mutex), &cls );

  cls.cls = MRBC_CLASS(VM);
  mrbc_set_const( MRBC_SYM(VM), &cls );

  mrbc_define_method(0, mrbc_class_object, "sleep", c_sleep);
  mrbc_define_method(0, mrbc_class_object, "sleep_ms", c_sleep_ms);
}



#ifdef MRBC_DEBUG
//================================================================
/*! DEBUG print queue

  (examples)
  void pqall(void);
  mrbc_define_method(0,0,"pqall", (mrbc_func_t)pqall);
 */
void pq(const mrbc_tcb *p_tcb)
{
  if( p_tcb == NULL ) return;

  // TCB address
  for( const mrbc_tcb *p = p_tcb; p; p = p->next ) {
#if defined(UINTPTR_MAX)
    mrbc_printf("$%08x   ", (uint32_t)(uintptr_t)p);
#else
    mrbc_printf("$%08x   ", (uint32_t)p);
#endif
  }
  mrbc_printf("\n");

  // name
  for( const mrbc_tcb *p = p_tcb; p; p = p->next ) {
    mrbc_printf("%-11.11s ", p->name[0] ? p->name : "(noname)" );
  }
  mrbc_printf("\n");

#if 0
  // next ptr
  for( const mrbc_tcb *p = p_tcb; p; p = p->next ) {
#if defined(UINTPTR_MAX)
    mrbc_printf(" next:%04x  ", (uint16_t)(uintptr_t)p->next);
#else
    mrbc_printf(" next:%04x  ", (uint16_t)p->next);
#endif
  }
  mrbc_printf("\n");
#endif

  // task priority.
  for( const mrbc_tcb *p = p_tcb; p; p = p->next ) {
    mrbc_printf(" pri:%3d    ", p->priority_preemption);
  }
  mrbc_printf("\n");

  // state
  //  st:SsRr
  //     ^ suspended -> S:suspended
  //      ^ waiting  -> s:sleep m:mutex J:join
  //       ^ ready   -> R:ready
  //        ^ running-> r:running
  for( const mrbc_tcb *p = p_tcb; p; p = p->next ) {
#if 1
    mrbc_tcb t = *p;
    mrbc_printf(" st:%c%c%c%c    ",
        (t.state & TASKSTATE_SUSPENDED)?'S':'-',
        (t.state & TASKSTATE_WAITING)?("!sm!j"[t.reason]):"!-"[t.reason == 0],
        (t.state & 0x02)?'R':'-',
        (t.state & 0x01)?'r':'-' );
#else
    mrbc_printf(" st:%04b    ", p->state);
#endif
  }
  mrbc_printf("\n");

  // timeslice, vm->flag_preemption
  for( const mrbc_tcb *p = p_tcb; p; p = p->next ) {
    mrbc_tcb t = *p;
    mrbc_printf(" ts,fp:%2d,%d ", t.timeslice, t.vm.flag_preemption);
  }
  mrbc_printf("\n");
}

void pqall(void)
{
  hal_disable_irq();
  mrbc_printf("<<<<< DORMANT >>>>>\n");   pq(q_dormant_);
  mrbc_printf("<<<<< READY >>>>>\n");     pq(q_ready_);
  mrbc_printf("<<<<< WAITING >>>>>\n");   pq(q_waiting_);
  mrbc_printf("<<<<< SUSPENDED >>>>>\n"); pq(q_suspended_);
  hal_enable_irq();
}
#endif
