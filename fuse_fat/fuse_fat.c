/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall fusexmp.c `pkg-config fuse --cflags --libs` -o fusexmp
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef ALL_RWX_PERMS
#define ALL_RWX_PERMS 0777
#endif

#ifndef STARTING_ROOT_BLOCK
#define STARTING_ROOT_BLOCK (NUM_FAT_BLOCKS + 1)
#endif

#ifndef LAST_BLOCK_IN_FILE
#define LAST_BLOCK_IN_FILE -2
#endif

#ifndef NO_ALLOC_BLOCK
#define NO_ALLOC_BLOCK -1
#endif

#ifndef FREE_BLOCK
#define FREE_BLOCK 0
#endif

#ifndef SUPER_BLOCK
#define SUPER_BLOCK 0
#endif

#ifndef FAT_BLOCK
#define FAT_BLOCK 1
#endif

#ifndef DATA_BLOCK
#define DATA_BLOCK 2
#endif

#ifndef DIR_ENTRIES_PER_DATA_BLOCK
#define DIR_ENTRIES_PER_DATA_BLOCK (BLOCK_SIZE / sizeof(dir))
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <stdlib.h>
#include <malloc_np.h>
#include <math.h>
#include <libgen.h>
#include "./fuse_fat.h"

block *blocks[TOTAL_BLOCKS];

static int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to)
{
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	int res;

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

int get_block_type(int index) {
	if (index == 0) return SUPER_BLOCK;
	if (1 <= index && index <= NUM_FAT_BLOCKS) return FAT_BLOCK;
	if (index > NUM_FAT_BLOCKS) return DATA_BLOCK;

	return -1;
}

void initialize_super_block(block *super_block){
	char *write_position = super_block->bytes;
	int tmp;

	tmp = SUPER_BLOCK_MAGIC_NUMBER;
	memcpy(write_position, &tmp, sizeof(int32_t));
	write_position += sizeof(int32_t);

	tmp = TOTAL_BLOCKS;
	memcpy(write_position, &tmp, sizeof(int32_t));
	write_position += sizeof(int32_t);

	tmp = NUM_FAT_BLOCKS;
	memcpy(write_position, &tmp, sizeof(int32_t));
	write_position += sizeof(int32_t);

	tmp = BLOCK_SIZE;
	memcpy(write_position, &tmp, sizeof(int32_t));
	write_position += sizeof(int32_t);

	tmp = STARTING_ROOT_BLOCK;
	memcpy(write_position, &tmp, sizeof(int32_t));
}

void initialize_fat() {
	int block_index = 0;
	for (int i = 1; i <= NUM_FAT_BLOCKS; ++i) // Start after super block.
	{
		for (int j = 0; j < ENTRIES_PER_FAT_BLOCK; ++j)
		{
			if (block_index == STARTING_ROOT_BLOCK) {
				blocks[i]->bytes[j * 32] = LAST_BLOCK_IN_FILE;
			} else {
				if (block_index < STARTING_ROOT_BLOCK) {
					blocks[i]->bytes[j * 32] = NO_ALLOC_BLOCK;
				} else {
					blocks[i]->bytes[j * 32] = FREE_BLOCK;
				}
			}
			block_index++;
		}
	}
}

void initialize_disk()
{
	for (int i = 0; i < TOTAL_BLOCKS; i++) {
		blocks[i] = malloc(sizeof(block));
	}
	initialize_super_block(blocks[0]);
	initialize_fat(); 
}

/* Parses directories from 64 contiguous bytes.
 * 
 * With words of 32 bits...
 *   0-5:    (char[6 * 4]) file name (including null-term)
 *   6-7:    (int64_t) creation time
 *   8-9:    (int64_t) modification time
 *   10-11:  (int64_t) access time
 *   12:     (int32_t) file length in bytes
 *   13:     (int32_t) start block
 *   14:     (int32_t) flags
 *   15:     (int32_t) unused
 */
dir *get_directory(char bytes[sizeof(dir)]) {
	char *write_position = bytes;
	dir *d = malloc(sizeof(get_directory));

	memcpy(&d->file_name, write_position, sizeof(d->file_name));
	write_position += sizeof(d->file_name);

	memcpy(&d->creation_time, write_position, sizeof(d->creation_time));
	write_position += sizeof(d->creation_time);

	memcpy(&d->modification_time, write_position, sizeof(d->modification_time));
	write_position += sizeof(d->modification_time);

	memcpy(&d->access_time, write_position, sizeof(d->access_time));
	write_position += sizeof(d->access_time);

	memcpy(&d->file_length, write_position, sizeof(d->file_length));
	write_position += sizeof(d->file_length);

	memcpy(&d->start_block, write_position, sizeof(d->start_block));
	write_position += sizeof(d->start_block);

	memcpy(&d->flags, write_position, sizeof(d->flags));
	write_position += sizeof(d->flags);

	memcpy(&d->unused, write_position, sizeof(d->unused));
	return d;
}

void write_directory(dir *d, char bytes[sizeof(dir)]) {
	char *write_position = bytes;

	memcpy(write_position, &d->file_name, sizeof(d->file_name));
	write_position += sizeof(d->file_name);

	memcpy(write_position, &d->creation_time, sizeof(d->creation_time));
	write_position += sizeof(d->creation_time);

	memcpy(write_position, &d->modification_time, sizeof(d->modification_time));
	write_position += sizeof(d->modification_time);

	memcpy(write_position, &d->access_time, sizeof(d->access_time));
	write_position += sizeof(d->access_time);

	memcpy(write_position, &d->file_length, sizeof(d->file_length));
	write_position += sizeof(d->file_length);

	memcpy(write_position, &d->start_block, sizeof(d->start_block));
	write_position += sizeof(d->start_block);

	memcpy(write_position, &d->flags, sizeof(d->flags));
	write_position += sizeof(d->flags);

	memcpy(write_position, &d->unused, sizeof(d->unused));
	write_position += sizeof(d->unused);
}

static dir* search_block(block *start_block, char *filename){
	
	for (int i = 0; i < DIR_ENTRIES_PER_DATA_BLOCK; ++i)
	{
		char current_block[sizeof(dir)];
		memcpy(
				current_block,
				&start_block->bytes[i * sizeof(dir)],
				sizeof(dir)
		);
		dir *d = get_directory(current_block);
		if (strcmp(d->file_name, filename) == 0)
		{
			return d;
		}
	}
	return NULL;
}

static dir* find_file_in_block(block *start_block, char *path){
	char *token = strtok(path, "/");
	char *rest_of_path = strtok(NULL, "");
	dir *d = search_block(start_block, token);
	if (d == NULL) return NULL;
	if (rest_of_path != NULL){
		return find_file_in_block(blocks[d->start_block], rest_of_path);
	}
	return d;
}

static int32_t get_fat(int32_t i){
	int fat_block_num = (i/ENTRIES_PER_FAT_BLOCK) + 1; // offset for super block
	int byte_offset = i%ENTRIES_PER_FAT_BLOCK;
	int32_t result;
	printf("get_fat(%d) [start]\n", i);
	memcpy(&result, &blocks[fat_block_num]->bytes[byte_offset], sizeof(result));
	printf("get_fat(%d) [end]: %d\n", i, result);
	return result;
}

static void set_fat(int32_t i, int32_t value){
	int fat_block_num = (i/ENTRIES_PER_FAT_BLOCK) + 1; // offset for super block
	int byte_offset = i%ENTRIES_PER_FAT_BLOCK;
	memcpy(&blocks[fat_block_num]->bytes[byte_offset], &value, sizeof(value));
	printf("set_fat[%d]: %d == %d\n", i, get_fat(i), value); 
}

static dir* find_file(char *path){
	for (int32_t i = STARTING_ROOT_BLOCK;
		 i != LAST_BLOCK_IN_FILE && i != NO_ALLOC_BLOCK && i != FREE_BLOCK;
		 i = get_fat(i))
	{
		printf("  finding file for block[%d]\n: %s", i, path);
		dir *d = find_file_in_block(blocks[i], path);
		if (d != NULL) return d;
	}
	return NULL;
}

static int fat_getattr(const char *path, struct stat *stbuf){
	printf("getting attributes of: %s\n", path);
	char *mutable_path = strdup(path);
	printf("mutable path: %s\n", mutable_path);
	memset(stbuf, 0, sizeof(struct stat));
	printf("memset stbuf\n");
	if (strcmp(mutable_path, "/") == 0) {
		printf("got attributes of root\n");
		stbuf->st_mode = S_IFDIR;
		stbuf->st_nlink = 2;
	} else {
		printf("finding file for: %s\n", mutable_path);
		dir *d = find_file(mutable_path);
		if (d == NULL) {
			printf("file not found with path: %s\n", path);
			return -ENOENT;
		}

		printf("found file with path: %s\n", path);
		printf("  file_name: %s\n", d->file_name);
		printf("  creation_time: %ld\n", d->creation_time);
		printf("  modification_time: %ld\n", d->modification_time);
		printf("  access_time: %ld\n", d->access_time);
		printf("  file_length: %d\n", d->file_length);
		printf("  start_block: %d\n", d->start_block);
		printf("  flags: %d\n", d->flags);
		printf("  unused: %d\n", d->unused);
		
		// if d is a directory
		if ((d->flags & 1) == 1){
			stbuf->st_mode = S_IFDIR;
			stbuf->st_nlink = 2;
		} else {
			// if d is a file
			stbuf->st_mode = S_IFREG;
			stbuf->st_nlink = 1;
		}
		stbuf->st_size = d->file_length;
		stbuf->st_atim.tv_sec = d->access_time;
		stbuf->st_mtim.tv_sec = d->modification_time;
		stbuf->st_ctim.tv_sec = d->creation_time;
	}
	stbuf->st_mode |= ALL_RWX_PERMS;
	printf("got attributes for: %s\n", path);
	return 0;
}

static dir** get_dir_entries(block *block) {
	printf("Getting directory entries\n");
	dir **dir_entries = calloc(DIR_ENTRIES_PER_DATA_BLOCK, sizeof(dir));
	int num_entries = 0;
	for (int i = 0; i < DIR_ENTRIES_PER_DATA_BLOCK; ++i)
	{
		int has_data = 0;
		for(int j = 0; j < sizeof(dir); j++){
			if (block[i*sizeof(dir)].bytes[j] != 0x0000000)
			{
				has_data = 1;
				break;
			}
		}
		if (has_data == 1) {
			char bytes[sizeof(dir)];
			memcpy(bytes, &block[i*sizeof(dir)], sizeof(dir));
			dir_entries[i] = get_directory(bytes);
			num_entries += 1;
		}
	}
	dir **resized_entries = realloc(dir_entries, (num_entries+1) * sizeof(dir));
	resized_entries[num_entries] = NULL;
	return resized_entries;
}

static int fat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	char *mutable_path = strdup(path);
	printf( "--> Getting The List of Files of %s\n", mutable_path);

	(void) offset;
	(void) fi;
	dir *d;
	int32_t start_block_idx;

	if (strcmp(mutable_path, "/") != 0) {
		printf("finding file\n");
		if ((d = find_file(mutable_path)) == NULL) return -ENOENT;
		printf("found file\n");

		start_block_idx = d->start_block;
	} else {
		start_block_idx = STARTING_ROOT_BLOCK;
	}

	filler(buf, ".", NULL, 0); // Current Directory
	filler(buf, "..", NULL, 0); // Parent Directory

	for (int32_t i = start_block_idx;
		 i != LAST_BLOCK_IN_FILE && i != NO_ALLOC_BLOCK && i != FREE_BLOCK;
		 i = get_fat(i))
	{
		dir **dir_entries = get_dir_entries(blocks[i]);
		for (int j = 0; dir_entries[j] != NULL; ++j)
		{
			if (dir_entries[j] != NULL) {
				printf("  file_name: %s\n", dir_entries[j]->file_name);
				printf("  creation_time: %ld\n", dir_entries[j]->creation_time);
				printf("  modification_time: %ld\n", dir_entries[j]->modification_time);
				printf("  access_time: %ld\n", dir_entries[j]->access_time);
				printf("  file_length: %d\n", dir_entries[j]->file_length);
				printf("  start_block: %d\n", dir_entries[j]->start_block);
				printf("  flags: %d\n", dir_entries[j]->flags);
				printf("  unused: %d\n", dir_entries[j]->unused);
				struct stat st;
				memset(&st, 0, sizeof(st));
				// if d is a directory
				if ((dir_entries[j]->flags & 1) == 1){
					st.st_mode = S_IFDIR;
					st.st_nlink = 2;
				} else {
					// if d is a file
					st.st_mode = S_IFREG;
					st.st_nlink = 1;
				}
				st.st_size = dir_entries[j]->file_length;
				st.st_atim.tv_sec = dir_entries[j]->access_time;
				st.st_mtim.tv_sec = dir_entries[j]->modification_time;
				st.st_ctim.tv_sec = dir_entries[j]->creation_time;
				st.st_mode |= ALL_RWX_PERMS;

				filler(buf, dir_entries[j]->file_name, &st, 0);
			}
		}
		free(dir_entries);
	}
	printf("Filled directory entries\n");
	return 0;
}

/* Checks existence and permissions of a given path.
 */
static int fat_open(const char *path, struct fuse_file_info *fi){
	// We build the root on init and we don't check permissions
	// so we don't need to find any metadata (it's guaranteed to exist).
	if (strcmp(path, "/") == 0) {
		return 0;
	}

	char *mutable_path = strdup(path);
	dir *d = find_file(mutable_path);
	if (d == NULL)
		return -ENOENT;

	// According to the assignment, permissions are rwx for everyone, so
	// we do not need to bother checking them here.
	
	return 0;
}

/* Scans FAT for a free block.
 */
int get_free_fat_block_idx(){
	int block_idx = 0;
	for (int i = 1; i <= NUM_FAT_BLOCKS; i++) {
		for (int j = 0; j < ENTRIES_PER_FAT_BLOCK; j++) {
			int32_t status;
			int byte_offset = j * sizeof(int32_t);
			memcpy(&status, &blocks[i]->bytes[byte_offset], sizeof(int32_t));
			if (status == FREE_BLOCK) {
				return block_idx;	
			}
			block_idx++;
		}
	}
	return -1;
}

static int fat_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	char *mutable_path = strdup(path);
	dir *d = find_file(mutable_path);
	if (d == NULL) return -ENOENT;
	
	char *write_position = buf;
	int bytes_to_read;	
	// then find the next block from the fat, and memcpy rest of data to buf
	for (int32_t i = d->start_block;
	 i != LAST_BLOCK_IN_FILE && i != NO_ALLOC_BLOCK && i != FREE_BLOCK;
	 i = get_fat(i))
	{
		if (size/BLOCK_SIZE > 0)
		{
			bytes_to_read = BLOCK_SIZE;
		} else {
			bytes_to_read = size;
		}
		memcpy(write_position, &blocks[i]->bytes[0], bytes_to_read);
		write_position += bytes_to_read;
		size -= bytes_to_read;
		if (size <= 0) break;
	}
	return 0;
}

static char* find_empty_dir(block *parent_block)
{
	for (int i=0; i < DIR_ENTRIES_PER_DATA_BLOCK; i++) {
		char *my_dir = &parent_block->bytes[i * sizeof(dir)];
		int has_data = 0;
		for(int j=0; j < sizeof(dir); j++){
			if (parent_block->bytes[j] != 0x00)
			{
				has_data = 1;
				break;
			}
		}
		if (has_data == 0) {
			printf("empty dir: %p\n", my_dir);
			return my_dir;
		}
	}
	return NULL;
}

char *get_space_for_directory(int block_idx) {
	// Check the current block for space.
	char *write_position = find_empty_dir(blocks[block_idx]);
	if (write_position != NULL) {
		return write_position;
	}

	// Check link in FAT.
	int next_block_idx = get_fat(block_idx);
	if (next_block_idx == NO_ALLOC_BLOCK) {
		return NULL;
	} else if (next_block_idx == LAST_BLOCK_IN_FILE || next_block_idx == FREE_BLOCK) {
		int free_idx = get_free_fat_block_idx();
		set_fat(block_idx, free_idx);
		set_fat(free_idx, LAST_BLOCK_IN_FILE);
		return &blocks[free_idx]->bytes[0];
	}

	// We still have links to go through in FAT.
	return get_space_for_directory(next_block_idx);
}


static int fat_mkentry(const char *path, mode_t mode, int32_t flags)
{
	printf("Making dir: %s\n", path);
	// find parent directory
	char *mutable_path = strdup(path);
	dir *d = find_file(mutable_path);
	int slash = 0;

	// (d->flags & 1) == 1) to set dir flag to show its a directory
	if (d == NULL) // path does not exist
	{
		for (int i = 0; i < sizeof(path) / sizeof(char); i++)
		{
			if (strcmp(&path[i], "/") == 0)
				{
					slash = i;
				}
		}

		// Get path up to last slash.
		char *parsedpath = NULL;
		strncpy(parsedpath, path, slash * sizeof(char));
		printf("parsed path: %s\n", parsedpath);
		
		// Get parent block.
		int parent_block_idx;
		if (parsedpath == NULL) {
			parent_block_idx = STARTING_ROOT_BLOCK;
		} else {
			dir *parent_entry = find_file(parsedpath);
			parent_block_idx = parent_entry->start_block;
		}

		dir *new_dir = malloc(sizeof(dir));

		// Set file name.
		char file_name[sizeof(new_dir->file_name)];
		strncpy(file_name, basename(path), sizeof(new_dir->file_name));
		strcpy(new_dir->file_name, file_name);
		printf("file name: %s\n", new_dir->file_name);

		// Set times.
		new_dir->creation_time = time(NULL);
		new_dir->modification_time = time(NULL);
		new_dir->access_time = time(NULL);

		// Set starting block of dirs contents.
		int free_block_idx_for_contents = get_free_fat_block_idx();
		new_dir->start_block = free_block_idx_for_contents;
		printf("  (before) contents- fat[%d]: %d\n", free_block_idx_for_contents, get_fat(free_block_idx_for_contents));
		set_fat(free_block_idx_for_contents, LAST_BLOCK_IN_FILE);
		printf("  (after) contents- fat[%d]: %d\n", free_block_idx_for_contents, get_fat(free_block_idx_for_contents));

		// Setup flags and other misc. info.
		new_dir->flags = flags;
		new_dir->unused = 1;
		new_dir->file_length = 0;

		printf("file_name: %s\n", new_dir->file_name);
		printf("creation_time: %ld\n", new_dir->creation_time);
		printf("modification_time: %ld\n", new_dir->modification_time);
		printf("access_time: %ld\n", new_dir->access_time);
		printf("file_length: %d\n", new_dir->file_length);
		printf("start_block: %d\n", new_dir->start_block);
		printf("flags: %d\n", new_dir->flags);
		printf("unused: %d\n", new_dir->unused);

		char *write_position = get_space_for_directory(parent_block_idx);
		printf("writing %s to address %p\n", path, write_position);
		write_directory(new_dir, write_position);
	}
	return 0;
}

static int fat_mkdir(const char *path, mode_t mode) {
	return fat_mkentry(path, mode, 1);
}

static int fat_mknod(const char *path, mode_t mode, dev_t rdev) {
	return fat_mkentry(path, mode, 0);
}

static void  rm_dir(block *remove_block)
{
	for (int i=0; i < DIR_ENTRIES_PER_DATA_BLOCK; i++) {
		for(int j=0; j < sizeof(dir); j++){
			if (remove_block->bytes[j] != 0x0000000)
			{
				remove_block->bytes[j] = 0;
			}
		}	
	}
}

static int fat_rmdir(const char *path)
{
	// find parent directory
	char *mutable_path = strdup(path);
	dir *d = find_file(mutable_path);
	if (d == NULL) return -ENOENT;	
	block *remove_block = blocks[d->start_block];
	rm_dir(remove_block);

	d->creation_time = 0;
	d->modification_time = 0;
	d->access_time = 0;
	d->file_length = 0;
	d->start_block = 0;;
	d->flags = 0;
	d->unused = 0;
	return 0;
}

static int fat_rename(const char *path, const char *newname)
{
	char *mutable_path = strdup(path);
	dir *d = find_file(mutable_path);
	if (d == NULL) {
		return -ENOENT;
	}

	if((d->flags & 1) == 0)
	{
		strcpy(d->file_name, newname);
	}
	return 0;
}


static struct fuse_operations xmp_oper = {
	.getattr 	= fat_getattr,
	.open 		= fat_open,
	.readdir	= fat_readdir,
	.read		= fat_read,
	.mkdir		= fat_mkdir,
	.mknod		= fat_mknod,
	.rmdir		= fat_rmdir,
	.statfs		= xmp_statfs,
	.release	= xmp_release,
	.rename     = fat_rename
};

int main(int argc, char *argv[])
{
	umask(0);
	initialize_disk();
	dir *d = malloc(sizeof(dir));

	char file_name[sizeof(d->file_name)];
	strncpy(file_name, "yeet", sizeof(d->file_name));
	strcpy(d->file_name, file_name);
	printf("file name: %s\n", d->file_name);

	d->creation_time = 0;
	d->modification_time = 0;
	d->access_time = 0;
	d->file_length = 0;
	d->start_block = 0;
	d->flags = 0;
	d->unused = 0;

	char *write_position = &blocks[STARTING_ROOT_BLOCK]->bytes[0];
	write_directory(d, write_position);

	dir *gotten = get_directory(write_position);
	printf("got directory on INIT: %s\n", gotten->file_name);

	return fuse_main(argc, argv, &xmp_oper, NULL);
}



