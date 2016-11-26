#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include <errno.h>

#define PASS 8
#define BOAT_SIZE 4
#define K 3

#define SEM_BOAT 0
#define SEM_COUNT 1
#define SEM_TRAP_IN 2
#define SEM_TRAP_OUT 3
#define SEM_GO_HOME 4
#define SEM_START_TOUR 5
#define SEM_STOP_TOUR 6

#define SET(command, opt_num, opt_op, opt_flg) \
		command.sem_num = opt_num; \
		command.sem_op = opt_op; \
		command.sem_flg = opt_flg;

void pass(int semid, int pass_id)
{
	struct sembuf command;
	printf("[%d] Здрасти\n", pass_id);

	while (1) {
		while (1) {
			SET(command, SEM_GO_HOME, -1, IPC_NOWAIT)
			if (semop(semid, &command, 1) == -1) {
				if (errno != EAGAIN)
					perror("PASS::CHECK");
			}
			else {
				printf("[%d] Ну ок, я домой\n", pass_id);
				return;
			}

			SET(command, SEM_BOAT, -1, IPC_NOWAIT);
			if (semop(semid, &command, 1) == -1) {
				if (errno != EAGAIN)
					perror("PASS::WAIT_BOAT");
			}
			else
				break;
		}

		SET(command, SEM_TRAP_IN, -1, 0);
		if (semop(semid, &command, 1) == -1) perror("PASS::WAIT_TRAP_IN ON");
			printf("[%d] Иду по трапу на борт\n", pass_id);

		SET(command, SEM_TRAP_IN, 1, 0);
		if (semop(semid, &command, 1) == -1) perror("PASS::WAIT_TRAP_IN OFF");

		SET(command, SEM_COUNT, 1, 0)
		if (semop(semid, &command, 1) == -1) perror("PASS::COUNT_ME ON");


		SET(command, SEM_START_TOUR, -1, 0);
		if (semop(semid, &command, 1) == -1) perror("PASS::WAIT START");
		printf("[%d] Ура, меня покатали!\n", pass_id);
		SET(command, SEM_STOP_TOUR, 1, 0);
		if (semop(semid, &command, 1) == -1) perror("PASS::WAIT START");

		SET(command, SEM_TRAP_OUT, -1, 0);
		if (semop(semid, &command, 1) == -1) perror("PASS::WAIT_TRAP_OUT ON");
			printf("[%d] Иду по трапу в сторону берега\n", pass_id);

		SET(command, SEM_TRAP_OUT, 1, 0);
		if (semop(semid, &command, 1) == -1) perror("PASS::WAIT_TRAP_OUT OFF");

		SET(command, SEM_COUNT, 1, 0)
		if (semop(semid, &command, 1) == -1) perror("PASS::COUNT_ME OFF");
	}
}

void boat(int semid)
{
	struct sembuf command;
	printf("I am boat\n");
    for (int ii = 0; ii < K; ii++)
	{
		printf("Сажаю народ\n");
		if (semctl(semid, SEM_COUNT, SETVAL, 0) == -1) perror("BOAD::SET SEM_COUNT");
		if (semctl(semid, SEM_BOAT, SETVAL, BOAT_SIZE) == -1) perror("BOAD::SET SEM_BOAT");
		if (semctl(semid, SEM_TRAP_IN, SETVAL, 1) == -1) perror("BOAD::SET SEM_TRAP_IN");

		for (int i = 0; i < BOAT_SIZE; i++) {
			SET(command, SEM_COUNT, -1, 0);
			if (semop(semid, &command, 1) == -1) perror("BOAT::WAIT");
		}
		if (semctl(semid, SEM_TRAP_IN, SETVAL, 1) == -1) perror("BOAD::SET SEM_TRAP_IN");
		printf("Все сели, поплаваем \n");

		if (semctl(semid, SEM_STOP_TOUR, SETVAL, 0) == -1) perror("BOAD::SET SEM_STOP_TOUR");
		if (semctl(semid, SEM_START_TOUR, SETVAL, BOAT_SIZE) == -1) perror("BOAD::SET SEM_START_TOUR");

		for (int i = 0; i < BOAT_SIZE; i++) {
			SET(command, SEM_STOP_TOUR, -1, 0);
			if (semop(semid, &command, 1) == -1) perror("BOAT::WAIT");
		}

		printf("Выгружаемся\n");
		if (semctl(semid, SEM_COUNT, SETVAL, 0) == -1) perror("BOAD::SET SEM_BOAT");
		if (semctl(semid, SEM_TRAP_OUT, SETVAL, 1) == -1) perror("BOAD::SET SEM_TRAP_OUT");
		for (int i = 0; i < BOAT_SIZE; i++) {
			SET(command, SEM_COUNT, -1, 0);
			if (semop(semid, &command, 1) == -1) perror("BOAT::WAIT");
		}
		if (semctl(semid, SEM_TRAP_OUT, SETVAL, 0) == -1) perror("BOAD::SET SEM_TRAP_OUT");
	}
	printf("Бензин кончился, больше не катаю \n");
	 if (semctl(semid, SEM_GO_HOME, SETVAL, PASS) == -1) perror("BOAT::SET SEM_GO_HOME");
}

int main(int argc, char** argv)
{
    int semid = semget(IPC_PRIVATE, 10, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1) {
		perror("SEMGET");
		return -1;
    }

    if (semctl(semid, SEM_GO_HOME, SETVAL, 0) == -1) perror("GOD::SET SEM_GO_HOME");

	pid_t cur_pid = -1;
	pid_t process[PASS + 1] = {};

	for (int i = 0; i <= PASS; i++) {
		cur_pid = fork();
		if (cur_pid == 0) {
			if (i == 0)	boat(semid);
			else pass(semid, i);
			break;
		}
		else {
			process[i] = cur_pid;
		}
	}

	if (cur_pid != 0) {
		int runned = PASS + 1;
		int status;
		while (runned > 0) {
			pid_t wpid = wait(&status);
			if (WIFEXITED(status))
				runned--;
		}
		semctl(semid, IPC_RMID, 0);
	}
	return 0;
}
