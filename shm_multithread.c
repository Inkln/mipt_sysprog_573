#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

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

#define DEBUG
#include "debug.h"


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

int writer_to_shared_memory(int shm_id, int sem_id, const char* filename)
{
	int ret_code = EXIT_SUCCESS;

	// attach shared memory
    void* shared_memory = shmat(shm_id, NULL, SHM_RND);
    if (shared_memory == (void*)(-1)) {
		PERROR("Shared memory wan not attached");
		ret_code = EXIT_FAILURE;
		goto writer_free_section;
    }
    else
		log("shared memory was attached to writer process");

	// open file to input_descriptor for read
	int input_descriptor = open(filename, O_RDONLY, 0666);
	if (input_descriptor == -1) {
		PERROR("\"%s\" was not opened for read only", filename);
		ret_code = EXIT_FAILURE;
		goto writer_free_section;
	}
	else
		log("\"%s\" opened for read only", filename);

    int work_path = 0;
    int data_transfer_size;
    void* buffer = shared_memory;

	log("writing process started");
	while (1) {
		lock(sem_id, SEM_WRITE(work_path));

		data_transfer_size = read(input_descriptor, buffer + sizeof(int), BUFFER_SIZE - sizeof(int));
		if (data_transfer_size == -1) {
			PERROR("Writer, read error, work_path=%d", work_path);
			ret_code = EXIT_FAILURE;
			goto writer_free_section;
		}

		memcpy(buffer, &data_transfer_size, sizeof(int));

		log("%d bytes was copied from \"%s\" to buffer[%d]", data_transfer_size + (int)sizeof(int), filename, work_path);
		unlock(sem_id, SEM_READ(work_path));

        if (data_transfer_size <= 0)
			break;

        work_path = (work_path + 1) % BUFFER_COUNT;
        buffer = shared_memory + (work_path * BUFFER_SIZE);
    }


writer_free_section:
	log("writing process finished");

	// close input file
	if (close(input_descriptor) == -1) {
		PERROR("\"%s\" was not closed", filename);
	}
	else
		log("\"%s\" was closed from read only mode", filename);

	// detach shared_memory
    if (shmdt(shared_memory) == -1) {
		PERROR("Shared memory was not detached");
    }
    else
		log("shared memory was detached from writer");
    return ret_code;
}

//////////////////////////////////////////////////////////////////////////////
//                             #
//     m mm   mmm    mmm    mmm#   mmm    m mm
//     #"  " #"  #  "   #  #" "#  #"  #   #"  "
//     #     #""""  m"""#  #   #  #""""   #
//     #     "#mm"  "mm"#  "#m##  "#mm"   #
//
//////////////////////////////////////////////////////////////////////////////

int reader_from_shared_memory(int shm_id, int sem_id, const char* filename)
{
	int ret_code = EXIT_SUCCESS;

	// attach shared memory
    void* shared_memory = shmat(shm_id, NULL, SHM_RDONLY);
    if (shared_memory == (void*)(-1)) {
		PERROR("Shared memory wan not attached");
		ret_code = EXIT_FAILURE;
		goto reader_free_section;
    }
    else
		log("shared memory was attached to writer process");

	// open file to input_descriptor for read
	int output_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (output_descriptor == -1) {
		PERROR("\"%s\" was not opened for writing and truncated", filename);
		ret_code = EXIT_FAILURE;
		goto reader_free_section;
	}
	else
		log("\"%s\" was opened for writing and truncated", filename);

    int work_path = 0;
    int sum = 0;
    int data_transfer_size;
    void* buffer = shared_memory;

	log("reading process started");
	while (1) {
		lock(sem_id, SEM_READ(work_path));

		memcpy(&data_transfer_size, buffer, sizeof(int));
		sum = 0;
		while (sum < data_transfer_size) {
			sum += write(output_descriptor, buffer + sum + sizeof(int), data_transfer_size - sum);
		}

		log("%d bytes was copied from buffer[%d] to \"%s\"", data_transfer_size + (int)sizeof(int), work_path, filename);
		unlock(sem_id, SEM_WRITE(work_path));

        if (data_transfer_size <= 0)
			break;

        work_path = (work_path + 1) % BUFFER_COUNT;
        buffer = shared_memory + (work_path * BUFFER_SIZE);
    }

reader_free_section:
	log("reading process finished");

	// close output file
	if (close(output_descriptor) == -1) {
		PERROR("\"%s\" was not closed", filename);
	}
	else
		log("\"%s\" was closed from write only mode", filename);
	// detach shared_memory
    if (shmdt(shared_memory) == -1) {
		PERROR("Shared memory was not detached");
    }
    else
		log("shared memory was detached from reader");
    return ret_code;
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
	log("token was received (%d)", token);

    // shared memory allocation
    int shm_id = shmget(token, BUFFER_COUNT * BUFFER_SIZE, 0666 | IPC_CREAT);
    if (shm_id == -1) {
		PERROR("Shared memory was not allocated");
		goto free_section;
    }
    else
		log("shared memory was received with id=%d", shm_id);

    // semafors allocation
    int sem_id = semget(token, BUFFER_COUNT * 2, IPC_CREAT | 0666);
    if (sem_id == -1) {
        PERROR("Semafor was not created");
        goto free_section;
    }
    else
		log("semafors were received with id=%d", sem_id)

	// set values for semafors
    for (int i = 0; i < BUFFER_COUNT; i++)
		if (unlock(sem_id, SEM_WRITE(i)) == -1) {
            PERROR("SEM_WRITE(%d)=%d initial unlock failed", i, SEM_WRITE(i));
            goto free_section;
		}

	pid_t reader_process_id = 0;
	pid_t writer_process_id = 0;

	int status = 0;
	pid_t wait_pid = 0;


	if ((writer_process_id = fork()) == 0) {
		log("writer process created with id=%d", writer_process_id);
		return writer_to_shared_memory(shm_id, sem_id, argv[1]);
    } else if ((reader_process_id = fork()) == 0) {
    	log("reader process createdwith id=%d", reader_process_id);
		return reader_from_shared_memory(shm_id, sem_id, argv[2]);
    } else {
    	for (int i = 0; i < 2; i++) {
			wait_pid = wait(&status);
			if (WEXITSTATUS(status) == EXIT_FAILURE) {
				if (wait_pid == reader_process_id) {
					log("reader finished with EXIT_FAILURE");
					if (kill(writer_process_id, 11) == -1) {
						PERROR("kill writer error, process_id=%d", (int)writer_process_id);
					} else
						log("writer killed, process_id=%d", (int)writer_process_id);
				} else if (wait_pid == writer_process_id) {
					log("writer finished with EXIT_FAILURE");
					if (kill(reader_process_id, 11) == -1) {
						PERROR("kill reader error, process_id=%d", (int)writer_process_id);
					} else
						log("reader killed, process_id=%d", (int)writer_process_id);
				}
			} else if (WEXITSTATUS(status) == EXIT_SUCCESS && wait_pid == reader_process_id) {
				log("reader finished correctly");
			} else if (WEXITSTATUS(status) == EXIT_SUCCESS && wait_pid == writer_process_id) {
				log("writer finished correctly");
			}

    	}
    }

free_section:
	log("all child process finished");
	// free semafors
    if (semctl(sem_id, IPC_RMID, 0) == -1) {
        PERROR("Semafor was not removed, id=%d", sem_id);
	}
	else
		log("semafors was removed, id=%d", sem_id);

    // shared memory free
	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		PERROR("Shared memory was not free, id=%d", shm_id);
	}
	else
		log("shared memory was removed, id=%d", shm_id);

    return EXIT_SUCCESS;
}
