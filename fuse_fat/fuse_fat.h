#include <sys/types.h>

typedef struct block {
	char bytes[BLOCK_SIZE];
} block;

typedef struct dir {
	char file_name[4 * 6]; // 6 words.
	int64_t creation_time;
	int64_t modification_time;
	int64_t access_time;
	int32_t file_length;
	int32_t start_block;
	int32_t flags;
	int32_t unused;
} dir;

dir *get_directory(char bytes[sizeof(dir)]);
void write_directory(dir *d, char bytes[sizeof(dir)]);

int get_block_type(int index);
void initialize_super_block(block *super_block);
void initialize_fat();
void initialize_disk();
/*
static int xmp_getattr(const char *path, struct stat *stbuf);
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi);
static int xmp_open(const char *path, struct fuse_fiel_info *fi);
static int xmp_mkdir(const char *path, mode_t mode);
static int xmp_rmdir(const char *path);
static int xmp_write(const char *path, const char *buf, size_t size, 
		     off_t offset, struct fuse_file_info *fi);
struct int xmp_rename(const char *from, const char *to);
static int xmp_unlink(const char *path);
static int xmp_link(const char *from, const char *to);
*/
