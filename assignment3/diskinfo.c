/***** diskinfo.c **************************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * diskinfo.c is a source file that gets general info about a FAT12 disk image.
 *
 * This code lists a disk images OS name, disk label, it's total space and free
 * space in bytes, the number of files contained in it, not including
 * directories, the number of copies of the FAT and the number of sectors in
 * each FAT.
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
 * function: getOSName
 *******************************************************************************
 * Modifies os_name to the OS name in disk image.
 *
 * Copies the OS name from the boot sector to os_name.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	char *os_name	pointer to char array to modify
 *
 * @return	void		no return value
 ******************************************************************************/

void getOSName(char *ptr, char *os_name) {
	int i;

	for(i = 0; i < 8; i++) {
		os_name[i] = ptr[i+3];
	}
}


/*******************************************************************************
 * function: getDiskLabel
 *******************************************************************************
 * Modifies label to the disk label in disk image.
 *
 * Copies the disk label from the boot sector to the label char array. If the
 * label is not in the boot sector it is searched for in the root sectors.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	char *label	pointer to char array to modify
 *
 * @return	void		no return value
 *
 * @see				diskhelpers.h
 ******************************************************************************/

void getDiskLabel(char *ptr, char *label) {
	//try to get label from boot sector at offset 43 for 8 bytes
	int i;
	for(i = 0; i < 8; i++) {
		label[i] = ptr[i+43];
	}

	//if label not found try to find it in the root sector
	if(label[0] == ' ') {
		int directory_start = ROOT_SECTOR_START * BYTES_PER_SECTOR;
		while(ptr[directory_start] != 0x00 && directory_start < DATA_SECTOR_START * BYTES_PER_SECTOR) {
			//0x08 at position 11 in directory identifies a label
			if(ptr[directory_start + 11] == 0x08) {
				for(i = 0; i < 8; i++) {
					label[i] = ptr[i+directory_start];
				}

				break;
			}

			//go to start of next directory entry
			directory_start += 0x20;
		}
	}
}


/*******************************************************************************
 * function: getFileCount
 *******************************************************************************
 * Number of files in the given sector.
 *
 * Counts the number of files in the given sector and identifies if the end of
 * the directory has been reached. Recursively checks the number of files in
 * all directories.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	int sector_num	sector number of directory
 * @param	bool *rest_free	ptr identifying if free directory is reached
 *
 * @return	int		number of files in the given sector
 *
 * @see				diskhelpers.h
 * @see				int getSectorNum(int)
 * @see				int getFATEntry(char*, int)
 ******************************************************************************/

int getFileCount(char *ptr, int sector_num, bool *rest_free) {
	int count = 0;

	//get number of the first value of the given sector
	int directory_start = sector_num * BYTES_PER_SECTOR;

	//loop until not an empty directory and not out of current sector
	while(ptr[directory_start] != 0x00 && directory_start < (sector_num + 1) * BYTES_PER_SECTOR) {
		int attr = ptr[directory_start + 11];
		if((attr & 0x08) == 0 && (attr & 0x10) == 0) {
			count++;
		}

		//if entry is a subdirectory and doesn't start with .
		if((attr & 0x10) != 0 && ptr[directory_start] != '.') {
			//get FAT head position for subdirectory
			int fat_entry = ptr[directory_start + 26] + (ptr[directory_start + 27] << 8);
			bool rest_sub_free = false;

			while(!rest_sub_free) {
				//recursively get count
				count += getFileCount(ptr, getSectorNum(fat_entry), &rest_sub_free);

				//get next FAT entry for the directory until -1
				fat_entry = getFATEntry(ptr, fat_entry);
				if(fat_entry == 0xfff) break;
			}
		}

		//go to start of next directory entry
		directory_start += 0x20;
	}

	//identify that the end of the directory entries has been reached
	if(ptr[directory_start] == 0x00) *rest_free = true;

	return count;
}


/*******************************************************************************
 * function: main
 *******************************************************************************
 * Main execution for diskinfo.
 *
 * Opens the user provided file system image and retrieves pertinent data about
 * the file printing the information for the user.
 *
 * @param	int argc	number of arguments passed during execution
 * @param	char *argv[]	vector of arguments passed during execution
 *
 * @return	int		N/A
 *
 * @see				diskhelpers.h
 * @see				void getBasicInfo(char*)
 * @see				void getOSName(char*, char*)
 * @see				void getDiskLabel(char*, char*)
 * @see				int getFileCount(char*, int, bool*)
 * @see				int getFreeSpace(char*)
 ******************************************************************************/

int main(int argc, char *argv[]) {
	if(argc < 2) {
		printf("ERROR: Usage \"diskinfo <disk_image>\"\n");
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

	char *os_name = (char *)malloc(8);
	getOSName(ptr, os_name);

	char *label = (char *)malloc(8);
	getDiskLabel(ptr, label);

	//go through each root sector and count unless the rest of the directory
	//is free
	int file_count = 0, i;
	bool rest_free = false;
	for(i = 0; i < SECTORS_FOR_ROOT; i++) {
		file_count += getFileCount(ptr, ROOT_SECTOR_START, &rest_free);
		if(rest_free) break;
	}

	//print all info
	printf("OS name:                    %s\n", os_name);
	printf("Disk label:                 %s\n", label);
	printf("Size of disk:               %d bytes\n", SECTOR_COUNT * BYTES_PER_SECTOR);
	printf("Free size of disk:          %d bytes\n\n", getFreeSpace(ptr));
	printf("============================================\n");
	printf("Number of files on disk:    %d\n\n", file_count);
	printf("============================================\n");
	printf("Number of FAT copies:       %d\n", NUM_FATS);
	printf("Sectors per FAT:            %d\n", SECTORS_PER_FAT);

	free(os_name);
	free(label);

	munmap(ptr, buff.st_size);
	close(fd);
}
