#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#define DEBUG
#include "debug.h"

int process(int i)
{
	printf("I AM PROCESS %d, id = %d\n", i, (int)getpid());
	if (i == 2) {
		return EXIT_FAILURE;
	}
	if (i == 1)
		return EXIT_SUCCESS;
	sleep(60);

	return EXIT_SUCCESS;
}

int kill_childs(pid_t* childs_id)
{
	int exit_status = EXIT_SUCCESS;
    for (pid_t* iterator = childs_id; *iterator; iterator++) {
		printf("%d will killed next\n", *iterator);
		if (kill(*iterator, 9) == -1) {
			PERROR("process with id=%d was not killed correctly", (int)(*iterator));
			exit_status = EXIT_FAILURE;
		}
		else {
			log("process with id=%d was killed", (int)(*iterator));
		}
    }
    return exit_status;
}

int main(void)
{
	srand(time(0));

	const int N = 10;
	pid_t* childs_id = (pid_t*) calloc (sizeof(pid_t), N + 1);

	int exit_status = EXIT_SUCCESS;
    for (int i = 0; i < N; i++) {
		pid_t fork_status = fork();
		if (fork_status == -1) {
			PERROR("fork[%d] failed, child processes will be killed", i);
			kill_childs(childs_id);
			goto resourses_free_section;
		} else if (fork_status == 0) {
            exit_status = process(i);
            goto resourses_free_section;
		} else {
			childs_id[i] = fork_status;
		}
	}

	for (int i = 0; i < N; i++) {
        int status = 0;
        pid_t process = wait(&status);
        if (process != -1)
        if ((WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_FAILURE))) {
			log ("process with id = %d finished with error status, program will terminated", (int)process);
			kill_childs(childs_id);
        }
        else if (WIFEXITED(status)) {
			log ("process with id = %d finished with status %d", (int)process, (int)WEXITSTATUS(status));
        }
	}

systemV_free_section:

resourses_free_section:
	free(childs_id);
	return exit_status;
}
