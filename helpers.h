#ifndef SOI_VFS_HELPERS_H_
#define SOI_VFS_HELPERS_H_
#include "VFS.h"
#include <stdio.h>


// returns whether block i is free or not according to given block_usage_map
int is_free_bitmap_block(int i, void* block_usage_map);

// returns whether i-th filedescriptor is free or not in given rootdir 
int is_free_rootdir_block(int i, void* rootdir);

// helper for iterate_block_structure
// prints block group info
void print_addr_size_type(size_type addr, size_type size, int is_free, char* type);

// iterates over a block structure, groups into contiguous free/occupied blocks and prints
// information for each block group
// must provide an is_free_condition function proper for given structure type!
size_type iterate_block_structure(int (*is_free_condition)(int, void*), void* structure, size_type max_loop, size_type addr_offset, char* block_type);
#endif
