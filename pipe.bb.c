#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <errno.h>

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
    for (char* iter = (char*) str; *iter; prev = *iter, iter++)
        if (*iter != ' ' && prev == ' ')
			result++;
	return result;
}

int main(int argc, char** argv)
{
	char* input_buffer = (char*) calloc (sizeof(char), 4096);
	if (input_buffer == NULL) {
		perror("Allocate memory for input_buffer");
		return EXIT_FAILURE;
	}

	printf("Command line: ");
	scanf("%4095[^\r\n]", input_buffer);
	
	int data_size = strcnt(input_buffer, '|') + 1;
    char** data = (char**) calloc (sizeof(char*), data_size + 2);
	if (data == NULL) {
		perror("Allocate memory for data");
		return EXIT_FAILURE;
	}

	char* tok = strtok(input_buffer, "|");
	for (int i = 0; tok != NULL; i++, tok = strtok(NULL, "|")) {
		data[i] = (char*) calloc (sizeof(char), strlen(tok) + 1);
		if (data[i] == NULL) {
			perror("Allocate memory for data's subarray");
			return EXIT_FAILURE;
		}
		strcpy(data[i], tok);
	}

	////////////////////////////////////////////////////
	// pr 0 -> pipe0 -> pr1 -> pipe1 ... -> pipe_{n-1} 

    int* pipes = (int*) calloc (sizeof(int) * 2, data_size + 2);
    for (int i = 0; i <= data_size; i++)
        if (pipe(pipes + 2 * i) == -1)
			perror("Pipe error");

	pid_t current_pid = 0;

    for (int i = 0; i < data_size; i++) {
        current_pid = fork();
        if (current_pid == 0) {

			int new_argc = strwcnt(data[i]);

            char** new_argv = (char**) calloc (sizeof(char*), new_argc + 1);
			if (new_argv == NULL) {
				perror("Allocate memory for new_argv");
				return EXIT_FAILURE;
			}

            char* tok = strtok(data[i], " ");
            for (int j = 0; tok != NULL; j++, tok = strtok(NULL, " ")) {

                new_argv[j] = (char*) calloc (sizeof(char), strlen(tok) + 1);
				if (new_argv[j] == NULL) {
					perror("Allocate memory for new_argv's subarray");
					return EXIT_FAILURE;
				}

                strcpy(new_argv[j], tok);
			}

			if (i > 0)
				if (dup2(*(pipes + 2 * i - 2), 0) == -1) {
					perror("dup2 stdin");
					return EXIT_FAILURE;
				}
			if (dup2(*(pipes + 2 * i + 1), 1) == -1) {
				perror("dup2 stdout");
				return EXIT_FAILURE;
			}

			execvp(new_argv[0], new_argv);
			perror("Execvp");

            break;
        }
		else {
			waitpid(current_pid, NULL, 0);
            close(*(pipes + 2 * i + 1));
		}
    }

    if (current_pid != 0) {

		for (int i = 0; i + 1 < data_size; i++)
			close(*(pipes + 2 * i));

		int bytes = 0;
		int lines = 0;
		int words = 0;
		char last = '-';

		const int BUFF_SIZE = 1024 * 16;

		char buffer[BUFF_SIZE];
		int x = -1;

		while ((x = read(*(pipes + 2 * data_size - 2), buffer, BUFF_SIZE)) > 0) {
		    bytes += x;
		    write(1, buffer, x);

		    for (int i = 0; i < x; i++)
			if (buffer[i] == '\n')
			    lines++;
		    for (int i = 0; i + 1 < x; i++)
				if (isalpha(buffer[i]) || isdigit(buffer[i]))
				    if (!isalpha(buffer[i + 1]) && !isdigit(buffer[i + 1]))
						words++;
			last = buffer[x - 1];
		}

		close(*(pipes + 2 * data_size - 2));

		if (isalpha(last) || isdigit(last))
			words++;

		if (x == -1) {
			perror("Read pipe error");
		}
		printf("Output size: bytes=%d, words = %d, lines=%d\n", bytes, words, lines);
    }

	///////////////////////////////////////////////////
	if (current_pid != 0) {
		free(input_buffer);
		for (int i = 0; i < data_size; i++)
			free(data[i]);
		free(data);
	}
	return 0;
}
