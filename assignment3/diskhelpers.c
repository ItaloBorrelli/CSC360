/***** diskhelpers.c ***********************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * diskhelpers.c is a source file that contains certain data and methods that
 * are important to diskinfo, disklist, diskget and diskput for accessing
 * information about a FAT12 disk image.
 *
 * These are specifically data and methods that will be used by more than one
 * of the above listed executables.
 *
 * Use this source code when compiling and include diskhelpers.h in any file
 * using any of this code.
 *
 * If using any methods in this file it is critical that getBasicInfo is called
 * first to initialize all critical data about the file system image that may be
 * necessary to other methods.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "diskhelpers.h"


/*******************************************************************************
 * function: getBasicInfo
 *******************************************************************************
 * Get and calculate all critical data about the file system.
 *
 * Given a pointer to the start of the file system image this function explores
 * the boot sector for some of its basic disk geometry.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 *
 * @return	void		no return value
 *
 * @see				diskhelpers.h
 ******************************************************************************/

void getBasicInfo(char *ptr) {
	BYTES_PER_SECTOR = ptr[11] + ((ptr[12] & 0xff) << 8);
	NUM_RESERVED_SECTORS = ptr[14] + ((ptr[15] & 0xff) << 8);
	NUM_FATS = ptr[16];
	SECTOR_COUNT = ptr[19] + ((ptr[20] & 0xff) << 8);
	SECTORS_PER_FAT = ptr[22] + ((ptr[23] & 0xff) << 8);

	SECTORS_FOR_ROOT = ((ptr[17] & 0x000000ff) + ((ptr[18] & 0x000000ff) << 8)) * 32 / BYTES_PER_SECTOR;

	ROOT_SECTOR_START = NUM_RESERVED_SECTORS + (NUM_FATS * SECTORS_PER_FAT);
	DATA_SECTOR_START =
		NUM_RESERVED_SECTORS +
		NUM_FATS * SECTORS_PER_FAT +
		SECTORS_FOR_ROOT;
}


/*******************************************************************************
 * function: getFATEntry
 *******************************************************************************
 * Get a 12 bit FAT entry.
 *
 * Given a value n, this method finds the value of the n-th FAT entry in the FAT
 * table.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 * @param	int n		the entry to find
 *
 * @return	int		the value of the n-th FAT entry
 *
 * @see				diskhelpers.h
 ******************************************************************************/

int getFATEntry(char *ptr, int n) {
	//get location of the start of the first FAT table
	int fat_start = NUM_RESERVED_SECTORS * BYTES_PER_SECTOR;

	int entry=0, first, second;
	if((n%2) == 0) {
		entry += (ptr[BYTES_PER_SECTOR + ((3*n) / 2) + 1] & 0x0f) << 8;
		entry += ptr[BYTES_PER_SECTOR + ((3*n) / 2)] & 0xff;
	} else {
		entry += (ptr[BYTES_PER_SECTOR + (int)((3*n) / 2)] & 0xf0) >> 4;
		entry += (ptr[BYTES_PER_SECTOR + (int)((3*n) / 2) + 1] & 0xff) << 4;
	}

	return entry;
}


/*******************************************************************************
 * function: getFreeSpace
 *******************************************************************************
 * Calculate the number of free bytes in the file image.
 *
 * Check all FAT entries and count the number of free sectors in the file
 * image. Multiplied by the number of bytes per sector we calculate the total
 * amount of free space available.
 *
 * @param	char *ptr	a pointer to the first byte of the fs image
 *
 * @return	int		number of bytes of free space in the image
 *
 * @see				diskhelpers.h
 * @see				int getFATEntry(char*, int)
 ******************************************************************************/

int getFreeSpace(char *ptr) {
	int free_sectors = 0, i;

	//in FAT12 should be entry 2 through 2848
	//check each of those to see if they are free i.e. 0x000
	for(i = 2; i < SECTOR_COUNT - DATA_SECTOR_START + 2; i++) {
		if(getFATEntry(ptr, i) == 0x000) free_sectors++;
	}

	return free_sectors * BYTES_PER_SECTOR;
}


/*******************************************************************************
 * function: getSectorNum
 *******************************************************************************
 * Get the physical sector location.
 *
 * Given the FAT entry value calculate the sector number associated with the
 * value.
 *
 * @param	int fat_entry	FAT entry value
 *
 * @return	int		the sector number
 *
 * @see				diskhelpers.h
 ******************************************************************************/

int getSectorNum(int fat_entry) {
	return fat_entry + DATA_SECTOR_START - 2;
}
