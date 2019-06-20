/***** diskhelpers.h ***********************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * This header file declares the mehods defined in diskhelpers.c.
 ******************************************************************************/

#ifndef DISK_HELPERS_H_
#define DISK_HELPERS_H_

//defined for code coherency
typedef enum {false, true} bool;


/*******************************************************************************
 * DISK ATTRIBUTE DECLARATIONS
 ******************************************************************************/

int BYTES_PER_SECTOR;		//number of bytes in a sector
int NUM_RESERVED_SECTORS;	//reserved sectors in the disk image
int NUM_FATS;			//number of copies of the FAT table
int SECTOR_COUNT;		//total number of sectors
int SECTORS_PER_FAT;		//number of sectors in each FAT table

int SECTORS_FOR_ROOT;		//number of sectors reserved for root directory

int ROOT_SECTOR_START;		//sector number where the root directory starts
int DATA_SECTOR_START;		//sector number of the first non reserved space


/*******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/

void getBasicInfo(char *ptr);
int getFATEntry(char *ptr, int n);
int getFreeSpace(char *ptr);
int getSectorNum(int entry_num);


#endif //DISK_HELPERS_H_
