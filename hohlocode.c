#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include <errno.h>

#define PASS 6
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

#define SETSEM(SEM_NAME, VALUE) \
	if (semctl(semid, SEM_NAME, SETVAL, VALUE) == -1) perror("SETVAL " );
#define V(SEM_NAME) \
	{   struct sembuf command = {SEM_NAME,  1, 0}; if (semop(semid, &command, 1) == -1) { perror("SEM V " );    return EXIT_FAILURE; } }
#define VK(SEM_NAME, K) \
	{   struct sembuf command = {SEM_NAME,  K, 0}; if (semop(semid, &command, 1) == -1) { perror("SEM VK " );   return EXIT_FAILURE; } }
#define P(SEM_NAME) \
	{   struct sembuf command = {SEM_NAME, -1, 0}; if (semop(semid, &command, 1) == -1) { perror("SEM P " );    return EXIT_FAILURE; } }
#define PK(SEM_NAME, K) \
	{   struct sembuf command = {SEM_NAME, -K, 0}; if (semop(semid, &command, 1) == -1) { perror("SEM PK " );   return EXIT_FAILURE; } }
#define WAIT(SEM_NAME) \
	{   struct sembuf command = {SEM_NAME,  0, 0}; if (semop(semid, &command, 1) == -1) { perror("SEM WAIT " ); return EXIT_FAILURE; } }
//#define TRY_LOCK(SEM_NAME) \
//	{   struct sembuf command = {SEM_NAME, -1, IPC_NOWAIT}; }
#define printf(...) { \
		int lock_fd = -1; \
		while ((lock_fd = open("/tmp/hohlolock", O_CREAT | O_EXCL, 0666)) == -1) usleep(10000); \
		printf(__VA_ARGS__); close(lock_fd); unlink("/tmp/hohlolock"); \
	}
void pass(int semid, int pass_id)
{
	printf("[%d] Здрастиi\n", pass_id);

	while (1) {
		P(SEM_BOAT);
		P(SEM_TRAP_IN);
		printf("[%d] Иду по трапу на борт\n", pass_id);
		V(SEM_TRAP_IN);
		V(SEM_COUNT);
		P(SEM_START_TOUR);
		printf("[%d] Ура, меня покатали!\n", pass_id);
		V(SEM_STOP_TOUR);
		P(SEM_TRAP_OUT);
		printf("[%d] Иду по трапу в сторону берега\n", pass_id);
		V(SEM_TRAP_OUT);
		V(SEM_COUNT);
	}
}

void boat(int semid)
{
	struct sembuf command;
	printf("Здрасти, я лодка\n");
    for (int ii = 0; ii < K; ii++)
	{
		printf("Сажаю народ\n");

		VK(SEM_BOAT, BOAT_SIZE)
		V(SEM_TRAP_IN);

		PK(SEM_COUNT, BOAT_SIZE);
		P(SEM_TRAP_IN);

		printf("Все сели, поплаваем \n");

		VK(SEM_START_TOUR, BOAT_SIZE);
		PK(SEM_STOP_TOUR, BOAT_SIZE);

		printf("Выгружаемся\n");

		V(SEM_TRAP_OUT);
		for (int i = 0; i < BOAT_SIZE; i++)
			P(SEM_COUNT);

		P(SEM_TRAP_OUT);
	}
	printf("Бензин кончился, больше не катаю \n");
	VK(SEM_GO_HOME, PASS);
	return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    int semid = semget(IPC_PRIVATE, 20, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1) {
		perror("SEMGET");
		return EXIT_FAILURE;
    }

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
	return EXIT_SUCCESS;
}
