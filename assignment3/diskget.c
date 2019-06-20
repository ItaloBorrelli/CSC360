/***** diskget.c ***************************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * diskget.c is a source code that gets a file from a system file image.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#include "diskhelpers.h"


/*******************************************************************************
 * function: copyToNew
 *******************************************************************************
 * Copies from a file image to a file.
 *
 * @param	char *ptr_from	a pointer to the first byte of fs image being
 * 					copied from
 * @param	char *ptr_to	a pointer to the first byte of fs image being
 * 					copied to
 * @param	int index	index of the first byte of the file
 * @param	int file_size	total size of file being copied
 *
 * @return	void		no return value
 ******************************************************************************/

void copyToNew(char *ptr_from, char *ptr_to, int index, int file_size) {
	int i, remaining = file_size, address;
	int fat_entry = ptr_from[index+26] + (ptr_from[index+27] << 8);

	//do this until the FAT entry is -1 or 0xfff
	do {
		address = BYTES_PER_SECTOR * (31 + fat_entry);

		for(i = 0; i < BYTES_PER_SECTOR; i++) {
			if(remaining == 0) break;
			ptr_to[file_size-remaining] = ptr_from[i+address];
			remaining--;
		}

		fat_entry = getFATEntry(ptr_from, fat_entry);
	} while(fat_entry != 0xfff);
}


/*******************************************************************************
 * function: nameCompare
 *******************************************************************************
 * Checks if the directory at the given location agrees with the filename
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	int directory_start
 * 				byte value of start of directory to list
 * @param	char *filename	filename to compare directory to
 *
 * @return	bool		false if filename doesn't agree with directory
 * 				true otherwise
 ******************************************************************************/

bool nameCompare(char *ptr, int directory_start, char *filename) {
	int i;
	for(i = 0; i < 11; i++) {
		if(filename[i] != ptr[directory_start+i]) return false;
	}
	return true;
}


/*******************************************************************************
 * function: findFile
 *******************************************************************************
 * Recursively searches for the given file.
 *
 * Searches through all directories and subdirectories for the given filename
 * until it is found and gives the byte address in the file_index pointer.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	int sector_num	sector number of directory
 * @param	bool *rest_free	ptr identifying if free directory is reached
 * @param	int *file_index	ptr that pointing to the byte address of the
 * 					file being searched for if found or -1
 * @param	char *filename	name of file being searched for
 *
 * @return	void		no return value
 ******************************************************************************/

void findFile(char *ptr, int sector_num, bool *rest_free, int *file_index, char *filename) {
	//get number of the first value of the given sector
	int directory_start = sector_num * BYTES_PER_SECTOR;

	//loop until not an empty directory and not out of current sector and
	//checks the file hasn't been found yet
	while(ptr[directory_start] != 0x00 && directory_start < (sector_num + 1) * BYTES_PER_SECTOR && *file_index == -1) {
		int attr = ptr[directory_start + 11];
		int fat_entry = ptr[directory_start+26] + (ptr[directory_start+27] << 8);

		//if not a volume label, sub-directory and not fat entry 0 or 1
		//checks if the file is of the same name
		if((attr & 0x08) == 0 && (attr & 0x10) == 0 && fat_entry != 0 && fat_entry != 1) {
			if(nameCompare(ptr, directory_start, filename)) {
				*file_index = directory_start;
				return;
			}
		}

		//if the directory is a subdirectory search it for the file of
		//the given name
		if((attr & 0x10) != 0 && ptr[directory_start] != '.') {
			int fat_entry = ptr[directory_start+26] + (ptr[directory_start+27] << 8);
			bool rest_sub_free = false;

			//continue until there are no more directory listings
			//or the file index has a value
			while(!rest_sub_free && *file_index == -1) {
				findFile(ptr, getSectorNum(fat_entry), &rest_sub_free, file_index, filename);

				fat_entry = getFATEntry(ptr, fat_entry);
				if(fat_entry == 0xfff) break;
			}
		}

		//increment to next directory start
		directory_start += 0x20;
	}

	//if 0x00 there are no more directories in this subdirectory
	if(ptr[directory_start] == 0x00) *rest_free == true;
}


/*******************************************************************************
 * function: main
 *******************************************************************************
 * Main execution for diskget.
 *
 * @param	int argc	number of arguments passed during execution
 * @param	char *argv[]	vector of arguments passed during execution
 *
 * @return	int		N/A
 ******************************************************************************/

int main(int argc, char *argv[]) {
	if(argc < 3) {
		printf("ERROR: Usage \"diskget <disk_image> <file_name>\"\n");
		exit(EXIT_FAILURE);
	}

	//opens the file as read only
	int fd = open(argv[1], O_RDONLY);
	if(fd < 0) {
		printf("ERROR: Open failed\n");
		EXIT_FAILURE;
	}

	//create buffer for file statistics
	struct stat buff;
	fstat(fd, &buff);

	//map ptr to file system image
	char *ptr = mmap(0, buff.st_size, PROT_READ, MAP_SHARED, fd, 0);

	if(ptr == MAP_FAILED) {
		printf("ERROR: Failed to map file\n");
		exit(EXIT_FAILURE);
	}

	char *name = malloc(sizeof(char)), *ext = malloc(sizeof(char));
	int i, name_count=0, ext_count=0;
	bool found_period = false;

	for(i = 0; i < strlen(argv[2]); i++) {
		if(argv[2][i] == '.') {
			found_period = true;
		} else if(!found_period) {
			name[name_count++] = argv[2][i];
		} else {
			ext[ext_count++] = argv[2][i];
		}
	}

	if(name_count > 8) {
		printf("ERROR: Too many characters for file name (>8)\n");
		exit(EXIT_FAILURE);
	}
	if(ext_count > 3) {
		printf("ERROR: Too many characters for file extension (>3)\n");
		exit(EXIT_FAILURE);
	}
	if(name_count < 1) {
		printf("ERROR: File name and extension must each have at least one character\n");
		exit(EXIT_FAILURE);
	}

	char *filename = (char *)malloc(11);
	for(i = 0; i < 8; i++) {
		if(i < name_count) filename[i] = name[i];
		else filename[i] = ' ';
	}
	for(i = 0; i < 3; i++) {
		if(i < ext_count) filename[i+8] = ext[i];
		else filename[i+8] = ' ';
	}

	//initialize values provided in the header
	getBasicInfo(ptr);

	int file_index = -1;
	bool rest_free = false;
	for(i = 0; i < SECTORS_FOR_ROOT; i++) {
		findFile(ptr, ROOT_SECTOR_START+i, &rest_free, &file_index, filename);

		if(file_index != -1 || rest_free) break;
	}

	if(file_index == -1) {
		printf("File not found\n");
		exit(EXIT_FAILURE);
	} else {
		int file_size =
			(ptr[file_index+28] & 0xff) +
			((ptr[file_index+29] & 0xff) << 8) +
			((ptr[file_index+30] & 0xff) << 16) +
			((ptr[file_index+31] & 0xff) << 24);

		int fd_new = open(argv[2], O_RDWR|O_CREAT, 0666);
		if(fd_new < 0) {
			printf("ERROR: Failed to open new file\n");
			EXIT_FAILURE;
		}

		int ret_val = lseek(fd_new, file_size-1, SEEK_SET);
		if(ret_val == -1) {
			printf("Failed to seek to end of new file\n");
			exit(EXIT_FAILURE);
		}

		ret_val = write(fd_new, "", 1);
		if(ret_val !=  1) {
			printf("Failed to write end of file byte\n");
			exit(EXIT_FAILURE);
		}

		char *ptr_new = mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, fd_new, 0);
		if(ptr_new == MAP_FAILED) {
			printf("Failed to map new file\n");
			exit(EXIT_FAILURE);
		}

		copyToNew(ptr, ptr_new, file_index, file_size);

		munmap(ptr_new, file_size);
		close(fd_new);
	}

	free(filename);
	free(name);
	free(ext);
	munmap(ptr, buff.st_size);
	close(fd);
}
