/**
 * @file k_mem.h
 * @author Nicholas Cantone
 * @date July 2024
 * @brief Memory management header.
 */

#ifndef INC_K_MEM_H_
#define INC_K_MEM_H_

/************************************************
 *               INCLUDES
 ************************************************/

#include "common.h"
#include "k_task.h"

/************************************************
 *               DEFINITIONS
 ************************************************/

#define METADATA_SECRET_KEY      0b10011001 // Used to verify validity of pointer provided for deallocation
#define MAX_LEVEL                15         // Exponent for max size, (2^15)

/************************************************
 *               TYPEDEFS
 ************************************************/

// used for specifying size of memory blocks
typedef unsigned int size_t;

typedef struct block_metadata {
	U8 secret_key;                 // used for checking validity in deallocation
	U8 is_allocated;               
	U32 task_tid;
	U16 level_pos;                 // index of memory block in it's level
	struct block_metadata* next;
	struct block_metadata* prev;
	U32 dummy; // 8 byte alignment value
} metadata;


/************************************************
 *              FUNCTION DEFS
 ************************************************/

/*
 * @brief: Initializes the RTX’s memory manager. Including locating the heap and writing the
 *	       initial metadata required to start searching for free regions
 *
 * @return: Returns RTX_OK on success and RTX_ERR if called more than once.
*/
int k_mem_init();

/*
 * @brief: Allocates size bytes according to the Buddy algorithm.
 *
 * @param: size: size of region to be allocated, in bytes.
 * @return: Returns a pointer to allocated memory or NULL if the request fails.
 */
void* k_mem_alloc(size_t size);

/*
 * @brief: Frees the memory pointed to by ptr as long as the currently running task is
 *         the owner of that block and as long as the memory pointed to by ptr is in fact allocated.
 * @param ptr: pointer to memory location to free.
 * @return: returns RTX_OK on success and RTX_ERR on failure.
 */
int k_mem_dealloc(void* ptr);

/*
 * @brief: This function counts the number of free memory regions that are strictly less than size
 *         bytes.
 *
 * @param: size: size of region to be counted in bytes.
 * @return: Returns the number of free memory regions.
 */
int k_mem_count_extfrag(size_t size);


/*
 * @brief: Transfer ownership of a region of memory to TID.
 *
 * @param: tid: ID of new owner.
 * @param: ptr: pointer to region of memory that will be transferred.
 * @return: Returns the number of free memory regions.
 */
void transfer_memory(void *ptr, task_t tid);

#endif /* INC_K_MEM_H_ */
