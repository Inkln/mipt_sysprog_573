#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <pwd.h>
#include <grp.h>

#include <getopt.h>
#include <errno.h>

//#define DEBUG
#include "debug.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

int cmp_int(const void* __1, const void* __2)
{
	return *(int*)__1 - *(int*)__2;
}
///////////////////////////////////////////////
//	                               m
//	  mmmm   mmm    mmm    m mm  mm#mm
//	 #" "#  #   "  #" "#   #"  "   #
//	 #   #   """m  #   #   #       #
//	 "#m##  "mmm"  "#m#"   #       "mm
//	     #
//	     "
///////////////////////////////////////////////
void my_qsort(int* block, int size)
{
	if (size <= 1)
		return;
	if (size <= 8) { // the best constant for my computer
		int buffer;
		for (int i = 1; i < size; i++) {
			for (int j = i; j > 0 && block[j - 1] > block[j]; j--) {
				buffer = block[j];
				block[j] = block[j - 1];
				block[j - 1] = buffer;
			}
		}
		return;
	}

    int median = block[size / 2];
    int it1 = 0, it2 = size - 1, buffer;
    while (it1 < it2) {
		while (block[it1] < median)
			it1++;
		while (block[it2] > median)
			it2--;
		if (it1 < it2) {
			buffer = block[it1];
			block[it1] = block[it2];
			block[it2] = buffer;
			it1++;
			it2--;
		}
    }
    if (it1 == it2) {
		my_qsort(block, it1 + 1);
		my_qsort(block + it1, size - (it1));
    }
	else {
		my_qsort(block, it1);
		my_qsort(block + it1, size - it1);
	}
}

///////////////////////////////////////////////
// 	#        "    ""#    ""#
// 	#   m  mmm      #      #     mmm    m mm
// 	# m"     #      #      #    #"  #   #"  "
// 	#"#      #      #      #    #""""   #
// 	#  "m  mm#mm    "mm    "mm  "#mm"   #
///////////////////////////////////////////////
int kill_childs(pid_t* childs_id)
{
	int exit_status = EXIT_SUCCESS;
    for (pid_t* iterator = childs_id; *iterator; iterator++) {
		if (kill(*iterator, SIGKILL) == -1) {
			PERROR("process with id=%d was not killed correctly", (int)(*iterator));
			exit_status = EXIT_FAILURE;
		}
		else {
			log("process with id=%d was killed", (int)(*iterator));
		}
    }
    return exit_status;
}

///////////////////////////////////////////////
//	                        m
//	  mmm    mmm    m mm  mm#mm   mmm    m mm
//	 #   "  #" "#   #"  "   #    #"  #   #"  "
//	  """m  #   #   #       #    #""""   #
//	 "mmm"  "#m#"   #       "mm  "#mm"   #
///////////////////////////////////////////////
int sort_process(int shm_id, int shm_size, int begin_index, int end_index)
{
	if (end_index <= begin_index)
		return EXIT_SUCCESS;

	int exit_status = EXIT_SUCCESS;
	// attach shared memory
	int* shared_memory = (int*) shmat(shm_id, NULL, SHM_RND);
	if (shared_memory == (void*)(-1)) {
		PERROR("Shared memory wan not attached to sort");
		exit_status = EXIT_FAILURE;
		goto sort_free_section;
	}
	else
		log("shared memory was attached to sort process");

    qsort(shared_memory + begin_index, end_index - begin_index, sizeof(int), cmp_int);
	//my_qsort(shared_memory + begin_index, end_index - begin_index);

sort_free_section:
	// detach shared_memory
	if (shmdt(shared_memory) == -1) {
		PERROR("Shared memory was not detached from sort");
	}
	else
		log("shared memory was detached from sort");

	return exit_status;
}
///////////////////////////////////////////////
//	mmmmm   mmm    m mm   mmmm   mmm
//	# # #  #"  #   #"  " #" "#  #"  #
//	# # #  #""""   #     #   #  #""""
//	# # #  "#mm"   #     "#m"#  "#mm"
//	                      m  #
//           			   ""
///////////////////////////////////////////////


int merge(int* block, int size, int step)
{
	int* buffer = (int*) malloc (sizeof(int) * size);

	int* src = block;
	int* dest = buffer;
	int* tmp;

    int iter_step = step;
    while (iter_step < size) {
        for (int i = 0; i * iter_step < size; i += 2) {
            int first_begin = MIN(iter_step * i, size);
            int first_end = MIN(iter_step * (i + 1), size);

            int second_begin = MIN(iter_step * (i + 1), size);
            int second_end = MIN(iter_step * (i + 2), size);

            int it1 = first_begin, it2 = second_begin;
            int counter = first_begin;
            while (it1 < first_end && it2 < second_end) {
                if (src[it1] < src[it2]) {
					dest[counter] = src[it1];
					it1++;
					counter++;
                }
                else {
					dest[counter] = src[it2];
					it2++;
					counter++;
                }
            }
            while (it1 < first_end) {
				dest[counter] = src[it1];
				it1++;
				counter++;
            }
			while (it2 < second_end) {
				dest[counter] = src[it2];
				it2++;
				counter++;
            }
        }
        tmp = src, src = dest, dest = tmp;
        iter_step *= 2;
    }
    if (src != block)
        memcpy(block, src, sizeof(int) * size);

	free(buffer);
	return 0;
}
///////////////////////////////////////////////
//	                 "
//	mmmmm   mmm   mmm    m mm
//	# # #  "   #    #    #"  #
//	# # #  m"""#    #    #   #
//	# # #  "mm"#  mm#mm  #   #
///////////////////////////////////////////////

int core(int argc, char** argv)
{
	srand(time(0));
	int exit_status = EXIT_SUCCESS;

	int opt_size = 5;
	int opt_threads = 1;
	int opt_max = 100;
	int opt = 0;

	while (( opt = getopt(argc, argv, "s:t:m:")) != -1 ) {
		if (opt == 's') {
			opt_size = atoi(optarg);
			if (opt_size <= 0) {
				log ("bad size option %s", optarg);
				return EXIT_FAILURE;
			}
		}
		if (opt == 't') {
			opt_threads = atoi(optarg);
			if (opt_threads <= 0) {
				log ("bad threads option %s", optarg);
				return EXIT_FAILURE;
			}
		}
		if (opt == 'm') {
			opt_max = atoi(optarg);
			if (opt_max <= 0) {
				log ("bad max option %s", optarg);
				return EXIT_FAILURE;
			}
			opt_max = abs(opt_max);
		}
	}

	log ("options was read, size=%d, threads=%d", opt_size, opt_threads);

    if (opt_size <= opt_threads) {
		opt_threads = opt_size;
		log ("threads was erased");
    }

    // create system V token
	int token = ftok(argv[0], 0);
	log("token was received (%d)", token);

	// shared memory allocation
	int shm_id = shmget(token, opt_size * sizeof(int), 0666 | IPC_CREAT);
	if (shm_id == -1) {
		PERROR("Shared memory was not allocated");
		goto main_free_section;
	}
	else
		log("shared memory was received with id=%d", shm_id);

	// attach shared memory
	int* shared_memory = (int*) shmat(shm_id, NULL, SHM_RND);
	if (shared_memory == (void*)(-1)) {
		PERROR("Shared memory wan not attached to main");
		exit_status = EXIT_FAILURE;
		goto main_free_section;
	}
	else
		log("shared memory was attached to main process");

    int* backup = (int*) malloc (sizeof(int) * opt_size);
    if (backup == NULL) {
        PERROR("memory for backup was not allocated");
		goto main_free_section;
    }

	for (int i = 0; i < opt_size; i++) {
		shared_memory[i] = rand() % opt_max;
		backup[i] = shared_memory[i];
	}

	/////////////////////////////////////////////////////////////////

	int step = opt_size / opt_threads;
	if (opt_threads * step < opt_size)
		step++;

	struct timeval tv_start, tv_end;
    if (gettimeofday(&tv_start, NULL) == -1) {
		PERROR("time error");
    }

	pid_t* childs_id = (pid_t*) calloc (sizeof(pid_t), opt_size + 1);

	for (int i = 0; i < opt_threads; i++) {
		pid_t fork_status = fork();
		if (fork_status == -1) {
			PERROR("fork[%d] failed, child processes will be killed", i);
			kill_childs(childs_id);
			goto free_section;
		} else if (fork_status == 0) {
			exit_status = sort_process(shm_id, opt_size, step * i, MIN(step + step * i, opt_size));
            goto free_section;
		} else {
			childs_id[i] = fork_status;
		}
	}

	for (int i = 0; i < opt_threads; i++) {
        int status = 0;
        pid_t process = wait(&status);
        if (process != -1)
			if ((WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_FAILURE))) {
				log ("process with id = %d finished with error status, program will terminated", (int)process);
				kill_childs(childs_id);
			}
			else if (WIFEXITED(status)) {
				log ("process with id = %d finished with status %d", (int)process, (int)WEXITSTATUS(status));
			}
	}

	if (gettimeofday(&tv_end, NULL)) {
        PERROR("stop time error")
	}
    else {
        tv_end.tv_sec -= tv_start.tv_sec;
        tv_end.tv_usec -= tv_start.tv_usec;
        if (tv_end.tv_usec < 0) {
            tv_end.tv_sec -= 1;
            tv_end.tv_usec += 1000000;
        }
    }


	double sort_time = (double)tv_end.tv_sec + (double)tv_end.tv_usec / 1000000 ;

	double merge_time = clock();
	merge(shared_memory, opt_size, step);
	merge_time = (clock() - merge_time) / CLOCKS_PER_SEC;

	double qsort_time = clock();
	qsort(backup, opt_size, sizeof(int), cmp_int);
	qsort_time = (clock() - qsort_time) / CLOCKS_PER_SEC;

    int res = 1;
    for (int i = 0; i < opt_size; i++) {
		if (backup[i] != shared_memory[i]) {
			res = 0;
			break;
		}
    }

    if (res) {
		printf_color(CL_FT_GREEN, "OK    : ");
    }
    else {
		printf_color(CL_FT_RED, "FAILED: ");
    }
	printf("threads: %d | psort: %5.05lg | merge: %5.05lg | total: %5.05lg | qsort: %5.05lg | qsort/psort: %5.05lg\n",
								opt_threads, sort_time, merge_time, sort_time + merge_time, qsort_time, qsort_time / sort_time);

	/////////////////////////////////////////////////////////////////
main_free_section:
	// detach shared_memory
	if (shmdt(shared_memory) == -1) {
		PERROR("Shared memory was not detached from main");
	}
	else
		log("shared memory was detached from main");

	// shared memory free
	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		PERROR("Shared memory was not free, id=%d", shm_id);
	}
	else
		log("shared memory was removed, id=%d", shm_id);

free_section:
	free(childs_id);
	free(backup);

	return exit_status;
}

int main(int argc, char** argv)
{
    return core(argc, argv);
}
