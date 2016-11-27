#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

// FONT MACRO
#define CL_FT_BLACK             "\x1b[30m"
#define CL_FT_RED               "\x1b[31m"
#define CL_FT_GREEN             "\x1b[32m"
#define CL_FT_BROWN             "\x1b[33m"
#define CL_FT_BLUE              "\x1b[34m"
#define CL_FT_PURPLE    		"\x1b[35m"
#define CL_FT_MAGENTA   		"\x1b[36m"
#define CL_FT_GRAY              "\x1b[37m"

#define CL_BG_BLACK             "\x1b[40m"
#define CL_BG_RED               "\x1b[41m"
#define CL_BG_GREEN             "\x1b[42m"
#define CL_BG_BROWN             "\x1b[43m"
#define CL_BG_BLUE              "\x1b[44m"
#define CL_BG_PURPLE    		"\x1b[45m"
#define CL_BG_MAGENTA   		"\x1b[46m"
#define CL_BG_GRAY              "\x1b[47m"

#define TY_FT_DEFAULT   		"\x1b[0m"
#define TY_FT_BOLD              "\x1b[1m"
#define TY_FT_HBOLD             "\x1b[2m"
#define TY_FT_UNDERLINE 		"\x1b[4m"
//#define TY_FT_BOLD    		"\x1b[5m"
#define TY_FT_REVERSE   		"\x1b[7m"

#define SET_FONT(X) printf(X)

// LOG MACRO
#if defined(DEBUG)
	#define log(format, ...) \
	{ \
		char __tb[101]; \
		time_t t = time(NULL); \
		struct tm* tmp = localtime(&t); \
		strftime(__tb, 100, "%d/%m/%Y %H:%M:%S ", tmp); \
		printf("%s [pid=%d] ", __tb, (int)getpid()); \
		printf(format, ##__VA_ARGS__); \
		printf("\n"); \
		fflush(stdout); \
	}
#else
	#define log(format, ...) ;
#endif

// PERROR MACRO
#define PERROR(format, ...) \
	{ \
		char __tb[101]; \
		time_t t = time(NULL); \
		struct tm* tmp = localtime(&t); \
		strftime(__tb, 100, "%d/%m/%Y %H:%M:%S ", tmp); \
		printf("%s [pid=%d]", __tb, (int)getpid()); \
		SET_FONT(CL_FT_RED); \
		printf(" ERROR! "); \
		SET_FONT(TY_FT_DEFAULT); \
		printf(format, ##__VA_ARGS__); \
		printf(" (%s)\n", strerror(errno)); \
		fflush(stdout); \
	}

// SEMAFOR MACRO
#define SET(command, opt_num, opt_op, opt_flg) \
                command.sem_num = opt_num; \
                command.sem_op = opt_op; \
                command.sem_flg = opt_flg;

// SEM = VALUE
#define SETSEM(semid, SEM_ID, VALUE) \
        if (semctl(semid, SEM_ID, SETVAL, VALUE) == -1) \
			perror("SETVAL " );

// SEM++
#define V(semid, SEM_ID) { \
			struct sembuf command = {SEM_ID,  1, 0}; \
			if (semop(semid, &command, 1) == -1) { \
				perror("SEM V " ); \
				return EXIT_FAILURE; \
			} \
		}

//SEM += K
#define VK(semid, SEM_ID, K) { \
			struct sembuf command = {SEM_ID,  K, 0}; \
			if (semop(semid, &command, 1) == -1) { \
				perror("SEM VK " ); \
				return EXIT_FAILURE; \
			} \
		}

// SEM-- (blocked operation!)
#define P(SEM_ID) { \
			struct sembuf command = {SEM_ID, -1, 0}; \
			if (semop(semid, &command, 1) == -1) { \
				perror("SEM P " ); \
				return EXIT_FAILURE; \
			} \
		}

// SER -= K (blocked operation)
#define PK(SEM_ID, K) { \
			struct sembuf command = {SEM_ID, -K, 0}; \
			if (semop(semid, &command, 1) == -1) { \
				perror("SEM PK " ); \
				return EXIT_FAILURE; \
			} \
		}

// waiting ZERO in SEM
#define WAIT(SEM_ID) { \
			struct sembuf command = {SEM_ID,  0, 0}; \
			if (semop(semid, &command, 1) == -1) { \
				perror("SEM WAIT " ); \
				return EXIT_FAILURE; \
			} \
		}

#endif // DEBUG_H_INCLUDED
