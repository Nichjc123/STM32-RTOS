#include "k_task.h"
#include "k_mem.h"
#include "common.h"
#include <stdio.h>
#include <limits.h>
#include "stm32f4xx.h"

/************************************************
 *             DEFINITIONS
 ************************************************/

#define DEFAULT_DEADLINE              -1
#define DEFAULT_SLEEP_TIME            -1
#define NULL_TASK_TID                 0

/************************************************
 *             GLOBAL VARS
 ************************************************/

KERNEL_CONFIG kernel_config;

/************************************************
 *             HELPER FUNCTIONS
 ************************************************/

// A task scheduled when no other tasks are available.
void null_task(void *) {
    while (1) {
        // Put the CPU in a low-power state or perform background tasks
        __WFI(); // Wait For Interrupt, put CPU in sleep mode
    }
}

// Create the null task
void osNull_task(void){
	TCB* create_tcb = &kernel_config.TCBS[0];
	create_tcb->tid = NULL_TASK_TID;

	// Push null task function
	create_tcb->ptask = &null_task;
	create_tcb->stack_size = STACK_SIZE;

	U32* MSP_INIT_VAL = *(U32**)0x0;
	create_tcb->p_stack_mem = (U32)(MSP_INIT_VAL - MAIN_STACK_SIZE);
	// Initialize the stack for the task
	create_tcb->SP = (U32)(create_tcb->p_stack_mem);
	U32* stackptr = (U32*)create_tcb->SP;

	// Set up initial stack frame
	*(--stackptr) = 1 << 24;                    // xPSR, setting Thumb mode
	*(--stackptr) = create_tcb->ptask;          // PC, function address
	for (int i = 0; i < 14; i++) {
		*(--stackptr) = 0xA;
	}

	create_tcb->SP = stackptr;
}

static task_t scheduler(void)
{
	task_t next_task = TID_DORMANT;
	int earliest_deadline = INT_MAX;
	int all_sleeping = TRUE;
	int all_dormant = TRUE;


	//Find earliest deadline among ready tasks
    for (int i = 1; i < MAX_TASKS; i++)
    {
        if (kernel_config.TCBS[i].state != DORMANT)
        {
			// Found a non-dormant task.
            all_dormant = FALSE;

            if (kernel_config.TCBS[i].state == READY)
            {
                all_sleeping = FALSE;
				// Update earliest deadline.
                if (kernel_config.TCBS[i].remaining_time < earliest_deadline)
                {
                    earliest_deadline = kernel_config.TCBS[i].remaining_time;
                    next_task = i;
                }
            } else if (kernel_config.TCBS[i].state == RUNNING)
            {
				// Found a running task.
                all_sleeping = FALSE;
            }
        }
    }

    //If all tasks are dormant/ sleeping
    if (all_dormant || all_sleeping)
    {
    	return 0;
    }

    return next_task;
}

void SVC_Handler_Main(unsigned int *svc_args) {

	unsigned int svc_number;

    // Stack frame contains: R0, R1, R2, R3, R12, LR, PC, xPSR
    // SVC number is the immediate value passed in the SVC instruction
    // It can be found at the address of the PC (Program Counter) - 2
    svc_number = ((char *)svc_args[6])[-2];

    // Handle the system call based on the svc_number
    // For demonstration, we'll just print the SVC number
    printf("System Call Number: %u\n", svc_number);

    switch (svc_number){
		case 0:
			__set_CONTROL(__get_CONTROL()& ~CONTROL_nPRIV_Msk);
			break;
		case 1:
			os_kernel_start();
			break;
    }
    return;

}

/*
 * This function is called from the PendSV handler in the context switch process 
 * to get the new task to run and set some of its state.
*/
void new_task()
{
	task_t new_task;

	kernel_config.TCBS[kernel_config.running_task].SP = __get_PSP();
	if (kernel_config.TCBS[kernel_config.running_task].state == RUNNING){
		kernel_config.TCBS[kernel_config.running_task].state = READY;
	}

	// Run scheduler to get new task to run and set to running task
	new_task = scheduler();

	kernel_config.running_task = new_task;

	// set state of new task to running
	kernel_config.TCBS[new_task].state = RUNNING;

	// Update PSP to SP of new task
	__set_PSP((U32)kernel_config.TCBS[new_task].SP);

	return;
}

// Functions for printing kernel info
static const char* get_task_state_string(U8 state) {
    switch (state) {
        case TASK_DORMANT: return "DORMANT";
        case TASK_READY: return "READY";
        case TASK_RUNNING: return "RUNNING";
        case TASK_SLEEPING: return "SLEEPING";
        default: return "UNKNOWN";
    }
}

void print_kernel_info(void) {
    printf("=== Kernel Configuration ===\r\n");
    printf("Number of running tasks: %u\r\n", kernel_config.num_running_tasks);
    printf("Kernel is running: %s\r\n", kernel_config.is_running ? "Yes" : "No");
    printf("Currently running task ID: %u\r\n\n", kernel_config.running_task);

    printf("=== Task Control Blocks ===\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        TCB* tcb = &kernel_config.TCBS[i];

        printf("Task %d:\r\n", i);
        printf("Task ID: %u\r\n", tcb->tid);
        printf("Task State: %s\r\n", get_task_state_string(tcb->state));
        printf("Stack Size: %u bytes\r\n", tcb->stack_size);
        printf("Stack Pointer: %p\r\n", (void*)tcb->SP);
        printf("Stack Memory: %p\r\n", (void*)tcb->p_stack_mem);
        printf("Remaining Sleep Time: %u\r\n", tcb->remaining_sleep_time);
        printf("Deadline: %u\r\n", tcb->deadline);
        printf("Remaining Time: %u\r\n", tcb->remaining_time);
        printf("Task Function Address: %p\r\n", (void*)tcb->ptask);
        printf("\n");  // Add an extra newline for separation between tasks
    }
}

/************************************************
 *             FUNCTIONS
 ************************************************/

void ContextSwitch(void)
{
	if(kernel_config.is_running == FALSE || kernel_config.running_task == TID_DORMANT)
	{
		return;
	}

	// Call PendSV handler (Lab1.s)
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	__asm("isb");

	return;
}

void osKernelInit(void)
{
	SHPR3 |= 0xFFU << 24; //Set the priority of SysTick to be the weakest
	SHPR3 |= 0xFEU << 16; //shift the constant 0xFE 16 bits to set PendSV priority
	SHPR2 |= 0xFDU << 24; //set the priority of SVC higher than PendSV
    // Initialize TCBs
    for (U8 i = 0; i < MAX_TASKS; i++)
    {
        kernel_config.TCBS[i].tid = TID_DORMANT;
        kernel_config.TCBS[i].state = TASK_DORMANT;
        kernel_config.TCBS[i].SP = NULL;
        kernel_config.TCBS[i].p_stack_mem = NULL;
        kernel_config.TCBS[i].stack_size = 0x4000;
        kernel_config.TCBS[i].remaining_sleep_time = DEFAULT_SLEEP_TIME;
        kernel_config.TCBS[i].deadline = DEFAULT_DEADLINE;
        kernel_config.TCBS[i].remaining_time = DEFAULT_DEADLINE;
    }

    // Init other members
    kernel_config.num_running_tasks = 0;
    kernel_config.is_running = TRUE;
    kernel_config.running_task = TID_DORMANT;
    osNull_task();
}

task_t osGetTID (void)
{
	if(kernel_config.is_running == FALSE || kernel_config.running_task == TID_DORMANT)
	{
		return 0;
	}
	return kernel_config.running_task;
};

int osKernelStart(void){
	if ((kernel_config.is_running == 0) || (kernel_config.TCBS[1].state == DORMANT))
	{
		return RTX_ERR;
	}
	else
	{
		kernel_config.is_running = TRUE;
		// Get the first task and run it 
		task_t firstTask = scheduler();
		kernel_config.running_task = firstTask;
		__set_PSP((U32)kernel_config.TCBS[kernel_config.running_task].SP);
		kernel_config.TCBS[kernel_config.running_task].state = RUNNING;
		HAL_Init();
		// Calls os_kernel_start (lab1.s) which restores context of scheduled task.
		__asm("SVC #1");
	}
	return RTX_OK;
}

void osYield(void){
	if(kernel_config.is_running == FALSE || kernel_config.running_task == TID_DORMANT)
	{
		return;
	}

	// Reset a task’s time remaining back to its deadline.
	kernel_config.TCBS[kernel_config.running_task].remaining_time = kernel_config.TCBS[kernel_config.running_task].deadline;

	// Call PendSV to save state and restore state of new task.
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	__asm("isb");

	return;
}

int osTaskExit(void)
{
	if(kernel_config.is_running == FALSE || kernel_config.running_task == TID_DORMANT){
		return RTX_ERR;
	}

	task_t current_tid = osGetTID();
	if(current_tid == TID_DORMANT){
		return RTX_ERR;
	}
	// Deallocate task's stack.
	if(k_mem_dealloc(kernel_config.TCBS[current_tid].p_stack_mem )== -1)
	{
		return RTX_ERR;
	}

	//set current task to dormant
	kernel_config.TCBS[current_tid].state = DORMANT;
	kernel_config.TCBS[current_tid].tid = TID_DORMANT;
	kernel_config.num_running_tasks--;

	ContextSwitch();

	return RTX_OK;
}

// Default task is a deadline task with 5 ms deadline.
int osCreateTask(TCB* task){
	return osCreateDeadlineTask(5, task);
}

int osTaskInfo(task_t TID, TCB *task_copy) {
	if(kernel_config.num_running_tasks <= 1){
		return RTX_ERR;
	}
	// Copy TCB into task copy
	for (int i = 1; i < MAX_TASKS; i++) {
		if (kernel_config.TCBS[i].tid == TID) {
			memacopy(task_copy, kernel_config.TCBS + i, sizeof(TCB));
			return RTX_OK;
		}
	}

	return RTX_ERR;
}

int osSetDeadline(int deadline, task_t TID){
	// Since changing a deadline must be done atomically interrupts are disabled
    __disable_irq();
	// error
	if( deadline <= 0 || TID <= 0 || TID >= MAX_TASKS || kernel_config.TCBS[TID].tid == -1 || TID == kernel_config.running_task)
	{
        __enable_irq();
		return RTX_ERR;
	}

	// Update deadline and related fields
	kernel_config.TCBS[TID].deadline = deadline;
	kernel_config.TCBS[TID].remaining_time = deadline;
	U32 current_task_remaing_time =  kernel_config.TCBS[kernel_config.running_task].remaining_time;
	// Context switch if new deadline is less than the running task.
	if (kernel_config.TCBS[TID].remaining_time < current_task_remaing_time)
	{
        __enable_irq();
		ContextSwitch();
	}else{
        __enable_irq();
	}

	return RTX_OK;
}

int osCreateDeadlineTask(int deadline, TCB* task){
	if(kernel_config.num_running_tasks >= MAX_TASKS || deadline <= 0 || task == NULL || task->ptask == NULL || task->stack_size < STACK_SIZE){
		return RTX_ERR;
	}

	// Find available TID
	int create_tid = -1;
	for(int i =1; i < MAX_TASKS; i++){
		if(kernel_config.TCBS[i].tid == TID_DORMANT){
			create_tid = i;
			break;
		}
	}
	if(create_tid == -1){
		return RTX_ERR;
	}

	// Create a new TCB and populate with values in task
	TCB* create_tcb = &kernel_config.TCBS[create_tid];
	task->tid = create_tid;
	*create_tcb= *task;
	create_tcb->tid = create_tid;

	create_tcb->stack_size=task->stack_size;
	create_tcb->ptask = task->ptask;
	create_tcb->p_stack_mem = (U32*)(k_mem_alloc(create_tcb->stack_size));
	// Memory cannot be allocated for the new task
	if(create_tcb->p_stack_mem == NULL){
		task->tid = -1;
		return RTX_ERR;
	}

	// Transfer ownership of memory to new task.
	transfer_memory(create_tcb->p_stack_mem, create_tcb->tid);
	create_tcb->SP = (U32*)((U32)create_tcb->p_stack_mem + (U32)create_tcb->stack_size);
	create_tcb->stack_high = (U32)create_tcb->SP;

	// Initialize the stack for the task
	U32* stackptr = (U32*)create_tcb->SP;

	// Set up initial stack frame
	*(--stackptr) = 1 << 24;                // xPSR, setting Thumb mode
	*(--stackptr) = task->ptask;      		// PC, function address
	for (int i = 0; i < 14; i++) {
		*(--stackptr) = 0xA;
	}

	create_tcb->SP = stackptr;

	// Copy the initialized TCB back to the provided task structure
	kernel_config.num_running_tasks++;
	create_tcb->state = TASK_READY;
	create_tcb->tid = create_tid;
	create_tcb->deadline = deadline;
	create_tcb->remaining_time = deadline;
	create_tcb->remaining_sleep_time = DEFAULT_SLEEP_TIME;

	// Schedule newly created task if it has shorter time slice.
	U32 current_task_remaing_time =  kernel_config.TCBS[kernel_config.running_task].remaining_time;

	if ((create_tcb->remaining_time < current_task_remaing_time) && (kernel_config.running_task != TID_DORMANT))
	{
		ContextSwitch();
	}

	return RTX_OK;
}

void osSleep(int timeInMs)
{
	if (timeInMs <= 0){
		return;
	}
	//set state to sleeping
	kernel_config.TCBS[kernel_config.running_task].state = SLEEPING;

	//set sleep time
	kernel_config.TCBS[kernel_config.running_task].remaining_sleep_time = (U32) timeInMs;

	ContextSwitch();

	return;
}

void osPeriodYield(){
	// because this is a periodic task which cant be scheduled till its period is over we set its state to sleeping
	kernel_config.TCBS[kernel_config.running_task].state = SLEEPING;
	// so out remaining sleeptime is actually just the remaining task time for this task
	kernel_config.TCBS[kernel_config.running_task].remaining_sleep_time = kernel_config.TCBS[kernel_config.running_task].remaining_time;

	ContextSwitch();
}


