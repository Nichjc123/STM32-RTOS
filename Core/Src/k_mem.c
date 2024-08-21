#include "k_mem.h"
#include <stdio.h>
#include "stm32f4xx.h"
#include <math.h>
#include "k_task.h"

extern uint32_t _estack;
extern uint32_t _Min_Stack_Size;
extern uint32_t _img_end;


/************************************************
 *               GLOBALS
 ************************************************/

U32 init_called    = 0;   // Initialization flag
U8 bitarray[2047]  = {0}; // Buddy system bit array
metadata *free_list[11];  // Linked list of free blocks
U32 heap_start;           // Heap starting address

/************************************************
 *               HELPER FUNCTIONS
 ************************************************/

unsigned int integer_log2(unsigned int num)
{
	if (num == 0)
		return 0; // Handle the edge case for 0
	return 31 - __builtin_clz(num);
}

/*
 *  Getters
*/

// Return parent index.
static inline U16 get_parent(U16 index)
{
	return (index - 1) >> 1;
}

// Return left child index.
static inline U16 get_left_child(U16 index)
{
	return index << 1;
}

// Return right child index.
static inline U16 get_right_child(U16 index)
{
	return (1 << index) + 1;
}

static inline U16 get_buddy(const U16 index)
{
	U8 level;
	U16 level_pos;
	index_to_level_and_pos(index, &level, &level_pos);

	return ((1 << level) - 1) + (level_pos ^ 1);
}

// Return buddy index given a level (tree level) and index within that level (level_pos)
static inline U16 get_buddy_level_level_pos(const U8 level, const U16 level_pos)
{
	return ((1 << level) - 1) + (level_pos ^ 1);
}

/*
 * Bit array <----> address converters
*/

// Return bitarray index and level given memory address of a block.
static U16 addr_to_index(const U32 *const ptr, U8 *level_out)
{
	// Find the index for the bottom level node (child of desired node)
	U16 bottom_level_offset = ((U32)ptr - heap_start) / (U32)(1 << 5);
	U16 bottom_level_index = (1 << 10) - 1 + bottom_level_offset;

	// Move up the heap until allocated node is found
	U8 level = 10;
	U16 bitarray_index = bottom_level_index;

	while (bitarray[bitarray_index] == 0)
	{
		level--;
		bitarray_index = get_parent(bitarray_index);
	}

	*level_out = level;

	return bitarray_index;
}

// Return level given index.
static inline U8 index_to_level(const U16 index)
{
	return (U8)(integer_log2(index + 1));
}

// Return level offset given index.
static U16 index_to_level_pos(const U16 index)
{
	U8 level = index_to_level(index);
	return (U16)(index - (1 << level) + 1);
}

// Return memory address given index.
static U32 index_to_addr(const U16 index)
{
	U8 level;
	U16 level_pos;

	index_to_level_and_pos(index, &level, &level_pos);

	return heap_start + (U32)(1 << (MAX_LEVEL - level)) * level_pos;
}

// Return memory address and buddy address without using helper given bitarray index.
void fast_index_to_addr(const U16 index, U32 **addr, U32 **buddy_addr)
{
	U32 level = integer_log2(index + 1);
	U32 level_pos = (index - (1 << level) + 1);

	U32 level_offset = 1 << (MAX_LEVEL - level);
	*addr = heap_start + level_offset * level_pos;
	*buddy_addr = heap_start + level_offset * (level_pos + 1);
}

// Return level offset given the index and level for the bitarray.
static inline U16 index_level_to_level_pos(const U16 index, const U8 level)
{
	return (U16)(index - (1 << level) + 1);
}

// Return level and level offset given bit array index.
static void index_to_level_and_pos(const U16 index, U8 *level, U16 *level_pos)
{
	*level = index_to_level(index);
	*level_pos = (U16)(index - (1 << *level) + 1);
}

/*
 * Helper function for the allocation function. If a node of the correct size is not available,
 * this function will split a larger node.
 * @param parent: node to be split.
*/
static void split_node(metadata *parent, int lvl)
{
	//Getting indicies and level positions
	int parent_index = (1 << lvl) + parent->level_pos - 1;
	int parent_pos = parent->level_pos;
	int child_index = (parent_index << 1) + 1;

	// Set the parent and child nodes to 1 (signifies filled/partially filled).
	bitarray[parent_index] = 1;
	bitarray[child_index] = 1;

	// Remove the node we are splitting from the free list.
	free_list[lvl] = free_list[lvl]->next;
	
	// Get 2 children nodes
	metadata *child;
	metadata *child_buddy;
	fast_index_to_addr(child_index, &child, &child_buddy);

	// Update the children's metadata.
	child->level_pos = parent_pos << 1;
	child_buddy->level_pos = child->level_pos + 1;

	child_buddy->next = NULL;
	child->next = child_buddy;

	child->prev = NULL;
	child_buddy->prev = child;

	// Update the free list one level below our newly split node to point to the child.
	free_list[lvl + 1] = child;
}

// Return block size for a given level.
static int level_to_block_size(int level)
{
	return (1 << (MAX_LEVEL - level)) + sizeof(metadata);
}

// Return memory address given the level and offset of a memory block.
static inline U32 level_level_pos_to_addr(const U8 level, const U16 level_pos)
{
	return heap_start + (U32)(1 << (MAX_LEVEL - level)) * level_pos;
}

/************************************************
 *               FUNCTIONS
 ************************************************/

int k_mem_init()
{
	// Init heap address
	heap_start = &_img_end;

	// Return error if init already called
	if (init_called == 1 || kernel_config.is_running == FALSE)
	{
		return RTX_ERR;
	}
	init_called = 1;

	// Init root node of memory
	metadata *head = index_to_addr(0);
	free_list[0] = head;

	head->next = NULL;
	head->prev = NULL;
	head->level_pos = 0;

	// Setting all other free lists to null.
	for (int i = 1; i < 11; i++)
	{
		free_list[i] = NULL;
	}
	
	return RTX_OK;
}

void *k_mem_alloc(size_t size)
{
	// Check to make sure init called and size are greater than 0
	if (init_called == 0 || size == 0)
	{
		return NULL;
	}

	int lvl = MAX_LEVEL;                                 // level variable 
	int size_of_block_at_lvl = level_to_block_size(lvl); // size of a block at a level of lvl

	// Find the level that can accomodate an allocation of given size.
	while ((size_of_block_at_lvl < size) && (lvl > 0))
	{
		lvl--;
		size_of_block_at_lvl = level_to_block_size(lvl);
	}

	// If level is 0 we could not find a block large enough to fulfill the allocation. 
	if (lvl == 0)
	{
		return NULL;
	}

	// If there is a free node at lvl, allocate it.  
	if (free_list[lvl] != NULL)
	{
		int pos = (1 << lvl) + (free_list[lvl]->level_pos) - 1;
		bitarray[pos] = 1;
	}
	else
	{
		// Now we must split a larger node to satisfy the request.

		// Finding the level which has a free node we can split
		int free_node_lvl = lvl;
		while ((free_node_lvl >= 0) && (free_list[free_node_lvl] == NULL))
		{
			free_node_lvl--;
		}

		// No available block large enough
		if (free_node_lvl < 0)
		{
			return NULL;
		}

		// Split the free node down to our desired level
		for (free_node_lvl; free_node_lvl < lvl; free_node_lvl++)
		{
			split_node(free_list[free_node_lvl], free_node_lvl);
		}
	}

	U8 *base_address = (U8 *)free_list[lvl];

	// Remove the node we just allocated from the free list
	free_list[lvl] = free_list[lvl]->next;

	// Initialize the metadata for the newly allocated node.
	metadata *meta = (metadata *)base_address;
	meta->task_tid = osGetTID();
	meta->secret_key = METADATA_SECRET_KEY;
	meta->is_allocated = 1;

	return base_address + sizeof(metadata);
}

void transfer_memory(void *ptr, task_t tid) {
	U8 *mem_addr = (U8 *)ptr;
	metadata *meta = (metadata *)(mem_addr - sizeof(metadata));

	meta->task_tid = tid;
}

int k_mem_dealloc(void *ptr)
{
	if (init_called == 0) // Check to make sure init called and size are greater than 0
	{
		return RTX_ERR;
	}

	U8 *mem_addr_p = (U8 *)ptr;

	if (ptr == NULL)
	{
		// printf("NULL pointer\r\n");
		return RTX_ERR;
	}

	/**
	 *  Check validity of pointer (random/invalid so check for metadata key value)
	 */
	metadata *block_metadata_p = (metadata *)(mem_addr_p - sizeof(metadata));
	// printf("block_metadata: secret_key: %d, is_allocated: %d, task_tid: %d, level_pos: %d, next: %p, prev: %p\r\n", block_metadata_p->secret_key, block_metadata_p->is_allocated, block_metadata_p->task_tid, block_metadata_p->level_pos, block_metadata_p->next, block_metadata_p->prev);

	if (block_metadata_p->secret_key != 0b10011001)
	{
		// printf("Invalid pointer\r\n");
		return RTX_ERR;
	}

	/*
	 *  Check that memory that ptr points to is allocated
	 */
	if (block_metadata_p->is_allocated == 0)
	{
		// printf("Memory not allocated\r\n");
		return RTX_ERR;
	}

	/*
	 *  Check that current running task owns block
	 */
	if (block_metadata_p->task_tid != osGetTID())
	{
		// printf("Task does not own block\r\n");
		return RTX_ERR;
	}

	/****************************************************
	 *                  DEALLOCATE
	 ****************************************************/

	/*
	 *  Finding block (start at addr block and move up to parent) (Slide 30)
	 */
	U8 level = 10;
	U16 bitarray_index = addr_to_index(mem_addr_p, &level);

	/**************************
	 *  Coalescing algorithm
	 **************************/

	U8 current_level = level;
	U16 current_level_pos = index_level_to_level_pos(bitarray_index, current_level);

	U16 buddy_index = get_buddy_level_level_pos(current_level, current_level_pos);
	U8 finish_coalescing = 0;

	do
	{
		// set current node to 0
		bitarray[bitarray_index] = 0;
		metadata *current_node = (metadata *)level_level_pos_to_addr(current_level, current_level_pos);

		// update current node metadata
		current_node->is_allocated = 0;
		current_node->level_pos = current_level_pos; // What is this for
		current_node->next = NULL;
		current_node->prev = NULL;

		if (bitarray[buddy_index] != 0 || bitarray_index == 0)
		{
			// if buddy is 1 (or we are root) we add ourselves to free list (AND END ALGORITHM)
			metadata *prev_head = free_list[current_level];

			current_node->next = prev_head;
			current_node->prev = NULL;

			if (prev_head != NULL)
				prev_head->prev = current_node;

			// printf("prev head:%p, curr: %p\r\n",prev_head,current_node);

			free_list[current_level] = current_node;
			return RTX_OK;
			finish_coalescing = 1;
		}
		else if (bitarray[buddy_index] == 0)
		{
			// if buddy is 0 remove it from free list (and we are not root)
			// figure out if left or right child if left child we set abvove lvl to us.

			metadata *free_buddy = (metadata *)level_level_pos_to_addr(current_level, current_level_pos ^ 1);
			if (free_list[current_level] == free_buddy)
			{
				free_list[current_level] = free_buddy->next;
			}
			if (free_buddy->next != NULL)
			{
				free_buddy->next->prev = free_buddy->prev;
			}
			if (free_buddy->prev != NULL)
			{
				free_buddy->prev->next = free_buddy->next;
			}
		}

		current_level--;
		current_level_pos = current_level_pos >> 1;

		// update current node to parent
		bitarray_index = get_parent(bitarray_index);
		buddy_index = get_buddy_level_level_pos(current_level, current_level_pos);

	} while (!finish_coalescing);

	return RTX_OK;
}

int k_mem_count_extfrag(size_t size)
{
	// Check to make sure init called and size are greater than 0
	if ((init_called == 0) || (size == 0) || (size <= 32))
	{
		return 0;
	}

	int max_level = MAX_LEVEL;                           // level variable 
	int size_of_block_at_lvl = level_to_block_size(lvl); // size of a block at a level of lvl

	// Find the level that can accomodate an allocation of given size.
	while ((size_of_block_at_lvl < size) && (lvl > 0))
	{
		lvl--;
		size_of_block_at_lvl = level_to_block_size(lvl);
	}

	int count = 0;

	// Increment the count for the levels that could accomodate the size.
	for (int lvl = max_level; lvl <= MAX_LEVEL; ++lvl)
	{
		if (free_list[i] != NULL)
		{
			count++;
		}
	}
	return count;
}
