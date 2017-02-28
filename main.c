#include "VFS.h"
#include <stdlib.h> // atoi
#include <stdio.h> // printf
#include <unistd.h> // unlink

void print_usage()
{
	printf("Usage: VFS option args\n\n");
	printf("Available options:\n");
	printf("c - create Virtual FileSystem, args - filename size\n");
	printf("o - open a Virtual FileSystem and print its info, args - filename\n");
	printf("l - list the root directory of a Virtual FileSystem, args - vfs_filename\n");
	printf("a - copy file to a Virtual FileSystem, args - vfs_filename, file_to_add, filename_on_VFS\n");
	printf("g - copy file from a Virtual Filesystem, args - vfs_filename, filename_on_VFS, path_to_copy\n");
	printf("r - remove file from a Virtual FileSystem, args - vfs_filename, filename_to_delete\n");
	printf("d - delete a Virtual FileSystem, args - vfs_filename\n");
	printf("h - print this usage info\n");
}

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		print_usage();
		return 2;
	}
	if(argv[1][1] != 0) return 2;
	
	switch(argv[1][0])
	{
		case 'c':
			if(argc < 4)
			{
				print_usage();
				return 2;
			}
			create_VFS(argv[2], atoi(argv[3]));
			break;
		case 'o':
			if(argc < 3)
			{
				print_usage();
				return 2;
			}
			read_VFS_info(argv[2]);
			break;
		case 'a':
			if(argc < 5)
			{
				print_usage();
				return 2;
			}
			add_file_to_VFS(argv[2], argv[3], argv[4]);
			break;
		
		case 'l':
			if(argc < 3)
			{
				print_usage();
				return 2;
			}
			list_VFS_rootdir(argv[2]);
			break;
			
		case 'g':
			if(argc < 5)
			{
				print_usage();
				return 2;
			}
			get_file_from_VFS(argv[2], argv[3], argv[4]);
			break;
			
		case 'r':
			if(argc < 4)
			{
				print_usage();
				return 2;
			}
			remove_file_from_VFS(argv[2], argv[3]);
			break;
			
		case 'd':
		
			if(argc < 2)
			{
				print_usage();
				return 2;
			}
			if(unlink(argv[2]) != 0)
			{
				printf("Couldn't remove file %s\n", argv[2]);
			}
			else
			{
				printf("Successfully removed file %s\n", argv[2]);
			}
			break;
			
		case 'h':
			print_usage();
			return 0;
			
		default:
			print_usage();
			return 2;
	}
	
	return 0;
}
