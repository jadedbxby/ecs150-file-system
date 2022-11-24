#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/*End-of-Chain FAT*/
#define FAT_EOC 0xFFFF
#define SIG "ECS150FS"

/* TODO: Phase 1 */

/* Define datastructure to store metadata blocks */

//Superblock
typedef struct __attribute__((__packed__)) superblock {
	uint8_t signature[8];
	uint16_t tot_blocks;
	uint16_t root_index;
	uint16_t block_start_index;
	uint16_t data_blocks;
	uint8_t fat_blocks;
	uint8_t padding[4079];
}superblock;

//FAT
typedef struct __attribute__((__packed__)) fat {
	uint16_t flat_array;
}fat; 

//Root directory 
typedef struct __attribute__((__packed__)) root_dir {
	uint8_t file_name[16];
	uint32_t file_size;
	uint16_t block1_index;
	uint8_t padding[10];
}root;


//File Descriptor 
typedef struct FileDescriptor{
	int id;
	int offset;
	int index; // index of the file of the root directory
}FileDescriptor;


/* Instantiate local datastructs */
static superblock* sup_inst;
static fat* fat_inst;
static root root_inst[128]; 

/* Instantiate file descriptor */
static FileDescriptor file[FS_OPEN_MAX_COUNT]; 
static int openFiles = 0;
static int fileID;

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */

	//check for failed to open the disk 
	if (block_disk_open(diskname) != 0) {
		return -1; 
	}

	//Initialize metadata blocks 
	sup_inst = malloc(BLOCK_SIZE);
	fat_inst = malloc((sup_inst->fat_blocks)*BLOCK_SIZE);

	/* Superblock error catching */

	//signature error check
	if (strncmp((char*)(sup_inst->signature), SIG, 8)) {
		return -1; 
	}

	//total amount of blocks check 
	if (sup_inst->tot_blocks != block_disk_count()) {
		return -1;
	}

	/*root starts in the block following the FAT*/
	if(sup_inst->root_index != (sup_inst->fat_blocks + 1)) {
		return -1;
	}

	//first data block index check
	if(sup_inst->block_start_index != (sup_inst->root_index+1)){
		return -1;
	}

	/* Error catching for FAT*/

	//check EOC value 
	if(fat_inst[0].flat_array != FAT_EOC) {
		return -1;
	}

	/* PHASE 3: Initialize fd*/
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		file[i].id = -1;
		file[i].offset = 0;
		file[i].index = -1;
	}

	return 0;

}

int fs_umount(void)
{
	/* TODO: Phase 1 */

	//check for failed to close the disk
	if (block_disk_close() != 0) {
		return -1;
	}

	return 0; 
}

//Count free fat blocks
int free_fat_count() {
	int c = 0;
	for (int i=1; i < sup_inst->data_blocks; i++) { 
		if(fat_inst[i].flat_array == 0) //check if empty 
			c++;
	}
	return c;
}

//Count root dir entries 
int free_rdir_count() {
	int c = 0;
	for (int i=0; i < FS_FILE_MAX_COUNT; i++) {
		if((char)*(root_inst[i].file_name) == '\0') //if entry is empty
			c++;
	}
	return c;
}

int fs_info(void)
{
	/* TODO: Phase 1 */

	printf("FS Info: \n ");
	printf("total_blk_count= %d \n",sup_inst->tot_blocks);
	printf("fat_blk_count= %d \n",sup_inst->fat_blocks);
	printf("rdir_blk=: %d \n",sup_inst->root_index);
	printf("data_blk=%d\n", sup_inst->block_start_index);
	printf("data_blk_count=%d\n",sup_inst->data_blocks);

	printf("fat_free_ratio=%d/%d\n", free_fat_count(), sup_inst->data_blocks);
	printf("rdir_free_ratio=%d/128\n", free_rdir_count());

	return 0; 
}

//Helper function to search for free FAT block to create file in
int free_fat() {
	uint16_t i = 1;
	while (i< sup_inst->data_blocks) {
		if(fat_inst[i].flat_array == 0) { 
			return i; //if slot in fat array is empry return its index to create file into
		}
		i++;
	}
	return -1; //if no free space error
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */

	//valid filename check 

	//valid filename length check
	if(strlen(filename)*sizeof(char) > FS_FILENAME_LEN){
		return -1;
	}

	//find free fat block and remap everything 
	int free_slot;
	for (free_slot = 0; free_slot < FS_FILE_MAX_COUNT; free_slot++) {
		if((char) *(root_inst[free_slot].file_name) == '\0') {
			strcpy((char*) root_inst[free_slot].file_name, filename);
			root_inst[free_slot].file_size = 0;
			root_inst[free_slot].block1_index = free_fat();
			fat_inst[root_inst[free_slot].block1_index].flat_array = FAT_EOC;
			break;
		}
	}

	//no more free space check
	if(free_slot == FS_FILE_MAX_COUNT) {
		return -1;
	}

	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	int slot;
	
	//deleting 
	for (slot = 0; slot < FS_FILE_MAX_COUNT; slot++) {
		//if file found in block 
		if(strncmp((char*)root_inst[slot].file_name, filename, strlen(filename)) == 0) {
			uint16_t i = root_inst[slot].block1_index;
			//go through entrres in fat and empty it 
			while(i != FAT_EOC) {
				uint16_t j = fat_inst[i].flat_array;
				fat_inst[i].flat_array = 0;
				i = j; 
			}
			break;
		}
	}

	//file doesnt exist check
	if(slot == FS_FILE_MAX_COUNT) {
		return -1;
	}

	return 0; 
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	int i;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++) {
		//non empty filename check
		if((char)*(root_inst[i].file_name) != '\0') {
			printf("file: %s,size: %d, data_blk: %d\n", root_inst[i].file_name, root_inst[i].file_size, root_inst[i].block1_index);
		}
	}

	//no disk opened check 
	if(block_disk_count() == -1) {
		return -1;
	}

	return 0; 
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	if (openFiles > FS_OPEN_MAX_COUNT) // rename numFilesOpen
		return -1;

	for (int j = 0; j < FS_FILE_MAX_COUNT; j++) {
		if (strncmp((char*)root_inst[j].file_name, filename, strlen(filename)) == 0) {
			for (int k = 0; k < FS_FILE_MAX_COUNT; k++) {
				if (file[k].id == -1) {
					file[k].id = fileID;
					file[k].index = j;
					file[k].offset = 0; // change offset?
					fileID++;
					openFiles++;
					return file[k].id;
				}
			}
		}
	}
	return -1;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	{
	if (fd < 0 || fd > fileID)
		return -1;

	int i;
	for (i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (file[i].id == fd) {
			file[i].id = -1;
			file[i].offset = 0;
			file[i].index = -1;
			openFiles--;
			return 0;
		}
	}
	return -1;
}
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	if (fd > fileID || fd < 0)
		return -1;

	int i;
	for (i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (file[i].id == fd)
			return root_inst[file[i].index].file_size;
	}
	return -1;
}

//int fs_lseek(int fd, size_t offset)
//{
	/* TODO: Phase 3 */
//}

//int fs_write(int fd, void *buf, size_t count)
//{
	/* TODO: Phase 4 */
//}

//int fs_read(int fd, void *buf, size_t count)
//{
	/* TODO: Phase 4 */
//}