#include <stdlib.h>
#include <stdio.h> // printf + file stuff
#include <libgen.h> // path basename
#include <errno.h> // errno
#include <unistd.h> // truncate
#include <sys/types.h> // truncate
#include <string.h> // memcpy

#include "VFS.h"
#include "helpers.h"

// [superblock(1)][rootdir - file descriptors (16)][data block usage bitmap (varied)][data (varied)]

#define BLOCKS_NEEDED_FOR_BYTES(nb) ((((nb) - 1)/BLOCK_SIZE) + 1)
#define FILE_CHUNKS_NEEDED_FOR_BYTES(nb) (((nb) - 1)/FILE_DATA_CHUNK_SIZE + 1)
#define SUPERBLOCK_OFFSET 0

#define ROOTDIR_BLOCK_OFFSET 1
#define ROOTDIR_BYTE_OFFSET ROOTDIR_BLOCK_OFFSET*BLOCK_SIZE
#define ROOTDIR_SIZE_BLOCKS (BLOCKS_NEEDED_FOR_BYTES(sizeof(struct VFS_rootdir)))

#define BITMAP_BYTE_OFFSET (ROOTDIR_BYTE_OFFSET+sizeof(struct VFS_rootdir))
#define BITMAP_BLOCK_OFFSET (ROOTDIR_BLOCK_OFFSET + ROOTDIR_SIZE_BLOCKS)
#define BITMAP_SIZE_ACTUAL_BYTES(ndblocks) BITNSLOTS(ndblocks)
#define BITMAP_SIZE_BLOCKS(ndblocks) BLOCKS_NEEDED_FOR_BYTES(BITMAP_SIZE_ACTUAL_BYTES(ndblocks))
#define BITMAP_SIZE_DISK_BYTES(ndblocks) BITMAP_SIZE_BLOCKS(ndblocks)*BLOCK_SIZE

#define DATA_BYTE_OFFSET(ndblocks) (BITMAP_BYTE_OFFSET + BITMAP_SIZE_DISK_BYTES(ndblocks))
#define DATA_BLOCK_OFFSET(ndblocks) (BITMAP_BLOCK_OFFSET + BITMAP_SIZE_BLOCKS(ndblocks))
#define USABLE_DATA_SIZE_BYTES(ndblocks) ((ndblocks)*FILE_DATA_CHUNK_SIZE)
#define DATA_SIZE_DISK_BYTES(ndblocks) (ndblocks*BLOCK_SIZE)

#define DATA_ITH_BLOCK_OFFSET(block_nr, ndblocks) (DATA_BYTE_OFFSET(ndblocks) + block_nr*BLOCK_SIZE)

#define LAST_FILE_CHUNK_SIZE(filesize, blocks_taken) (FILE_DATA_CHUNK_SIZE*(1 - blocks_taken) + filesize)

#define PREDICTED_VFS_FILE_SIZE(ndblocks) (DATA_BYTE_OFFSET(ndblocks) + DATA_SIZE_DISK_BYTES(ndblocks))

int open_and_read_VFS(char* filename, struct VFS_superblock* sblock, struct VFS_rootdir* rdir, bitmap* fu_map, FILE** fd, size_type* d_fs)
{
	FILE* disk_file = fopen(filename, "r+b");;
	struct VFS_superblock superblock;
	struct VFS_rootdir rootdir;
	bitmap free_space_map;
	size_type disk_file_size;
	
	
	if(!disk_file)
	{
		printf("Could not open VFS file, error %d\n", errno);
		return 1;
	}
	
	// get filesize
	fseek(disk_file, 0, SEEK_END);
	disk_file_size = ftell(disk_file);
	rewind(disk_file);
	
	// read superblock
	fseek(disk_file, SUPERBLOCK_OFFSET, SEEK_SET);
	if(fread(&superblock, sizeof(struct VFS_superblock), 1, disk_file) != 1)
	{
		printf("Could not read superblock\n");
		return 2;
	}
	
	// check if filesize and size predicted from n_datablocks match
	if(disk_file_size != PREDICTED_VFS_FILE_SIZE(superblock.n_datablocks))
	{
		printf("VFS file size and expected disk size mismatch\n");
		return 3;
	}
	
	// read root directory
	fseek(disk_file, ROOTDIR_BYTE_OFFSET, SEEK_SET);
	if(fread(&rootdir, sizeof(rootdir), 1, disk_file) != 1)
	{
		printf("Could not read root directory\n");
		return 4;
	}
	
	//prepare and read bitmap
	if(fu_map)
	{
		free_space_map = (bitmap)malloc(BITMAP_SIZE_ACTUAL_BYTES(superblock.n_datablocks));
		fseek(disk_file, BITMAP_BYTE_OFFSET, SEEK_SET);
		if(fread(free_space_map, BITMAP_SIZE_ACTUAL_BYTES(superblock.n_datablocks), 1, disk_file) != 1)
		{
			printf("Could not read free space bitmap\n");
			free(free_space_map);
			return 5;
		}
	}
	
	*sblock = superblock;
	*rdir = rootdir;
	if(fu_map) *fu_map = free_space_map;
	*fd = disk_file;
	if(d_fs) *d_fs = disk_file_size;
	
	return 0;
}

int create_VFS(char* filename, size_type size_in_bytes)
{
	FILE* disk_file = fopen(filename, "wb");
	struct VFS_superblock superblock;
	
	if(!disk_file)
	{
		printf("Could not open file, error %d\n", errno);
		return 1;
	}
	
	superblock.n_datablocks = FILE_CHUNKS_NEEDED_FOR_BYTES(size_in_bytes);
	
	fwrite(&superblock, sizeof(superblock), 1, disk_file);
	
	fclose(disk_file);
	truncate(filename, PREDICTED_VFS_FILE_SIZE(superblock.n_datablocks));
	
	printf("Successfully created VFS in file %s\n", filename);
	
	return 0;
}

size_type find_free_file_descriptor(struct VFS_rootdir* rootdir, char* filename_on_disk)
{
	size_type fd_nr, i;
	int fd_flag = 0;
	for(i = 0; i < MAX_FILECOUNT; ++i)
	{
		if(!fd_flag && rootdir->files[i].filename[0] == 0)
		{
			fd_nr = i;
			fd_flag = 1;
		}
		if(strncmp(rootdir->files[i].filename, filename_on_disk, FILENAME_LENGTH) == 0)
		{
			printf("Can't add file, a file named %s already exists on disk\n", filename_on_disk);
			return MAX_FILECOUNT;
		}
	}
	if(!fd_flag)
	{
		printf("Can't add file, no file descriptor available\n");
		return MAX_FILECOUNT;
	}
	return fd_nr;
}

size_type find_file_descriptor(struct VFS_rootdir* rootdir, char* filename_on_disk)
{
	size_type i;
	for(i = 0; i < MAX_FILECOUNT; ++i)
	{
		if(strncmp(rootdir->files[i].filename, filename_on_disk, FILENAME_LENGTH) == 0)
		{
			return i;
		}
	}
	printf("Couldn't find file descriptor for given file\n");
	return MAX_FILECOUNT;
}

void add_file_error_cleanup(char* error, FILE* f1, FILE* f2, bitmap bmap, block_index_type* ublocks)
{
	printf("%s\n", error);
	fclose(f1);
	fclose(f2);
	free(bmap);
	free(ublocks);
}

int add_file_to_VFS(char* disk_filename, char* add_filename, char* filename_on_disk)
{
	FILE *disk_file, *add_file = fopen(add_filename, "rb");
	struct VFS_superblock superblock;
	struct VFS_rootdir rootdir;
	struct VFS_linked_datablock datablock;
	bitmap free_space_map;
	size_type disk_file_size, add_file_size, blocks_needed, last_chunk_size;
	block_index_type* used_blocks;
	block_index_type prev_used_block;
	size_type fd_nr, read_size;
	int r;
	unsigned int i, j;
	
	if(!add_file)
	{
		printf("Could not open file to add to VFS, error %d\n", errno);
		return 1;
	}
	
	// get filesize
	fseek(add_file, 0, SEEK_END);
	add_file_size = ftell(add_file);
	rewind(add_file);
	
	// how many blocks needed to fit file
	if(add_file_size == 0) blocks_needed = 0;
	else blocks_needed = FILE_CHUNKS_NEEDED_FOR_BYTES(add_file_size);
	if(blocks_needed == 0) last_chunk_size = 0;
	else last_chunk_size = LAST_FILE_CHUNK_SIZE(add_file_size, blocks_needed);
	
	// prepare array of block indices that will be our used blocks
	used_blocks = (block_index_type*)malloc(blocks_needed*sizeof(block_index_type));
	
	// open VFS
	r = open_and_read_VFS(disk_filename, &superblock, &rootdir, &free_space_map, &disk_file, &disk_file_size);
	if(r != 0)
	{
		fclose(add_file);
		return r;
	}
	
	// find free file descriptor if file of the same name doesnt already exist
	fd_nr = find_free_file_descriptor(&rootdir, filename_on_disk);
	
	// can't add
	if(fd_nr == MAX_FILECOUNT)
	{
		add_file_error_cleanup("", add_file, disk_file, free_space_map, used_blocks);
		return 10;
	}
	
	
	// try to find enough free blocks
	prev_used_block = 0;
	for (j = 0; j < blocks_needed; ++j)
	{
		for (i = prev_used_block; i < superblock.n_datablocks && BITTEST(free_space_map, i); ++i);
		if(i == superblock.n_datablocks)
		{
			add_file_error_cleanup("Can't add file, not enough free blocks", add_file, disk_file, free_space_map, used_blocks);
			return 11;
		}
		prev_used_block = i+1;
		used_blocks[j] = i;
	}
	
	// set blocks to used in bitmap
	for (j = 0; j < blocks_needed; ++j)
	{
		//printf("Setting block nr %d to be used\n", used_blocks[j]);
		BITSET(free_space_map, used_blocks[j]);
	}
	
	// set file descriptor
	strncpy(&(rootdir.files[fd_nr].filename[0]), filename_on_disk, FILENAME_LENGTH);
	rootdir.files[fd_nr].size = add_file_size;
	rootdir.files[fd_nr].filename[FILENAME_LENGTH] = 0; // null terminate
	if(blocks_needed == 0) rootdir.files[fd_nr].first_block = superblock.n_datablocks;
	else rootdir.files[fd_nr].first_block = used_blocks[0];
	
	//printf("Set filename to %s\n", rootdir.files[fd_nr].filename);
	
	// write file descriptor and bitmap to disk
	fseek(disk_file, ROOTDIR_BYTE_OFFSET, SEEK_SET);
	if(fwrite(&rootdir, sizeof(rootdir), 1, disk_file) != 1)
	{
		add_file_error_cleanup("Couldn't write root directory to disk", add_file, disk_file, free_space_map, used_blocks);
		return 12;
	}
	fseek(disk_file, BITMAP_BYTE_OFFSET, SEEK_SET);
	if(fwrite(free_space_map, BITMAP_SIZE_ACTUAL_BYTES(superblock.n_datablocks), 1, disk_file)!=1)
	{
		add_file_error_cleanup("Couldn't write free space map to disk", add_file, disk_file, free_space_map, used_blocks);
		return 13;
	}
	
	// chop into blocks and write file to disk
	if(blocks_needed != 0)
	{
		// read first chunk of file
		if(blocks_needed == 1) read_size = add_file_size;
		else read_size = FILE_DATA_CHUNK_SIZE;
		if(fread(&(datablock.data[0]), read_size, 1, add_file) != 1)
		{
			add_file_error_cleanup("Couldn't read 1st chunk from file", add_file, disk_file, free_space_map, used_blocks);
			return 13;
		}
		fseek(disk_file, DATA_ITH_BLOCK_OFFSET(used_blocks[0], superblock.n_datablocks), SEEK_SET); // set disk pos to first block
		
		// further blocks
		for(j = 1; j < blocks_needed; ++j)
		{
			// write previous block
			datablock.next_block = used_blocks[j];
			if(fwrite(&datablock, BLOCK_SIZE, 1, disk_file) != 1)
			{
				add_file_error_cleanup("Couldn't write block to disk", add_file, disk_file, free_space_map, used_blocks);
				return 13;
			}
			
			// read current chunk
			if(j == blocks_needed-1 && last_chunk_size != 0) read_size = last_chunk_size;
			else read_size = FILE_DATA_CHUNK_SIZE;
			if(fread(&(datablock.data[0]), read_size, 1, add_file)!=1)
			{
				add_file_error_cleanup("Couldn't read chunk from file", add_file, disk_file, free_space_map, used_blocks);
				return 13;
			}
			fseek(disk_file, DATA_ITH_BLOCK_OFFSET(used_blocks[j], superblock.n_datablocks), SEEK_SET); // set disk pos to current block
		}
		
		// write last block
		datablock.next_block = superblock.n_datablocks;
		if(fwrite(&datablock, BLOCK_SIZE, 1, disk_file) != 1)
		{
			add_file_error_cleanup("Couldn't write last block", add_file, disk_file, free_space_map, used_blocks);
			return 13;
		}
	}
		
	fclose(add_file);
	fclose(disk_file);
	free(free_space_map);
	free(used_blocks);
	printf("Successfully added file %s to VFS as file %s\n", add_filename, filename_on_disk);
	return 0;
}

void get_file_error_cleanup(char* error, FILE* f1, FILE* f2)
{
	printf("%s\n", error);
	fclose(f1);
	fclose(f2);
}

int get_file_from_VFS(char* disk_filename, char* filename_on_disk, char* filename_local)
{
	FILE *disk_file, *get_file;
	struct VFS_superblock superblock;
	struct VFS_rootdir rootdir;
	struct VFS_linked_datablock datablock;
	size_type disk_file_size, fd_nr, blocks_taken, last_chunk_size, get_file_size, get_size;
	block_index_type curr_block;
	int r;
	unsigned int i;
	
	r = open_and_read_VFS(disk_filename, &superblock, &rootdir, NULL, &disk_file, &disk_file_size);
	if(r != 0) return r;
	
	// find file descriptor
	fd_nr = find_file_descriptor(&rootdir, filename_on_disk);
	if(fd_nr == MAX_FILECOUNT)
	{
		fclose(disk_file);
		return 10;
	}
	
	// open local file for writing
	get_file = fopen(filename_local, "wb");
	
	if(!get_file)
	{
		printf("Couldn't open local file for writing\n");
		fclose(disk_file);
		return 11;
	}
	
	// get chunks
	get_file_size = rootdir.files[fd_nr].size;
	curr_block = rootdir.files[fd_nr].first_block; // set first block to get chunk
	
	// calculate how many blocks it takes
	if(get_file_size == 0) blocks_taken = 0;
	else blocks_taken = FILE_CHUNKS_NEEDED_FOR_BYTES(get_file_size);
	if(blocks_taken == 0) last_chunk_size = 0;
	else last_chunk_size = LAST_FILE_CHUNK_SIZE(get_file_size, blocks_taken);
	
	// read the blocks
	for(i = 0; i < blocks_taken; ++i)
	{
		// find ith block on disk and read it
		fseek(disk_file, DATA_ITH_BLOCK_OFFSET(curr_block, superblock.n_datablocks), SEEK_SET);
		if(fread(&datablock, sizeof(datablock), 1, disk_file) != 1)
		{
			get_file_error_cleanup("Couldn't read block from disk", get_file, disk_file);
			return 12;
		}
		// write data to file
		if(i == blocks_taken-1 && last_chunk_size != 0) get_size = last_chunk_size;
		else get_size = FILE_DATA_CHUNK_SIZE;
		if(fwrite(&datablock, get_size, 1, get_file) != 1)
		{
			get_file_error_cleanup("Couldn't write file chunk to disk", get_file, disk_file);
			return 13;
		}
		curr_block = datablock.next_block; // set curr_block for next chunk
	}
	
	fclose(get_file);
	fclose(disk_file);
	printf("Successfully copied file %s from VFS to %s\n", filename_on_disk, filename_local);
	return 0;

}

int remove_file_from_VFS(char* disk_filename, char* filename_on_disk)
{
	FILE *disk_file;
	struct VFS_superblock superblock;
	struct VFS_rootdir rootdir;
	bitmap free_space_map;
	unsigned int i;
	int r;
	size_type fd_nr, file_size, blocks_needed;
	block_index_type* used_blocks;
	block_index_type curr_block, next_block;
	
	r = open_and_read_VFS(disk_filename, &superblock, &rootdir, &free_space_map, &disk_file, NULL);
	if(r != 0) return r;
	
	// find file 
	fd_nr = find_file_descriptor(&rootdir, filename_on_disk);
	
	if(fd_nr == MAX_FILECOUNT)
	{
		fclose(disk_file);
		return 10;
	}

	
	// how many blocks needed to fit file
	file_size = rootdir.files[fd_nr].size;
	if(file_size == 0) blocks_needed = 0;
	else blocks_needed = FILE_CHUNKS_NEEDED_FOR_BYTES(file_size);
	
	// find block indices if any blocks were allocated
	if(blocks_needed != 0)
	{
		curr_block = rootdir.files[fd_nr].first_block; // set first block
		// prepare array of block indices that will be our used blocks
		used_blocks = (block_index_type*)malloc(blocks_needed*sizeof(block_index_type));
		
		// collect the block indices
		for(i = 0; i < blocks_needed; ++i)
		{
			used_blocks[i] = curr_block;
			fseek(disk_file, DATA_ITH_BLOCK_OFFSET(curr_block, superblock.n_datablocks) + FILE_DATA_CHUNK_SIZE, SEEK_SET); // only load next block index
			fread(&curr_block, sizeof(curr_block), 1, disk_file);
		}
		// clear bits in bitmap
		for(i = 0; i < blocks_needed; ++i)
		{
			BITCLEAR(free_space_map, used_blocks[i]);
		}
		
		free(used_blocks);
		
		// write bitmap to disk
		fseek(disk_file, BITMAP_BYTE_OFFSET, SEEK_SET);
		fwrite(free_space_map, BITMAP_SIZE_ACTUAL_BYTES(superblock.n_datablocks), 1, disk_file);
	}
	
	// clear and write file descriptor
	strncpy(rootdir.files[fd_nr].filename, "", FILENAME_LENGTH+1); // set filename to 0's
	fseek(disk_file, ROOTDIR_BYTE_OFFSET, SEEK_SET);
	fwrite(&rootdir, sizeof(struct VFS_rootdir), 1, disk_file); // write all file descriptors
	
	fclose(disk_file);
	free(free_space_map);
	
	printf("Successfully removed file %s from VFS\n", filename_on_disk);
	
	return 0;
}


int list_VFS_rootdir(char* filename)
{
	FILE* disk_file;
	struct VFS_superblock superblock;
	struct VFS_rootdir rootdir;
	bitmap free_space_bitmap;
	size_type disk_file_size, space_occupied = 0, space_occupied_total = 0, space_available_total = 0, space_free = 0;
	unsigned int i;
	int has_files_flag = 0;
	int r = open_and_read_VFS(filename, &superblock, &rootdir, NULL, &disk_file, &disk_file_size);
	if(r != 0) return r;
	
	printf("================================\nVFS successfully opened.\n================================\n\n");
	printf("Block size: %d\n", BLOCK_SIZE);
	printf("User data block size: %d\n", FILE_DATA_CHUNK_SIZE);
	printf("VFS file size: %dKB\n", disk_file_size/1024);
	//printf("Usable disk size: %dKB\n", USABLE_DATA_SIZE_BYTES(superblock.n_datablocks)/1024);
	printf("Nr of data blocks: %d\n", superblock.n_datablocks);
	
	printf("\n--------Root directory listing:--------\n");
	
	for(i = 0; i < MAX_FILECOUNT; ++i)
	{
		if(rootdir.files[i].filename[0] != 0)
		{
			space_occupied = FILE_CHUNKS_NEEDED_FOR_BYTES(rootdir.files[i].size)*FILE_DATA_CHUNK_SIZE;
			space_occupied_total += space_occupied;
			has_files_flag = 1;
			printf("File: %s, ", rootdir.files[i].filename);
			printf("real size: %d bytes, ",  rootdir.files[i].size);
			printf("size on disk: %dKB\n", space_occupied/1024);
		}
	}
	
	space_available_total = USABLE_DATA_SIZE_BYTES(superblock.n_datablocks);
	space_free = (space_available_total - space_occupied_total);
	
	printf("\nTotal usable disk space: %dKB\n", space_available_total/1024);
	printf("Taken usable disk space: %dKB (%.2f\%)\n", space_occupied_total/1024, (space_occupied_total*100.0)/space_available_total);
	printf("Free usable disk space: %dKB (%.2f\%)\n", space_free/1024, (space_free*100.0)/space_available_total);
	
	if(!has_files_flag)
	{
		printf("No files.\n");
	}
	
	fclose(disk_file);
	return 0;
}

int read_VFS_info(char* filename)
{
	FILE* disk_file;
	struct VFS_superblock superblock;
	struct VFS_rootdir rootdir;
	bitmap free_space_bitmap;
	size_type disk_file_size, tmp_offset;
	int r;
	
	r = open_and_read_VFS(filename, &superblock, &rootdir, &free_space_bitmap, &disk_file, &disk_file_size);
	if(r != 0) return r;
	
	printf("================================\nVFS successfully opened.\n================================\n\n");
	printf("Block size: %d\n", BLOCK_SIZE);
	printf("User data block size: %d\n", FILE_DATA_CHUNK_SIZE);
	printf("VFS file size: %dKB\n", disk_file_size/1024);
	printf("Usable disk size: %dKB\n", USABLE_DATA_SIZE_BYTES(superblock.n_datablocks)/1024);
	printf("Nr of data blocks: %d\n", superblock.n_datablocks);
	printf("\nDisk structure:\n");
	printf("---------------------------------------------\n");
	printf("addr\tsize (blocks)\t        type\n");
	printf("---------------------------------------------\n");
	printf("%d\tsize %12d\tsuperblock\n", SUPERBLOCK_OFFSET, 1);
	printf("%d\tsize %12d\troot directory\n", ROOTDIR_BLOCK_OFFSET, ROOTDIR_SIZE_BLOCKS);
	printf("%d\tsize %12d\tfree space bitmap\n", BITMAP_BLOCK_OFFSET, BITMAP_SIZE_BLOCKS(superblock.n_datablocks));
	
	iterate_block_structure(&is_free_bitmap_block, free_space_bitmap, superblock.n_datablocks, DATA_BLOCK_OFFSET(superblock.n_datablocks), "data");
	
	free(free_space_bitmap);
	fclose(disk_file);
	return 0;
	
}
	
