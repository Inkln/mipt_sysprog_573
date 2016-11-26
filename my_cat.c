#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

void cat(int fd)
{
	char* buffer = (char*) calloc (sizeof(char), BUFFER_SIZE);
	int x;

	while ((x = read(fd, buffer, BUFFER_SIZE)) > 0)
	{
		int writed = 0;
		int cur;
		while (writed < x)
		{
			cur = write(1, buffer + writed, x - writed);
			if (cur == -1)
			{
				perror("write");
				goto free;
			}
			writed += cur;
		}
	}
	if ( x == -1 )
		perror ("read");
	
free:
	free(buffer);
	return;
}

int main(int argc, char** argv)
{
	int fd;
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-"))
			fd = 0;
		else
			fd = open(argv[i], O_RDONLY, 0666);

		if (fd != -1)
		{
			cat(fd);
			if (fd != 0 && close(fd) == -1)
				perror("close");
		}
		else
			perror(argv[i]);
	}
	if (argc == 1)
		cat(0);
	
	return 0;
}

