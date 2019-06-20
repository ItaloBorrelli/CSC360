/***** disklist.c **************************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * disklist.c is a source code that lists all files and directories in a given
 * FAT12 file image.
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
 * function: printDirectory
 *******************************************************************************
 * Prints all values of the given directory
 *
 * Gets all necessary values and prints them in a human readable format.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	int directory_start
 * 				byte value of start of directory to list
 *
 * @return	void		no return value
 ******************************************************************************/

void printDirectory(char *ptr, int directory_start) {
	char type;
	int size;
	char *name = malloc(sizeof(char));
	char *ext = malloc(sizeof(char));

	int date;
	int year;
	int month;
	int day;

	int time;
	int hour;
	int minute;
	int second;

	//F for file, D for directory
	if((ptr[directory_start+11] & 0x10) == 0) {
		type = 'F';
	} else {
		type = 'D';
	}

	//calculates size in bytes of file
	size =
		(ptr[directory_start+28] & 0xff) +
		((ptr[directory_start+29] & 0xff) << 8) +
		((ptr[directory_start+30] & 0xff) << 16) +
		((ptr[directory_start+31] & 0xff) << 24);

	//get file name and extension
	int i;
	for(i = 0; i < 8; i++) {
		name[i] = ptr[directory_start+i];
	}
	for(i = 0; i < 3; i++) {
		ext[i] = ptr[directory_start+8+i];
	}

	//makes value a single string with name and extension
	char *fullname = (char *)malloc(20);
	int j = -1;
	for(i = 0; i < 12; i++) {
		if(name[i] != ' ' && i < 8) {
			fullname[i] = name[i];
		} else if(j == -1 && type != 'D') {
			fullname[i] = '.';
			j++;
		} else if(ext[j] != ' ' && j < 3) {
			fullname[i] = ext[j];
			j++;
		} else {
			fullname[i] = ' ';
		}
	}

	//get date values
	date = ptr[directory_start+16] + ((ptr[directory_start+17] & 0xff) << 8);
	year = ((date & 0xfe00) >> 9) + 1980;
	month = (date & 0x01e0) >> 5;
	day = date & 0x001f;

	//get time values
	time = ptr[directory_start+14] + ((ptr[directory_start+15] & 0xff) << 8);
	hour = (time & 0xf800) >> 11;
	minute = (time & 0x07e0) >> 5;
	second = (time & 0x001f) * 2;

	printf("%c %-10d %-20s %02d-%02d-%02d %02d:%02d:%02d\n",
			type, size, fullname,
			year, month, day,
			hour, minute, second);

	free(name);
	free(ext);
	free(fullname);
}


/*******************************************************************************
 * function: listFiles
 *******************************************************************************
 * Finds all files and directories in the directory and all sub-directories.
 *
 * Finds all directories and prints them then explores all sub-directories
 * recursively.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	int sector_num	sector number of directory
 * @param	bool *rest_free	ptr identifying if free directory is reached
 *
 * @return	void		no return value
 *
 * @see				diskhelpers.h
 * @see				void printDirectory(char*, int)
 ******************************************************************************/

void listFiles(char *ptr, int sector_num, bool *rest_free) {
	//get number of the first value of the given sector
	int directory_start = sector_num * BYTES_PER_SECTOR;

	//loop until not an empty directory and not out of current sector
	while(ptr[directory_start] != 0x00 && directory_start < (sector_num + 1) * BYTES_PER_SECTOR) {
		int attr = ptr[directory_start + 11];
		int fat_entry = ptr[directory_start+26] + (ptr[directory_start+27] << 8);

		//if not a volume label and not fat entry 0 or 1 sends to print
		//directory entry
		if((attr & 0x08) == 0 && fat_entry != 0 && fat_entry != 1) {
			printDirectory(ptr, directory_start);
		}

		directory_start += 0x20;
	}

	//reset directory_start value to start of directory sector
	directory_start = sector_num * BYTES_PER_SECTOR;

	//loop until not an empty directory and not out of current sector
	while(ptr[directory_start] != 0x00 && directory_start < (sector_num + 1) * BYTES_PER_SECTOR) {
		int attr = ptr[directory_start+11];
		int fat_entry = ptr[directory_start+26] + (ptr[directory_start+27] << 8);

		//explore if a directory that doesn't start with a .
		if((attr & 0x10) != 0 && ptr[directory_start] != '.' && fat_entry != 0 && fat_entry != 1) {
			//get and print directory name
			char *name = (char *)malloc(8);
			int i;
			for(i = 0; i < 8; i++) {
				name[i] = ptr[directory_start+i];
			}

			printf("\n%s\n==================\n", name);

			//explores sub-directory until the rest of the
			//sub-directory is empty
			bool rest_sub_free = false;
			while(!rest_sub_free) {
				listFiles(ptr, getSectorNum(fat_entry), &rest_sub_free);

				//get next FAT entry for the directory until -1
				fat_entry = getFATEntry(ptr, fat_entry);
				if(fat_entry == 0xfff) break;
			}

			free(name);
		}

		//go to start of next directory entry
		directory_start += 0x20;
	}

	//identify that the end of the directory entries has been reached
	if(ptr[directory_start] == 0x00) *rest_free = true;
}


/*******************************************************************************
 * function: main
 *******************************************************************************
 * Main execution for disklist.
 *
 * Opens and maps file image and calls a recursive directory lister on the root
 * directory.
 *
 * @param	int argc	number of arguments passed during execution
 * @param	char *argv[]	vector of arguments passed during execution
 *
 * @return	int		N/A
 *
 * @see				diskhelpers.h
 * @see				void getBasicInfo(char*)
 * @see				void listFiles(char*, int, bool*)
 ******************************************************************************/

int main(int argc, char *argv[]) {
	if(argc < 2) {
		printf("ERROR: Usage \"disklist <disk_image>\"\n");
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

	//initialize values provided in the header
	getBasicInfo(ptr);

	//rest_free tells us if there are no further directory entries to stop
	//loop
	bool rest_free = false;
	int i;
	printf("ROOT\n==================\n");
	for(i = 0; i < SECTORS_FOR_ROOT; i++) {
		listFiles(ptr, ROOT_SECTOR_START+i, &rest_free);

		if(rest_free) break;
	}

	munmap(ptr, buff.st_size);
	close(fd);
}
