#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/*End-of-Chain FAT*/
#define FAT_EOC 0xFFFF
/* Signature*/
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
static int fileID = 0;

/* helper functions to initialize metadata blocks*/
int init_sup() {
    sup_inst = malloc(BLOCK_SIZE);
    block_read(0,sup_inst);
    return 0;
}

int init_fat() {
    fat_inst = malloc((sup_inst->fat_blocks)*BLOCK_SIZE);
    
	//
    int i;
	for(i = 1; i <= sup_inst->fat_blocks; i++) {
		block_read(i, (char*)fat_inst + BLOCK_SIZE*(i-1));
	}

    return 0;
	
}

int init_root() {
    block_read(sup_inst->root_index, root_inst);
    return 0; 
}

/* PHASE 3: Initialize fd*/
int init_fd(){	
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		file[i].id = -1;
		file[i].offset = 0;
		file[i].index = -1;
	}
    return 0;
}


int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */

	//check for failed to open the disk 
	if (block_disk_open(diskname) != 0) {
		return -1; 
	}

    //initialize metadata
    init_sup();
    init_fat();
    init_root();
    
    init_fd();


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


	return 0;

}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	block_write(0, sup_inst);
	for (int i = 1; i <= sup_inst->fat_blocks; i++) {
		block_write(i, (char*)fat_inst + BLOCK_SIZE*(i - 1));
	}

	block_write(sup_inst->root_index, root_inst);
    
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
	printf("total_blk_count=%d \n",sup_inst->tot_blocks);
	printf("fat_blk_count=%d \n",sup_inst->fat_blocks);
	printf("rdir_blk=%d \n",sup_inst->root_index);
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

	//valid filename length check
	if(strlen(filename)*sizeof(char) > FS_FILENAME_LEN){
		return -1;
	}

	//find free fat block and remap everything 
	int free_slot;
	for (free_slot = 0; free_slot < FS_FILE_MAX_COUNT; free_slot++) {
		//if root index pointed to is empty 
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
	for (slot = 0; slot < FS_FILE_MAX_COUNT; slot++) {
		//if file found in block 
		if(strncmp((char*)root_inst[slot].file_name, filename, strlen(filename)) == 0) {
			uint16_t i = root_inst[slot].block1_index; 
			//go through entrees in fat and empty it 
			while(i != FAT_EOC) {
				uint16_t j = fat_inst[i].flat_array;
				fat_inst[i].flat_array = 0;
				i = j; 
			}
			break;
		}
	}

	//file not there check
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
	//max allowed open files check
	if (openFiles > FS_OPEN_MAX_COUNT) 
		return -1;

	for (int j = 0; j < FS_FILE_MAX_COUNT; j++) {
		//filename match found 
		if (strncmp((char*)root_inst[j].file_name, filename, strlen(filename)) == 0) {
			//look for epty entree to open to
			for (int k = 0; k < FS_FILE_MAX_COUNT; k++) {
				if (file[k].id == -1) {
					file[k].id = fileID;
					file[k].index = j;
					file[k].offset = 0; 
					fileID++;
					openFiles++;
					return file[k].id; // file id to open to
				}
			}
		}
	}
	return -1; //filename mathc not found 
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	{
	if (fd < 0 || fd > fileID)
		return -1;

	int i;
	for (i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		//file found at correct fd check
		if (file[i].id == fd) {
			file[i].id = -1;
			file[i].offset = 0;
			file[i].index = -1;
			openFiles--;
			return 0;
		}
	}
	return -1; //file not found check
}
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	//wrong descriptor check 
	if (fd > fileID || fd < 0) 
		return -1;
	
	int i;
	for (i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		//file found at correct fd check
		if (file[i].id == fd)
			return root_inst[file[i].index].file_size;
	}
	return -1; //file not found check
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	int fd_size = fs_stat(fd);
	//wrong file size check
	//out of scope check 
	if(fd_size == -1 || (int)offset > fd_size)
		return -1;
	
	int i;
	for(i = 0; i< FS_OPEN_MAX_COUNT; i++) {
		//file found at correct fd check
		if(file[i].id == fd) {
			file[i].offset = offset;
			return 0;
		}
	}
	return -1; //file not found check
}

/* Helper function to get fd_index of the open file descriptor */
int file_index(int fd)
{
	if (fd > fileID || fd < 0)
		return -1; //invalid fd

	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(file[i].id == fd){ //found fd
			return i;
		}
	}
	return -1; // fd not found
}
/* Helper function to get data block at offset */
int block_index(int fd)
{
	int fdIndex = file_index(fd); 
	int rootIndex = file[fdIndex].index;
	uint16_t dataIndex = root_inst[rootIndex].block1_index; 

	int offset = file[fd].offset;
	//out of scope offset check
	while(offset >= BLOCK_SIZE) { //iterate thru blocks
		dataIndex = fat_inst[dataIndex].flat_array;
		//end of fat block reahched -> nothing allocated
		if (dataIndex == FAT_EOC) 
			return -1;
		offset = offset - BLOCK_SIZE;
	}
	//return index at offset 
	return dataIndex + sup_inst->block_start_index;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	int fdIndex = file_index(fd); //current fd
	
	//invalid fd check 
	if (fdIndex == -1)
		return -1; 

	int buf_off = 0; //offset
	size_t remainR = count; //bytes to read from
	size_t leftOff, rightOff;
	size_t bytesRead; //number of bytes read from

	int i = 0; 
	//while not done reading 
	while(remainR != 0) { 

		leftOff = 0;
		//fisrt block read check
		if (i == 0)
			leftOff = file[fdIndex].offset % BLOCK_SIZE;
	
		rightOff = 0;
		//last block read check 
		if (remainR + leftOff < BLOCK_SIZE) {
			rightOff = BLOCK_SIZE - remainR - leftOff;
		}

		//read into buffer
		void *bounceBuffer = malloc(BLOCK_SIZE);
		if (block_read(block_index(fd), bounceBuffer) == -1) {
			fprintf(stderr, "Error in block reading");
			return count - remainR; 
		}

		//read into buffer 
		bytesRead = BLOCK_SIZE - leftOff - rightOff;
		memcpy((char*)buf+buf_off, (char*)bounceBuffer+leftOff, bytesRead); 

		//var increment 
		file[fd].offset = file[fd].offset + bytesRead; //offset
		buf_off = buf_off + bytesRead; //read buffer offset
		remainR = remainR - bytesRead; //left to read
		free(bounceBuffer);
		i++;
	}

	return count - remainR; //total number read
}

//change file size at fd 
void set_file_size(int fd, int inc) {
	int fdIndex = file_index(fd); //current fd 
	int rootIndex = file[fdIndex].index; //root 
	root_inst[rootIndex].file_size += inc;
}

/* Helper function to add more blocks */
void add_block(int fd){

	int fdIndex = file_index(fd); //current fd
	int rootIndex = file[fdIndex].index; //root
	uint16_t dataIndex = root_inst[rootIndex].block1_index; 
	uint16_t i = dataIndex; 
	//while not end of block
	while(fat_inst[i].flat_array != FAT_EOC){
		i = fat_inst[i].flat_array;
	}

	int newIndex;
	//block is full check
	if ((newIndex = free_fat()) == -1) { 
		return;
	} else {
		fat_inst[i].flat_array = newIndex; 
		fat_inst[fat_inst[i].flat_array].flat_array = FAT_EOC; //remap end of fat 
	}
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	int fdIndex = file_index(fd); //current fd
	
	//invalid fd check 
	if (fdIndex == -1)
		return -1; 

	int buf_off = 0; //offset
	size_t remainW = count; //bytes to write to
	size_t leftOff, rightOff; 
	size_t bytes_w; //number of bytes written to

	int i = 0; 

	//while not done writing
	while(remainW != 0) { 
		leftOff = 0; 
		if (i == 0)
			leftOff = file[fdIndex].offset % BLOCK_SIZE;

		rightOff = 0;
		if (remainW + leftOff < BLOCK_SIZE) {
			rightOff = BLOCK_SIZE - remainW - leftOff;
		}

		//write into buffer
		void *bounceBuffer = malloc(BLOCK_SIZE);
		//block is full check
		if (block_index(fd) == -1) { 
			//disk is full check
			if (free_fat_count() <= 0) { 
				set_file_size(fd, count - remainW);
				return count - remainW; //total number written
			} else { 
				//add new block
				add_block(fd);
			}
		} else {
			block_read(block_index(fd), bounceBuffer);
		}

		//write into remaining free space
		bytes_w = BLOCK_SIZE - leftOff - rightOff;
		memcpy((char*)bounceBuffer+leftOff, (char*)buf+buf_off, bytes_w); 

		block_write(block_index(fd), bounceBuffer); 

		//var increment 
		file[fd].offset = file[fd].offset + bytes_w; 
		buf_off = buf_off + bytes_w; 
		remainW = remainW - bytes_w;
		free(bounceBuffer);
		i++;
	}
	set_file_size(fd, count - remainW); //update remaining space
	return count - remainW; //total number written
}