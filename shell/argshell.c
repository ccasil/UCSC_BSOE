#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

extern char ** get_args();

void changedirect(char **args);
void execute(char **args, int i);
void whereredirect(char **args, int i);
void pipeit(char **args, int i);

// https://web.mst.edu/~ercal/284/UNIX-fork-exec/Fork-Execl.c

void promptuser() {
	printf("Command ('exit' to quit): ");
}

void changedirect(char **args) {
	char buffersize[100];
	char *cpath = getcwd(buffersize, 100); //current working directory

	if (args[1] == NULL) {
		// if "% cd" (go to home directory)
		printf("no arguments added after cd\n");
		printf("Home Directory: %s\n", getenv("HOME"));
		chdir(getenv("HOME"));
		// int chdir(const char *path);
		// chdir() changes the current working directory of the calling process to the directory specified in path.
		// 0: success, -1: error
		if (chdir(getenv("HOME")) != 0) {
			perror("chdir");
		}
	} else if (!strcmp (args[1], "..")) {
		// if "% cd .." (go up one directory)
		chdir("..");
		printf("Moving up from: %s\n", cpath);
	} else if (!strcmp (args[1], ".")) {
		// if "% cd ." (stay in current directory)
		printf("In Current Directory: %s\n", cpath);
		chdir(".");
	} else {
		// if "% cd dir" (move to directory)
		printf("Changing Directory...\n");
		chdir(args[1]);
		if (args[0] != 0) { // must use full path or just dir name
			perror("chdir");
		}
	}
}

void execute(char **args, int i) {
	pid_t my_pid, parent_pid, child_pid;
	int status;

	// create child process
	child_pid = fork();

	if (child_pid < 0) {
		printf("\nError Forking Child Process...\n");
		exit(1);
	} else if (child_pid == 0) {
		// check if child process
		my_pid = getpid();
		parent_pid = getppid();
		// printf("\nInside Child Process...\n");
		// printf("\nChild: my pid is: %d ", my_pid);
		// printf("& my parent's pid is: %d\n\n", parent_pid);
		// loop through user input
		for (i = 0; args[i] != NULL; i++) {
			// look for special characters
			printf("Argument %d: %s\n", i, args[i]);
			// direction: input redirection (<)  output redirection (>) / (>>)  pipe (|)
			if ((!strcmp(args[i], "<")) || (!strcmp(args[i], ">")) || (!strcmp(args[i], ">>"))) {
				// https://stackoverflow.com/questions/45538027/no-such-file-or-directory-after-redirection-simple-command-line-program
				whereredirect(args, i);
				// printf("Redirection Character Found: %i\n\n", i);
			} else if (!strcmp(args[i], "|")) {
				// pipe found
				pipeit(args, i);
				// printf("| Character Found: %i\n\n", i);
			}
		}
		if (execvp(args[0], args) < 0) { //execute the command
			printf("exec failed");
			exit(1);
		}
	} else {
		// else parent process
		// wait until child finishes
		waitpid(child_pid, &status, 0);
		// while(wait(&status) != child_pid);
		// printf("\nInside Parent Process...");
		// printf("Created Child Process...\n");
		// printf("\nParent: my child's pid is: %d\n\n", child_pid);
	}
}

// https://unix.stackexchange.com/questions/171025/what-does-do-vs
// > redirect stdout to file / if the file exists it'll be replaced
// >> redirect stdout to file / if the file does not exist it'll be created. If it exists, it'll be appended to the end of the file
void whereredirect(char **args, int i) {
	int fd0;
	int fd1;

	if (!strcmp(args[i], "<")) {
		// https://canvas.ucsc.edu/courses/16313/discussion_topics/57138 (sort -nr < scores ----> sort -nr; sort <; sort scores)
		// input redirection
		// A program that reads input from the keyboard can also read input from a text file.
		// printf("\n< input found %i\n", i);
		// get argument after <
		// args[i] = NULL;
		fd0 = open(args[i + 1], O_RDONLY);
		// printf("(CHILD)ARGUMENT BEFORE <: %s\n", args[i - 1]);
		// printf("(CHILD)ARGUMENT AFTER <: %s\n", args[i + 1]);
		if (fd0) {
			dup2(fd0, 0);
			close(fd0);
			args[i] = NULL;
			execvp(args[i + 1], &args[i + 1]);
		} else {
			perror("Not able to open");
			exit(1);
		} 
		if (args[i + 1] == NULL) {
			printf("\nno file given after <");
		}
	} else if (!strcmp(args[i], ">")) {
		// output redirection
		// printf("\n> input found %i\n", i);
		// get argument after >
		// args[i] = NULL;
		fd1 = open(args[i + 1], O_WRONLY | O_CREAT);
		// printf("(CHILD)ARGUMENT BEFORE >: %s\n", args[i - 1]);
		// printf("(CHILD)ARGUMENT AFTER >: %s\n", args[i + 1]);
		if (fd1) {
			dup2(fd1, 1);
			close(fd1);
			args[i] = NULL;
			execvp(args[i - 1], &args[i + 1]);
		} else {
			perror("Not able to open");
			exit(1);
		}
		if (args[i + 1] == NULL) {
			printf("\nno file given after >");
		}
	} else if (!strcmp(args[i], ">>")) {
		// output redirection
		// printf("\n>> input found %i\n", i);
		// get argument after >
		// args[i] = NULL;
		fd1 = open(args[i + 1], O_WRONLY | O_APPEND | O_CREAT);
		// printf("(CHILD)ARGUMENT BEFORE >>: %s\n", args[i - 1]);
		// printf("(CHILD)ARGUMENT AFTER >>: %s\n", args[i + 1]);
		if (fd1) {
			dup2(fd1, 1);
			close(fd1);
			args[i] = NULL;
			execvp(args[i - 1], &args[i + 1]);
		} else {
			perror("Not able to open");
			exit(1);
		} if (args[i + 1] == NULL) {
			printf("\nno file given after >");
		}
	}
}

void pipeit(char **args, int i) {
	int pipes[2];
	pipe(pipes);
	pid_t pid;

	int status;

	// printf("(CHILD)ARGUMENT BEFORE |: %s\n", args[i - 1]);
	// printf("(CHILD)ARGUMENT AFTER |: %s\n", args[i + 1]);

	if (fork() == 0) { // lowest child
		close(pipes[1]);
		dup2(pipes[0], STDIN_FILENO); // child READS in from pipe
		close(pipes[0]);
		args[i] = NULL;
		execvp(args[i + 1], &args[i + 1]);
		exit(0);
	}
	else if ((pid = fork()) == 0) { // if other child process
		dup2(pipes[0], STDIN_FILENO);
		dup2(pipes[1], STDOUT_FILENO); // child WRITES out to pipe
		close(pipes[0]);
		close(pipes[1]);
		args[i] = NULL;
		execvp(args[i - 1], &args[i - 1]);
		exit(0);
	} else { // parent
		waitpid(pid, &status, 0);
		close(pipes[0]);
		close(pipes[1]);
	} if (args[i + 1] == NULL) {
		printf("\nno file given after |");
	}
}

int main() {
	int i;
	char **args;

	while (1) {
		promptuser();
		args = get_args();
		// no user input
		if (args[0] == NULL) {
			printf ("No arguments on line!\n");
			continue;
		}
		// user inputs "exit"
		if ( !strcmp (args[0], "exit")) {
			printf ("Exiting...\n");
			exit(0);
			break;
		}
		// user needs to change directory (doesn't need to fork())
		if ( !strcmp (args[0], "cd")) {
			changedirect(args);
			continue;
		} else { // www.csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html
			// args: argument passed
			// index: index where special character is found (index + 1 = argument)
			execute(args, i);
		}	
	}
}
