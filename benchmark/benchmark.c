/* Benchmarks by recursively forking and execing.
 *
 * Two arguments are required and given to this program:
 *   - argv[1]: the seed to use for random number generation
 *   - argv[2]: the primary workload to use.  Allowed are one of
 *		"io" (80/20), "cpu" (20/80), "split" (50/50).
 *
 * I/O heavy workloads download files with curl.
 * CPU heavy workloads calculate factorials.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t original_pid;
int seed;
char *workload;

/* CPU intensive workload by maintaining multiplications,
 * stack state, and registers.
 * */
int factorial(int n) {
	if (n <= 2) {
		return n;
	}
	return n * factorial(n - 1);
}

/* IO intensive workload due to array accesses going to main memory. */
int sum_numbers() {
	int size = 100000;
	int max_num = 30000; // Limits the random number that was generated.
	unsigned int nums[size];
	for (size_t i = 0; i < size; i++) {
		nums[i] = i; // Use a different but consistent seed.
	}

	// Sum what we just created.
	unsigned int sum = 0;
	for (size_t i = 0; i < size; i++) {
		sum += nums[i];
	}
	return sum;
}


char *get_job() {
	// Generates a random number and chooses a job based on the result and
	// the specified workload.
	char *job = NULL;
	srandom(seed);
	int random_number = random() % 10;
	if (strcmp(workload, "io")) {
		// 80-20 split, favors IO
		if (random_number <= 7) {
			job = "io";
		} else {
			job = "cpu";
		}
	} else if (strcmp(workload, "cpu")) {
		// 20-80 split
		if (random_number <= 1) {
			job = "io";
		} else {
			job = "cpu";
		}
	} else if (strcmp(workload, "split")) {
		// 50-50 split
		if (random_number <= 4) {
			job = "io";
		} else {
			job = "cpu";
		}
	} else {
		printf("Invalid job name\n");
		exit(1);
	}
	return job;
}


// https://stackoverflow.com/questions/27541910/how-to-use-execvp
void forkbomb(int current_depth, int max_depth){
	if (current_depth == max_depth) {
		return;
	}

	pid_t pid = fork();

	if (pid < 0){
		printf("\nError Forking Child Process");
		exit(1);
	}

	char *job = get_job();
	if (strcmp(job, "io") == 0) {
		sum_numbers();
	} else if (strcmp(job, "cpu") == 0) {
		factorial(100);
	}

	forkbomb(current_depth + 1, max_depth);
}

int main(int argc, char **argv) {
	seed = atoi(argv[1]);
	workload = argv[2];
	original_pid = getpid();

	// Multithreaded spawn of 2^13 = 8192 processes.
	// Limit is 8499 by default so we can't go to depth 14.
	forkbomb(0, 12);
	exit(0);
}

