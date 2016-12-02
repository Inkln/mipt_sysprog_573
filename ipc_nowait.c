#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#include <errno.h>

struct msgbuf {
    long mtype;
    char mtext[1];
};

int main(void)
{
    struct msgbuf message = {};
    int key = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (key == -1)
    {
	perror("Key creating error");
	return -1;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
	for(int i = 0; i < 6; i++)
	{
	    message.mtype = i + 1;
	    if (msgsnd(key, &message, sizeof(struct msgbuf), 0) == -1)
		perror("Send error");
	    printf("Sent\n");
	    sleep(1);    
	}
    }
    else
    {
	for (int i = 0; i < 6; ) {
	    if (msgrcv(key, &message, sizeof(struct msgbuf), 6 - i, IPC_NOWAIT) == -1)
		perror("MSGRCV");
	    else {
		printf("Received %d\n", i);
		i++;
	    }
	    usleep(100000);
	}
    }
    return 0;
}
