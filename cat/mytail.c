#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define buffSize 1024

// http://www.di.uevora.pt/~lmr/syscalls.html to study system calls
// argc: argument count; contains number of arguments passed
// argv: argument vector; one dimensional array of strings
	// argv[0] cc
	// argv[1] -o
	// argv[2] mytail
	// argv[3] mytail.c
// write(file descriptor, points to char in array, number of bytes)
int main(int argc, char *argv[]){
	int fileOpen;
	int fileRead;
	int i;
	long position;
	char bufferSize[buffSize];

	int read_byte;

	if(argc < 2){
		// no input or just ./mytail
		perror("More arguments required\n./mytail file1 file2 file3\n");
		exit(1);
	} else {
		// > 1 file used as argument
		for (i =1; i < argc; i++){
			// iterate through arguments
			fileOpen = open(argv[i], O_RDWR);
			if (fileOpen){
				while ((fileRead = read(fileOpen, bufferSize, buffSize))){
					// read each file
					// read(file descriptor, character array where read content will be stored, number of bytes before truncating)

					position = lseek(fileOpen, 0L, SEEK_END);
					// lseek(fd, offset from end, start from end)

					if (position != -1){
						printf("The length of fileRead is %ld bytes. \n", position);
						write(1, "file opened\n", 13);
						write(1, bufferSize, fileRead);
					}
					else{
						perror("lseek error");
					}
				}
			} else {
				perror("Not able to open");
			}
			write(1, "file closed\n", 13);
			close(fileOpen);
		}
	}
	return 0;
}
