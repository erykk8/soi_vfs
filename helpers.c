#include "VFS.h"
#include "helpers.h"

// returns whether block i is free or not according to given block_usage_map
int is_free_bitmap_block(int i, void* block_usage_map)
{
	bitmap b = (bitmap)block_usage_map;
	return !(BITTEST(b, i));
}

// returns whether i-th filedescriptor is free or not in given rootdir 
int is_free_rootdir_block(int i, void* rootdir)
{
	struct VFS_filedescriptor* fd = (struct VFS_filedescriptor*)rootdir;
	return fd[i].filename[0] == 0;
}

void print_addr_size_type(size_type addr, size_type size, int is_free, char* type)
{
	printf("%d\tsize %12d\t", addr, size);
	if(is_free)
	{
		printf("free %s blocks\n", type);
	}
	else
	{
		printf("occupied %s blocks\n", type);
	}
}

size_type iterate_block_structure(int (*is_free_condition)(int, void*), void* structure, size_type max_loop, size_type addr_offset, char* block_type)
{
	char is_free_space = 1;
	char is_free_block = 1;
	size_type i=0;
	size_type curr_block_size=0;
	size_type curr_block_addr=0;
	
	// first block
	is_free_block = (*is_free_condition)(0, structure);
	is_free_space = is_free_block;
	curr_block_addr = addr_offset+i;
	curr_block_size = 1;
			
	for(i = 1; i < max_loop; ++i)
	{
		is_free_block = (*is_free_condition)(i, structure);
		// still the same block group
		if(is_free_block == is_free_space)
		{
			curr_block_size++;
		}
		// new block group starts so time to display the now ended group
		else
		{
			// print group info
			print_addr_size_type(curr_block_addr, curr_block_size, is_free_space, block_type);
			// start new block group
			is_free_space = is_free_block;
			curr_block_addr = addr_offset+i;
			curr_block_size = 1;
		}
	}
	// print last block group info
	print_addr_size_type(curr_block_addr, curr_block_size, is_free_block, block_type);
	// return global addr of last block + 1
	return curr_block_addr + curr_block_size;
}
