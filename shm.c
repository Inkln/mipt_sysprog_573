#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/sem.h>

#include <errno.h>
#include <getopt.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048
#define YOU_CAN_READ 1
#define YOU_CAN_WRITE 2

struct mbuf {
	long mtype;
	char mtext[1];
};

int reader(int shm_id, int msg_id, const char* destination)
{
    char* shared_memory = (char*) shmat (shm_id, NULL, SHM_RDONLY);
	if ((int) shared_memory == -1) {
		perror("Reader could not attach shared memory");
		return EXIT_FAILURE;
	}

	int fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
		perror("Reader could not open file");
		return EXIT_FAILURE;
    }

    int x, sum;
    struct mbuf message;

	message.mtype = YOU_CAN_WRITE;
	if( msgsnd(msg_id, &message, sizeof(message.mtext), 0) == -1) {
		perror("Reader could not send message for writing");
		return EXIT_FAILURE;
	}

    while (1) {
        if( msgrcv(msg_id, &message, sizeof(message.mtext), YOU_CAN_READ, 0) == -1) {
			perror("Reader could not receive message for reading");
			return EXIT_FAILURE;
		}

		memcpy(&x, shared_memory, sizeof(int));
		sum = 0;
		while (sum < x) {
			sum += write(fd, shared_memory + sum + 4, x - sum);
		}

        message.mtype = YOU_CAN_WRITE;
		if( msgsnd(msg_id, &message, sizeof(message.mtext), 0) == -1) {
			perror("Reader could not send message for writing");
			return EXIT_FAILURE;
		}

        if (x <= 0)
			break;
    }

	if (close(fd) == -1) {
        perror("Reader could not close file");
        return EXIT_FAILURE;
	}

	if (shmdt(shared_memory) == -1) {
		perror("Reader could not detach shared memory");
		return EXIT_FAILURE;
	}
    return EXIT_SUCCESS;
}


int writer(int shm_id, int msg_id, const char* source)
{
    char* shared_memory = (char*) shmat (shm_id, NULL, SHM_RND);
	if ((int) shared_memory == -1) {
		perror("Writer could not attach shared memory");
		return EXIT_FAILURE;
	}

    int fd = open(source, O_RDONLY, 0666);
    if (fd == -1) {
		perror("Writer could not open file");
		return EXIT_FAILURE;
    }

    int x;
    struct mbuf message;

    while (1) {
        if( msgrcv(msg_id, &message, sizeof(message.mtext), YOU_CAN_WRITE, 0) == -1) {
			perror("Writer could not receive message for writing");
			return EXIT_FAILURE;
		}

		x = read(fd, shared_memory + sizeof(int), BUFFER_SIZE);
		if (x == -1) {
			perror("Writer, read error");
			return EXIT_FAILURE;
		}
		memcpy(shared_memory, &x, sizeof(int));

		message.mtype = YOU_CAN_READ;
		if( msgsnd(msg_id, &message, sizeof(message.mtext), 0) == -1) {
			perror("Writer could not send message for reading");
			return EXIT_FAILURE;
		}
        if (x <= 0)
			break;
    }

	if (close(fd) == -1) {
        perror("Writer could not close file");
        return EXIT_FAILURE;
	}
	if (shmdt(shared_memory) == -1) {
		perror("Writer could not detach shared memory");
		return EXIT_FAILURE;
	}
    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		printf("Too few arguments\n");
		return EXIT_FAILURE;
	}
	int token = ftok(argv[0], 10);

	int shm_id = shmget (token, BUFFER_SIZE + sizeof(int), 0666 | IPC_CREAT);
    if (shm_id == -1) {
		perror("Base could not get shared memory");
		return EXIT_FAILURE;
    }

    int msg_id = msgget(token, IPC_CREAT | 0666);
    if (msg_id == -1) {
		perror("Base could not get message queue");
		return EXIT_FAILURE;
    }

    if (fork() == 0) {
		writer(shm_id, msg_id, argv[1]);
		return EXIT_SUCCESS;
    } else if (fork() == 0) {
		reader(shm_id, msg_id, argv[2]);
		return EXIT_SUCCESS;
    } else {
    	wait(NULL);
    	wait(NULL);
    }


	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		perror("Base could not remove memory");
		return EXIT_FAILURE;
    }
    if (msgctl(msg_id, IPC_RMID, 0) == -1) {
		perror("Base could not remove message queue");
		return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
