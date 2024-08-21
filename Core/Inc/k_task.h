/**
 * @file k_task.h
 * @author Nicholas Cantone
 * @date July 2024
 * @brief Task header.
 */


#ifndef INC_K_TASK_H_
#define INC_K_TASK_H_
#include "common.h"


/************************************************
 *               DEFINITIONS
 ************************************************/

#define SHPR2 *(uint32_t*)0xE000ED1C //for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t*)0xE000ED20 //PendSV is bits 23-16

/************************************************
 *               TYPEDEFS
 ************************************************/

typedef unsigned int U32;
typedef unsigned short U16;
typedef char U8;

//Task ID type.
typedef unsigned int task_t;

//Task state
enum task_state {
	TASK_DORMANT = 0,
	TASK_READY = 1,
	TASK_RUNNING = 2,
	TASK_SLEEPING = 3
};

// Struct to contain all task data.
typedef struct task_control_block{
	void (*ptask)(void* args); // entry address
	task_t tid; // task ID
	U8 state; // task's state
	U16 stack_size; // stack size. Must be a multiple of 8
	U32 stack_high; // largest address for task stack
	U32* SP; // stack pointer
	U32* p_stack_mem; //pointer to address of dynamically allocated stack
	U32 remaining_sleep_time;
	U32 deadline; //a fixed deadline for a periodic task
	U32 remaining_time; //decrements every tick, is initially equal to the deadline
}TCB;


//Master struct containing all data for kernel functions.
typedef struct kernel_config_t {
	TCB TCBS[MAX_TASKS];
	U8 num_running_tasks;
	U8 is_running; //as bool 0 = False else true
	task_t running_task;
}KERNEL_CONFIG;

/************************************************
 *             GLOBAL VARS
 ************************************************/

extern KERNEL_CONFIG kernel_config;

/************************************************
 *              FUNCTION DEFS
 ************************************************/

/*
 * @brief: Initializes all global kernel-level data structures and sets interrupt priorities.
 */
void osKernelInit(void);


/*
 * @brief: Create a new task and register it with the RTX
 *
 * @param task: pointer to new task's TCB.
 * @return RTX_OK on success and RTX_ERR on failure
 */
int osCreateTask(TCB* task);


/*
 * @brief: This function is called by application code after the kernel has been initialized to run the
 *         first task. 
 *
 * @return RTX_OK on success and RTX_ERR if the kernel is not initialized or already running.
 */
int osKernelStart(void);

/*
 * @brief:  Halts the execution of one task, saves it contexts, runs the
 *          scheduler, and loads the context of the next task to run.
 */
void osYield(void);

/*
 * @brief: Retrieve the information from the TCB of the task with id TID, and fill the TCB pointed to by task_copy.
 *
 * @param TID: TID of task we would like to get info
 * @param task_copy: new TCB to be populated with contents the TCB belonging to the task with given TID.
 * @return: RTX_OK if a task with the given TID exists, and RTX_ERR otherwise.
 */
int osTaskInfo(task_t TID, TCB* task_copy);

/*
 * @brief: Enables a task to find out its own TID.
 *
 * @return: TID of running task.
 */
task_t osGetTID (void);

/*
 * @brief: Immediately causes the task to exit and the scheduler to be called.
 *
 * @return: RTX_OK if it was called by a running task. Otherwise, it should return RTX_ERR.
 */
int osTaskExit(void);

/*
 * @brief: Performs same operations as osYield, in addition, this function removes this task from
 *         scheduling until the provided sleep time has expired. 
 *
 * @param timeInMs: amount of time the task should sleep.
 */
void osSleep(int timeInMs);

/*
 * @brief: Performs same operations as osYield, however does not reset the task's remaining time.
 */
void ContextSwitch(void);

/*
 * @brief: A periodic task will call this function instead of osYield when it has completed its current instance. 
 */
void osPeriodYield(void);

/*
 * @brief: Sets the deadline of the task with Id TID to the deadline stored in the deadline variable.
 *
 * @param deadline: new deadline value.
 * @param TID: ID of task to be updated.
 * @return: RTX_OK if the deadline provided is positive and a task with the given TID exists and 
 *          is ready to run. Otherwise it returns RTX_ERR.
 */
int osSetDeadline(int deadline, task_t TID);

/*
 * @brief: Create a new task and register it with the RTX if possible. This task has the deadline given in
 *         the deadline variable.
 *
 * @param deadline: deadline value for new task.
 * @param task: pointer to new task's TCB.
 * @return: RTX_OK on success and RTX_ERR on failure.
 */
int osCreateDeadlineTask(int deadline, TCB* task);

#endif /* INC_K_TASK_H_ */
