#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define _GNU_SOURCE

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

int main(int argc, char **argv, char **env)
{
    if (argc == 1) {
		printf("Too few arguments\n");
		return -1;
    }

    struct timeval tv_start, tv_end;
    struct timezone tz;
    if (gettimeofday(&tv_start, &tz) == -1) {
		perror("Time error\n");
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
		perror("Pipe error");
		goto end;
    }

    pid_t child_id = fork();
    if (child_id == -1) {
		perror("fork error");
		return -1;
    }

    if (child_id == 0) {
		close(1);
		if (dup2(pipefd[1], 1) == -1)
		    perror("Dup2");

		char **new_argv = argv + 1;
		execvp(argv[1], new_argv);
		perror("Execv error");

		return 0;
    } else {
		int bytes = 0;
		int lines = 0;
		int words = 0;
		char last = '-';

		const int BUFF_SIZE = 1024 * 16;

		char buffer[BUFF_SIZE];
		int x = -1;

		close(pipefd[1]);

		while ((x = read(pipefd[0], buffer, BUFF_SIZE)) > 0) {
		    bytes += x;
		    for (int i = 0; i < x; i++)
			if (buffer[i] == '\n')
			    lines++;
		    for (int i = 0; i + 1 < x; i++)
				if (isalpha(buffer[i]) || isdigit(buffer[i]))
				    if (!isalpha(buffer[i + 1]) && !isdigit(buffer[i + 1]))
						words++;
			last = buffer[x - 1];
		}
		close(pipefd[0]);
		if (isalpha(last) || isdigit(last))
			words++;

		if (x == -1) {
			perror("Read pipe error");
			goto end;
		}
		printf("Output size: bytes=%d, words = %d, lines=%d\n", bytes, words, lines);

		int status;
		pid_t wpid = wait(&status);
		if (wpid == -1) {
		    perror("wait");
		    goto end;
		} else {
		    if (WIFEXITED(status)) {
				printf("Exited, status = %d\n", WEXITSTATUS(status));
		    } else if (WIFSIGNALED(status)) {
				printf("Killed, signal = %d\n", WTERMSIG(status));
		    } else if (WIFSTOPPED(status)) {
				printf("Stopped");
			} else {
				printf("Bad wait status");
				goto end;
			}
		}
	}

  end:
    if (gettimeofday(&tv_end, &tz))
	perror("Stop time error\n");
    else {
	tv_end.tv_sec -= tv_start.tv_sec;
	tv_end.tv_usec -= tv_start.tv_usec;
	if (tv_end.tv_usec < 0) {
	    tv_end.tv_sec -= 1;
	    tv_end.tv_usec += 1000000;
	}
	printf("Work time: sec:%d msec:%d usec:%d\n",
	       (int) tv_end.tv_sec, (int) tv_end.tv_usec / 1000,
	       (int) tv_end.tv_usec % 1000);
    }

    return 0;
}
