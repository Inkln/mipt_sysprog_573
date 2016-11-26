#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#include <errno.h>
#include <opt.h>

// message types definition
#define N 3
#define MSG_COME_IN(i)			(i + 1 * (N + 1))
#define MSG_TO_START(i)			(i + 2 * (N + 1))
#define MSG_READY_TO_START(i)	(i + 3 * (N + 1))
#define MSG_GO(i)				(i + 4 * (N + 1))
#define RUNNER_ID(i)			(i % (N + 1))
#define PERROR(...) \
	{char __perror_buff[4096] = {}; sprintf(__perror_buff, __VA_ARGS__); perror(__perror_buff);}

// message structure definition
struct msgbuf {
	long mtype;
	char mtext[1];
};

// get first message from queue that have mtyp from range [brng; erng]
// if IPC_NOWAIT set and there isn't messages from the range, -1 will returned with errno set to ENOMSG
// else process will be blocked untill reading first message
long msgrcvrng(int key, void* message, size_t msgsize, int msgflags, int brng, int erng)
{
	if (msgflags & IPC_NOWAIT) {
        for (long msgtype = brng; msgtype <= erng; msgtype++)
			if (msgrcv(key, message, msgsize, msgtype, IPC_NOWAIT) == -1)
				if (errno != ENOMSG) {
					PERROR("msgrcvrng[%d-%d]::rcv[%d]", brng, erng, (int)msgtype);
				}
				else
					return msgtype;
		errno = ENOMSG;
		return -1;
	}
	else {
		while (1) {
			for (long msgtype = brng; msgtype <= erng; msgtype++)
				if (msgrcv(key, message, msgsize, msgtype, IPC_NOWAIT) == -1) {
					if (errno != ENOMSG) {
						PERROR("msgrcvrng[%d-%d]::rcv[%d]", brng, erng, (int)msgtype);
					}
				}
				else
					return msgtype;
		}
	}
}

// This is the runner's handler
void runner(int key, int runner_id)
{
	struct msgbuf message = {};

	// I come in
	message.mtype = MSG_COME_IN(runner_id);
	if (msgsnd(key, &message, sizeof(message.mtext), 0) == -1)
		PERROR("Runner[%d]::MSG_COME_IN::send", runner_id);

	// Go train and wait MSG_TO_START
	for (int i = 1; ; i++)
	{
		if (msgrcv(key, &message, sizeof(message.mtext), MSG_TO_START(runner_id), IPC_NOWAIT) == -1) {
			if (errno != ENOMSG)
				PERROR("Runner[%d]::MSG_TO_START::receive", runner_id);
		}
		else
			break;
		sleep(1);
		printf("Runner[%d] done %d training circle\n", runner_id, i); fflush(stdout);
	}
	printf("Runner[%d] finished training\n", runner_id);

	// Confirm status to start
	message.mtype = MSG_READY_TO_START(runner_id);
	if (msgsnd(key, &message, sizeof(message.mtext), 0) == -1) {
		PERROR("Runner[%d]::MSG_READY_TO_START::send", runner_id);
	}

	// Go run and finish
	if (msgrcv(key, &message, sizeof(message.mtext), MSG_GO(runner_id), 0) == -1) {
		PERROR("Runner[%d]::MSG_GO::receive", runner_id);
	}
	else {
		printf("Run..."); fflush(stdout);
		sleep(1);
		printf("Finished %d\n", runner_id);

		if (runner_id < N)  message.mtype = MSG_GO(runner_id + 1); // next runner
		else		    	message.mtype = MSG_GO(0);			   // judge id

		if (msgsnd(key, &message, sizeof(message.mtext), 0) == -1) {
			PERROR("Runner[%d]::MSG_GO::send", runner_id);
		}
	}
}

void judge(int key)
{
	struct msgbuf message = {};
	
	// Runners are coming in
	int ready = 0;
	while (ready < N)
	{
		long index = msgrcvrng(key, &message, sizeof(message.mtext), 0, MSG_COME_IN(1), MSG_COME_IN(N));
		printf("Runner[%d] come in\n", (int)RUNNER_ID(index));
		ready++;
	}

	// Sent message to start
	printf("All are ready! GO to start!\n");
	for (int i = 1; i <= N; i++) {
		message.mtype = MSG_TO_START(i);
		if (msgsnd(key, &message, sizeof(message.mtext), 0) == -1) {
			PERROR("Judge::MSG_TO_START::send");
		}
		printf("Judge send MSG_TO_START(%d)\n", i);
	}
	
	// waiting players on start line
	ready = 0;
	while (ready < N)
	{
		long index = msgrcvrng(key, &message, sizeof(message.mtext), 0, MSG_READY_TO_START(1), MSG_READY_TO_START(N));
		printf("Runner[%d] ready to start\n", (int)RUNNER_ID(index));
		ready++;
	}
	
	// pif-paf
	printf("GO!\n");

	message.mtype = MSG_GO(1);
	if (msgsnd(key, &message, sizeof(message.mtext), 0) == -1)
		PERROR("Judge::MSG_GO::send");
	
	// Waiting ilast runner
	if (msgrcv(key, &message, sizeof(message.mtext), MSG_GO(0), 0) == -1)
		perror("judge::MSG_GO::receive");
	else
		printf("All finished\n");
}

int main(int argc, char** argv)
{
	// 0 - judge
	// 1..n - runners

	int key = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
	if (key == -1) {
		perror("Key creating error");
		return -1;
	}

	pid_t cur_pid = -1;
	pid_t process[N + 1] = {};

	for (int i = 0; i <= N; i++) {
		cur_pid = fork();
		if (cur_pid == 0) {
			if (i == 0)	judge(key);
			else runner(key, i);
			break;
		}
		else {
			process[i] = cur_pid;
		}
	}

	if (cur_pid != 0) {
		int runned = N + 1;
		int status;
		while (runned > 0) {
			pid_t wpid = wait(&status);
			if (WIFEXITED(status))
				runned--;
		}
		if (msgctl(key, IPC_RMID, NULL) == -1)
			perror("msgctl");
	}
	return 0;
}
