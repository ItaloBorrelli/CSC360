University of Victoria
CSC 360 Fall 2018
Italo Borrelli
V00884840

This program represents a simple file system for FAT32 disks and is capable of completing the following tasks as described. Compile with make. Two sample disk images are provided in this folder.

diskinfo
Use as ./diskinfo <diskimage>
Get general information about the disk image

disklist
Use as ./disklist <diskimage>
Get all files and subdirectories with some information from the diskimage

diskget
Use as ./diskget <diskimage> <file>
Get a file from the disk image and write it to the current unix directory

diskput
Use as ./diskget <diskimage> <file>
Write a file to the diskimage
