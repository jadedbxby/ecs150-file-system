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


/* Instantiate local datastructs */
static superblock* sup_inst;
static fat* fat_inst;
static root* root_inst[128]; 

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

int fs_info(void)
{
	/* TODO: Phase 1 */

	printf("File system info: \n \n");
	printf("Total amount of blocks of virtual disk: %d \n",sup_inst->tot_blocks);
	printf("Root directory block index: %d \n",sup_inst->root_index);
	printf("Data block start index: %d \n",sup_inst->block_start_index);
	printf("Number of data blocks: %d \n",sup_inst->data_blocks);
	printf("Number of blocks for FAT: %d \n",sup_inst->fat_blocks);

	return 0; 
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

