#ifndef SOI_VFS_H_
#define SOI_VFS_H_

#include <stdio.h> // file type
#include "types.h"

//#define BLOCK_SIZE (4096+sizeof(block_index_type)) // 4KB + 4 bytes
#define BLOCK_SIZE 4096
#define FILENAME_LENGTH (31 - sizeof(block_index_type) - sizeof(size_type)) // actual length is this + 1
#define FILE_DATA_CHUNK_SIZE (BLOCK_SIZE - sizeof(block_index_type))


// DISK STRUCTURE:
// [ superblock ] [ disk usage bitmap ] [ data blocks ]
// superblock is always 1 block
// the disk usage bitmap's size depends on the amount of data blocks (disk size) which is BITNSLOTS(amount of blocks)
// data blocks are as many as set in the superblock : superblock.size



// size = 1 block
struct VFS_superblock
{
	size_type n_datablocks;
};

// size = 1 block
struct VFS_linked_datablock
{
	char data[FILE_DATA_CHUNK_SIZE];
	block_index_type next_block; // next_block == superblock.size <=> no next block to read
};


// size = 32 bytes
struct VFS_filedescriptor
{
	char filename[FILENAME_LENGTH+1];
	size_type size;
	block_index_type first_block;
};

#define MAX_FILECOUNT (16*BLOCK_SIZE/sizeof(struct VFS_filedescriptor)) // 2050 with BLOCK_SIZE = 4096+4

// size = 16 blocks
struct VFS_rootdir
{
	struct VFS_filedescriptor files[MAX_FILECOUNT];
};

// funcs
int open_and_read_VFS(char* filename, struct VFS_superblock* sblock, struct VFS_rootdir* rdir, bitmap* fu_map, FILE** fd, size_type* d_fs);
int create_VFS(char* filename, size_type size_in_bytes);
int add_file_to_VFS(char* disk_filename, char* add_filename, char* filename_on_disk);
int get_file_from_VFS(char* disk_filename, char* filename_on_disk, char* filename_local);
int remove_file_from_VFS(char* disk_filename, char* filename_on_disk);
int read_VFS_info(char* filename);
int list_VFS_rootdir(char* filename);


#endif
