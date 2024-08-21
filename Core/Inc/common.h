/*
 * common.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 */

#ifndef INC_COMMON_H_
#define INC_COMMON_H_


#define MAX_TASKS       16    //maximum number of tasks in the system

// Bool alternatives
#define TRUE            1
#define FALSE           0

// Special TIDs
#define TID_NULL        0     //predefined Task ID for the NULL task
#define TID_KERNEL      -1
#define TID_DORMANT     -1

// Stack related globals
#define STACK_SIZE      0x200 //min. size of each task’s stack
#define MAIN_STACK_SIZE 0x400

// Task States
#define DORMANT         0     //state of terminated task
#define READY           1     //state of task that can be scheduled but is not running
#define RUNNING         2     //state of running task
#define SLEEPING        3     //state of sleeping task

// Return Codes
#define RTX_ERR         -1
#define RTX_OK 		    1

#endif /* INC_COMMON_H_ */

void memacopy(void *dest, void *src, int size);
