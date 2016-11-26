#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

int strcnt(const char* str, char ch)
{
	int result = 0;
	for (char* iter = (char*)str; *iter; iter++)
		if (*iter == ch)
			result++;
	return result;
}

int strwcnt(const char* str)
{
    char prev = ' ';
    int result = 0;
    for (char* iter = (char*) str; *iter;  prev = *iter, iter++)
        if (*iter != ' ' && prev == ' ')
			result++;
	return result;
}


int main(int argc, char** argv)
{
	
	if (argc < 2) {
		printf("Too few arguments\n");
		return -1;
	}
	
	
	FILE* file = fopen(argv[0], "r");
	
	int data_size = 16;
	char** data = (char**) calloc (sizeof(char*), data_size);
	char buffer[4096];
	
	int i = 0;
	int count = 0;

	while (fscanf(file, "%4095[^\r\n]\n", buffer) > 0) {
		data[i] = (char*) calloc (sizeof(char), strlen(buffer));
		strcpy(data[i], buffer);
		if (i + 1 == data_size) {
			data_size += 16;
			data = (char**) realloc (data, sizeof(char*) * data_size);
			if (data == NULL) {
				fprintf(stderr, "Can't allocate memory\n");
				return -1;
			}
		}
		printf("%s\n", data[i]);
		i++;
		count++;
	}
	
	//printf("Count = %d\n", count);

	pid_t* process = (pid_t*) calloc (sizeof(pid_t), count);

	pid_t current_pid = 0;

	for (int i = 0; i < count; i++) {
		current_pid = fork();
		if (current_pid == -1) {
			fprintf(stderr, "Fork error %s\n", strerror(errno));
		}
		if (current_pid == 0) {
			int delay = 0;
			if (sscanf(data[i], "%d", &delay) != 1 || delay < 0) {
				fprintf(stderr, "Bad format\n");
				return -1;
			}
			else {
				sleep(delay);
				int new_argc = strwcnt(data[i]) - 1;
				char** new_argv = (char**) calloc (sizeof(char*), new_argc + 2);			
				char* tok = strtok(data[i], " ");
				tok = strtok(NULL, " ");
				for (int j = 0; tok != NULL; j++) {
					new_argv[j] = (char*) calloc (sizeof(char*), strlen(tok) + 1);
					if (new_argv[j] == NULL) {
						fprintf(stderr, "Can't allocate memory for new_argv\n");
						return -1;
					}
					strcpy(new_argv[j], tok);
					tok = strtok(NULL, " ");
				}

				printf("Execvp: \"");
				for (int j = 0; j < new_argc; j++)
					printf("%s ", new_argv[j]);
				printf("\" delay: %d\n", delay);

				execvp(new_argv[0], new_argv);
				printf("Execvp failed\n");
			}
			break;	
		}
		else {
			process[i] = current_pid;
			printf("Schedule: \"%s\", pid: %d\n", data[i], (int)current_pid);
		}
	}

	if (current_pid != 0) {
		int runned = count;
		while (runned > 0) {
			int status = 0;
			pid_t pid = wait(&status);
			if (pid == -1) 
				fprintf(stderr, "Wait error %s\n", strerror(errno));
			if (WIFEXITED(status)) 
				runned--;
			printf("Exited pid: %d\n", (int)pid);
		}
		for (int i = 0; i < count; i++)
			free(data[i]);
		free(data);
	}

	return 0;
}
