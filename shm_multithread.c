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
#define BUFFER_COUNT 16

#define SEM_READ(N) (2 * N)
#define SEM_WRITE(N) (2 * N + 1)

#define PERROR(format, ...) { fprintf(stderr, format, ##__VA_ARGS__); fprintf(stderr, " "); perror("log:"); }

//////////////////////////////////////////////////////////////////////////////
// this function locks critical section
inline int lock(int sem_id, int sem_num)
{
    struct sembuf command = {sem_num, -1, 0};
    return semop(sem_id, &command, 1);
}

// this function unlocks critical section
inline int unlock(int sem_id, int sem_num)
{
    struct sembuf command = {sem_num, 1, 0};
    return semop(sem_id, &command, 1);
}

//////////////////////////////////////////////////////////////////////////////
//                     "      m
//    m     m  m mm  mmm    mm#mm   mmm    m mm
//    "m m m"  #"  "   #      #    #"  #   #"  "
//     #m#m#   #       #      #    #""""   #
//      # #    #     mm#mm    "mm  "#mm"   #
//
//////////////////////////////////////////////////////////////////////////////

void writer_to_shared_memory(int shm_id, int sem_id, const char* filename)
{
	// attach shared memory
    void* shared_memory = shmat(shm_id, NULL, SHM_RND);
    if ((int)shared_memory == -1) {
		PERROR("Shared memory wan not attached");
		goto writer_free_section;
    }

	// open file to input_descriptor for read
	int input_descriptor = open(filename, O_RDONLY, 0666);
	if (input_descriptor == -1) {
		PERROR("File was not opened");
		goto writer_free_section;
	}

    int work_path = 0;
    int data_transfer_size;
    void* buffer = shared_memory;

	while (1) {
		lock(sem_id, SEM_WRITE(work_path));

		data_transfer_size = read(input_descriptor, buffer + sizeof(int), BUFFER_SIZE - sizeof(int));
		if (data_transfer_size == -1) {
			PERROR("Writer, read error, work_path=%d", work_path);
			return;
		}

		memcpy(buffer, &data_transfer_size, sizeof(int));

		unlock(sem_id, SEM_READ(work_path));

        if (data_transfer_size <= 0)
			break;

        work_path = (work_path + 1) % BUFFER_COUNT;
        buffer = shared_memory + (work_path * BUFFER_SIZE);
    }


writer_free_section:
	// close input file
	if (close(input_descriptor) == -1) {
		PERROR("File was not closed");
	}
	// detach shared_memory
    if (shmdt(shared_memory) == -1) {
		PERROR("Shared memory was not detached");
    }
    return;
}

//////////////////////////////////////////////////////////////////////////////
//                             #
//     m mm   mmm    mmm    mmm#   mmm    m mm
//     #"  " #"  #  "   #  #" "#  #"  #   #"  "
//     #     #""""  m"""#  #   #  #""""   #
//     #     "#mm"  "mm"#  "#m##  "#mm"   #
//
//////////////////////////////////////////////////////////////////////////////

void reader_from_shared_memory(int shm_id, int sem_id, const char* filename)
{
	// attach shared memory
    void* shared_memory = shmat(shm_id, NULL, SHM_RDONLY);
    if ((int)shared_memory == -1) {
		PERROR("Shared memory wan not attached");
		goto reader_free_section;
    }

	// open file to input_descriptor for read
	int output_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (output_descriptor == -1) {
		PERROR("File was not opened");
		goto reader_free_section;
	}

    int work_path = 0;
    int sum = 0;
    int data_transfer_size;
    void* buffer = shared_memory;

	while (1) {
		lock(sem_id, SEM_READ(work_path));

		memcpy(&data_transfer_size, buffer, sizeof(int));
		sum = 0;
		while (sum < data_transfer_size) {
			sum += write(output_descriptor, buffer + sum + sizeof(int), data_transfer_size - sum);
		}

		unlock(sem_id, SEM_WRITE(work_path));

        if (data_transfer_size <= 0)
			break;

        work_path = (work_path + 1) % BUFFER_COUNT;
        buffer = shared_memory + (work_path * BUFFER_SIZE);
    }


reader_free_section:
	// close output file
	if (close(output_descriptor) == -1) {
		PERROR("File was not closed");
	}
	// detach shared_memory
    if (shmdt(shared_memory) == -1) {
		PERROR("Shared memory was not detached");
    }
    return;
}

//////////////////////////////////////////////////////////////////////////////
//                    "
//    mmmmm   mmm   mmm    m mm
//   # # #  "   #    #    #"  #
//   # # #  m"""#    #    #   #
//   # # #  "mm"#  mm#mm  #   #
//
//////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv, char** env)
{
	// check arguments count
	if (argc < 3) {
		printf("Too few arguments\n");
		return EXIT_FAILURE;
	}

	// create system V token
	int token = ftok(argv[0], 0);

    // shared memory allocation
    int shm_id = shmget(token, BUFFER_COUNT * BUFFER_SIZE, 0666 | IPC_CREAT);
    if (shm_id == -1) {
		PERROR("Shared memory was not allocated");
		goto free_section;
    }

    // semafors allocation
    int sem_id = semget(token, BUFFER_COUNT * 2, IPC_CREAT | 0666);
    if (sem_id == -1) {
        PERROR("Semafor was not created");
        goto free_section;
    }

	// set values for semafors
    for (int i = 0; i < BUFFER_COUNT; i++)
		if (unlock(sem_id, SEM_WRITE(i)) == -1) {
            PERROR("SEM_WRITE(%d)=%d initial unlock failed", i, SEM_WRITE(i));
            goto free_section;
		}


    if (fork() == 0) {
		writer_to_shared_memory(shm_id, sem_id, argv[1]);
		return EXIT_SUCCESS;
    } else if (fork() == 0) {
		reader_from_shared_memory(shm_id, sem_id, argv[2]);
		return EXIT_SUCCESS;
    } else {
    	wait(NULL);
    	wait(NULL);
    }


free_section:
	// free semafors
    if (semctl(sem_id, IPC_RMID, NULL) == -1) {
        PERROR("Semafor was not removed");
	}

    // shared memory free
	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		PERROR("Shared memory was not free");
	}
    return EXIT_SUCCESS;
}
