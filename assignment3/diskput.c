/***** diskput.c ***************************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * diskput.c reads a file and copies it to a file system image.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "diskhelpers.h"

#define MAX_DEPTH 128		//max depth of directories



/*******************************************************************************
 * function: writeDirectory
 *******************************************************************************
 * Writes directory entry for the file being added
 *
 * @param	char *ptr	pointer to diskimage
 * @param	int new_dir	byte value to start of dir to write to
 * @param	int first_sector	first sector to use for the file
 * @param	char *filename	name of file being copied
 * @param	struct stat buff	buffer of file being copied
 *
 * @return	void		no return value
 ******************************************************************************/

void writeDirectory(char *ptr, int new_dir, int first_sector, char *filename, struct stat buff) {
	ptr += new_dir;
	int i, period = -1;
	char letter;

	for(i = 0; i < 8; i++) {
		letter = filename[i];
		if(letter == '.') period = i;
		ptr[i] = (period == -1) ? toupper(letter) : ' ';
	}

	while(period == -1) {
		if(filename[i] == '.') period = i;
		i++;
	}

	bool end = false;
	for(i = 0; i < 3; i++) {
		if(filename[i+period+1] == '\0') { end = true; ptr[i+8] = ' '; }
		else if(end) ptr[i+8] = ' ';
		else ptr[i+8] = toupper(filename[i+period+1]);
	}

	ptr[11] = 0x00;

	time_t time = buff.st_mtime;
	ptr[14] = ptr[22] = time & 0xff;
	ptr[15] = ptr[23] = (time & 0xff00) >> 8;
	ptr[16] = ptr[24] = (time & 0xff0000) >> 16;
	ptr[17] = ptr[25] = (time & 0xff000000) >> 24;

	ptr[26] = (first_sector-31) & 0xff;
	ptr[27] = ((first_sector-31) & 0xff00) >> 8;

	int size = buff.st_size;
	ptr[28] = size & 0xff;
	ptr[29] = (size & 0xff00) >> 8;
	ptr[30] = (size & 0xff0000) >> 16;
	ptr[31] = (size & 0xff000000) >> 24;

	ptr -= new_dir;
}


/*******************************************************************************
 * function: setFAT
 *******************************************************************************
 * Sets a FAT value.
 *
 * @param	char *ptr	pointer to diskimage
 * @param	int fat_entry	fat entry to write
 * @param	int val		value to write to fat table
 *
 * @return	void		no return value
 ******************************************************************************/

void setFAT(char *ptr, int fat_entry, int val) {
	if((fat_entry % 2) == 0) {
		ptr[BYTES_PER_SECTOR + (int)((3*fat_entry)/2)+1] = (val >> 8) & 0x0f;
		ptr[BYTES_PER_SECTOR + (int)((3*fat_entry)/2)] = val & 0xff;
	} else {
		ptr[BYTES_PER_SECTOR + (int)((3*fat_entry)/2)] = (val << 4) & 0xf0;
		ptr[BYTES_PER_SECTOR + (int)((3*fat_entry)/2)+1] = (val >> 4) & 0xff;
	}
}


/*******************************************************************************
 * function: getFreeFAT
 *******************************************************************************
 * Finds the next available sector using the FAT table.
 *
 * @param	char *ptr	pointer to diskimage
 *
 * @return	int		empty fat entry
 ******************************************************************************/

int getFreeFAT(char *ptr) {
	int fat_entry = 2;
	while(getFATEntry(ptr, fat_entry) != 0x000) {
		fat_entry++;
	}

	return fat_entry;
}


/*******************************************************************************
 * function: writeToDisk
 *******************************************************************************
 * Byte by byte writes from the open file to the disk image.
 *
 * @param	char *ptr	pointer to diskimage
 * @param	char *ptr_file	pointer to file being copied
 * @param	int file_size	size of file being copied
 *
 * @return	int		the first sector used for the file
 ******************************************************************************/

int writeToDisk(char *ptr, char *ptr_file, int file_size) {
	int i;
	int size_left = file_size;
	int fat_entry = getFreeFAT(ptr);
	int first_sector = fat_entry+31;
	int next;
	while(size_left > 0) {
		for(i = 0; i < BYTES_PER_SECTOR; i++) {
			if(size_left == 0) {
				setFAT(ptr, fat_entry, 0xfff);
				return first_sector;
			}

			ptr[((fat_entry+31)*BYTES_PER_SECTOR)+i] = ptr_file[file_size-size_left];
			size_left--;
		}

		setFAT(ptr, fat_entry, 0xfff);
		next = getFreeFAT(ptr);
		setFAT(ptr, fat_entry, next);
		fat_entry = next;
	}
}


/*******************************************************************************
 * function: findEmptyDir
 *******************************************************************************
 * Tries to find space for a directory in the given subdirectory.
 *
 * @param	char *ptr	pointer to diskimage
 * @param	int dir		value of the start of the sector being searched
 *
 * @return	int		byte value of start of an empty directory
 ******************************************************************************/

 int findEmptyDir(char *ptr, int dir) {
	if(dir == ROOT_SECTOR_START) {
		for(dir; dir < (ROOT_SECTOR_START + SECTORS_FOR_ROOT) * BYTES_PER_SECTOR; dir += 0x20) {
			if(ptr[dir] == 0x00) return dir;
		}

		/*
		 * if we get here the max 144 directories in root have already
		 * been used and there is no space to put it in the root
		 * directory
		 */
		printf("ERROR: No space in root directory");
		exit(EXIT_FAILURE);
	} else {
		int i, dir_save = dir;
		do {
			for(i = 0; i < BYTES_PER_SECTOR/0x20; i++) {
				if(ptr[dir+(i*0x20)] == 0x00) return dir+(i*0x20);
			}

			dir = (getFATEntry(ptr, (dir-31)/BYTES_PER_SECTOR)+31) * BYTES_PER_SECTOR;
		} while(dir-31 != 0xfff);

		/*
		 * if we get here all dir space has been used and we need to
		 * make new space in the directory so we'll relink the fat
		 * table and call this again (but I'm not doing this)
		 */
	}
}


/*******************************************************************************
 * function: dirCompare
 *******************************************************************************
 * Compares name of the directory being searched for to the current directory.
 *
 * @param	char *ptr	pointer to diskimage
 * @param	int dir_start	byte value of start of directory
 * @param	char *name	name to compare to
 *
 * @return	int		N/A
 ******************************************************************************/

bool dirCompare(char *ptr, int dir_start, char *name) {
	int i;
	for(i = 0; i < 8; i++) {
		if(name[i] != ptr[dir_start+i]) return false;
		else if(name[i] == '\0' && ptr[dir_start+i] == 0x20) return true;
	}

	if(name[8] != '\0') return false;
	return true;
}


/*******************************************************************************
 * function: searchSector
 *******************************************************************************
 * Goes through the given sector to find the subdirectory being searched for.
 *
 * @param	char *ptr	pointer to diskimage
 * @param	char *name	name of directory being searched for
 * @param	int sector	sector being searched
 *
 * @return	int		byte location of the subdirectory being searched for
 *				-2 if not found in this sector and at end
 *				-1 if not found in this sector
 ******************************************************************************/

int searchSector(char *ptr, char *name, int sector) {
	int dir_start = sector * BYTES_PER_SECTOR;
	while(ptr[dir_start] != 0x00 && dir_start < (sector + 1) * BYTES_PER_SECTOR) {
		if((ptr[dir_start+11] & 0x10) != 0) {
			if(dirCompare(ptr, dir_start, name)) return dir_start;
		}
		dir_start += 0x20;
	}

	if(ptr[dir_start] = 0x00) return -2;
	else return -1;
}


/*******************************************************************************
 * function: changeDirectory
 *******************************************************************************
 * Goes to the start of the given next directory.
 *
 * @param	char *ptr	pointer to diskimage
 * @param	char **current	directory we are in
 * @param	char *next	next directory to go to
 * @param	int *sector	start of this directory
 *
 * @return	bool	true if dir found, false if not
 ******************************************************************************/

bool changeDirectory(char *ptr, char **current, char *next, int *sector) {
	int dir_loc;
	if(*current == "root") {
		int i;
		for(i = 0; i < SECTORS_FOR_ROOT; i++) {
			dir_loc = searchSector(ptr, next, *sector+i);
			if(dir_loc == -2) return false;
			else if(dir_loc != -1) break;
		}
	} else {
		int fat_entry = *sector-31;
		do {
			dir_loc = searchSector(ptr, next, fat_entry+31);
			if(dir_loc == -2) return false;
			else if(dir_loc != -1) break;

			fat_entry = getFATEntry(ptr, fat_entry);
		} while(fat_entry != 0xfff);
	}

	if(dir_loc == -1) return false;

	*current = next;
	*sector = ptr[dir_loc+26] + (ptr[dir_loc+27] << 8) + 31;
}


/*******************************************************************************
 * function: parseFileName
 *******************************************************************************
 * Looks at the file name and parses it to subdirectory path and the file name.
 *
 * @param	char *arg	full argument from command line
 * @param	char **filename	filename to modify
 * @param	char *directories[]	array of subdirectory path to modify
 *
 * @return	int		number of subdirectories
 ******************************************************************************/

int parseFileName(char *arg, char **filename, char **filename_fat, char *directories[]) {
	int dir_depth = 0;
	char *ptr = strtok(arg, "/");
	while(ptr != NULL) {
		directories[dir_depth++] = ptr;
		ptr = strtok(NULL, "/");
	}

	*filename = directories[dir_depth-1];
	directories[dir_depth-1] = NULL;

	return dir_depth-1;
}


/*******************************************************************************
 * function: main
 *******************************************************************************
 * Main execution for diskput.
 *
 * @param	int argc	number of arguments passed during execution
 * @param	char *argv[]	vector of arguments passed during execution
 *
 * @return	int		N/A
 ******************************************************************************/

int main(int argc, char *argv[]) {
	if(argc < 3) {
		printf("ERROR: Usage \"diskput <disk_image> <file_name>\"\n");
		exit(EXIT_FAILURE);
	}

	char *filename = malloc(sizeof(char)), *filename_fat = malloc(sizeof(char));
	char *directories[MAX_DEPTH];
	int dir_depth = parseFileName(argv[2], &filename, &filename_fat, directories);

	//opens the file system as read/write
	int fs = open(argv[1], O_RDWR);
	if(fs < 0) {
		printf("ERROR: Opening disk image failed\n");
		exit(EXIT_FAILURE);
	}

	//create buffer for file statistics
	struct stat buff;
	fstat(fs, &buff);
	int disk_size = buff.st_size;

	//map ptr to file system image
	char *ptr = mmap(0, disk_size, PROT_READ|PROT_WRITE, MAP_SHARED, fs, 0);

	if(ptr == MAP_FAILED) {
		printf("ERROR: Failed to map disk image\n");
		exit(EXIT_FAILURE);
	}

	//opens the file to be copied as read
	int fc = open(filename, O_RDONLY);
	if(fc < 0) {
		//checks if file doesn't exist or if failed for another reason
		if(errno == ENOENT) printf("File not found\n");
		else printf("ERROR: Opening file for copying failed\n");
		exit(EXIT_FAILURE);
	}

	//reuse buffer for file statistics
	fstat(fc, &buff);
	int file_size = buff.st_size;

	//map ptr to file to copy
	char *ptr_file = mmap(0, file_size, PROT_READ, MAP_SHARED, fc, 0);

	if(ptr_file == MAP_FAILED) {
		printf("ERROR: Failed to map file\n");
		exit(EXIT_FAILURE);
	}

	//initialize data for disk image and get the amount of free space
	getBasicInfo(ptr);
	int free_space = getFreeSpace(ptr);

	if(file_size > free_space) {
		printf("Not enough free space in the disk image\n");
		exit(EXIT_FAILURE);
	}

	int i, dir_sector = ROOT_SECTOR_START;
	char *current_dir = "root";

	//go through disk and find first logical sector of the necessary
	//directory
	for(i = 0; i < dir_depth; i++) {
		int found = changeDirectory(ptr, &current_dir, directories[i], &dir_sector);
		if(!found) {
			printf("The directory not found\n");
			exit(EXIT_FAILURE);
		}
	}

	//find an empty directory in the given subdirectory
	int new_dir = findEmptyDir(ptr, dir_sector*BYTES_PER_SECTOR);

	//write data to disk and get first used fat
	int first_sector = writeToDisk(ptr, ptr_file, file_size);

	//write the directory entry
	writeDirectory(ptr, new_dir, first_sector, filename, buff);

	munmap(ptr, disk_size);
	munmap(ptr_file, file_size);
	close(fs);
	close(fc);
}
