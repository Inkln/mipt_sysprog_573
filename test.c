#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char** argv)
{
	printf("Test program\n");
	fflush(stdout);
	if (argc >= 2 && !strcmp(argv[1], "return"))
		return atoi(argv[2]);
	else if (argc >= 2 && !strcmp(argv[1], "sleep")) 
		sleep(atoi(argv[2]));
	else if (argc >= 2 && !strcmp(argv[1], "usleep")) 
		usleep(atoi(argv[2]));
	else if (argc >= 1 && !strcmp(argv[1], "sigsegv"))
		return *(int*)(0x123521231);
	return 0;
}

