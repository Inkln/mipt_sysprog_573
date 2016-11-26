#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#define CL_FT_BLACK		"\x1b[30m"
#define CL_FT_RED		"\x1b[31m"
#define CL_FT_GREEN		"\x1b[32m"
#define CL_FT_BROWN		"\x1b[33m"
#define CL_FT_BLUE		"\x1b[34m"
#define CL_FT_PURPLE	"\x1b[35m"
#define CL_FT_MAGENTA	"\x1b[36m"
#define CL_FT_GRAY		"\x1b[37m"

#define CL_BG_BLACK		"\x1b[40m"
#define CL_BG_RED		"\x1b[41m"
#define CL_BG_GREEN		"\x1b[42m"
#define CL_BG_BROWN		"\x1b[43m"
#define CL_BG_BLUE		"\x1b[44m"
#define CL_BG_PURPLE	"\x1b[45m"
#define CL_BG_MAGENTA	"\x1b[46m"
#define CL_BG_GRAY		"\x1b[47m"

#define TY_FT_DEFAULT	"\x1b[0m"
#define TY_FT_BOLD		"\x1b[1m"
#define TY_FT_HBOLD		"\x1b[2m"
#define TY_FT_UNDERLINE	"\x1b[4m"
//#define TY_FT_BOLD	"\x1b[5m"
#define TY_FT_REVERSE	"\x1b[7m"

#define SET_FONT(X) printf(X)

///////////////////////////////////////////////////////////////////////////////
/////////////////////////        SERVICE FUNCTIONS      ///////////////////////
int mystrcmp(const void* __1, const void* __2)
{
	return strcmp((const char*)__1, (const char*)__2);
}
///////////////////////////////////////////////////////////////////////////////
/////////////////////////		  PRINT FUNCTIONS       ///////////////////////

void file__print_inode (struct stat* st)			// print  only inode id
{
	printf("%6d ", (int) st->st_ino);
}
void file__print_size_kb (struct stat* st)			// print size of file in kilobytes
{
	printf("%4d ", (int)( st->st_size / 1024));
}
void file__print_size_b (struct stat* st)			// print size of file in bytes
{
	printf("%7d ", (int)st->st_size);
}
void file__print_nlink (struct stat* st)			// print nuber of link to file or 
{													//number of files in derectory
	printf("%2d ", (int) st->st_nlink);
}
void file__print_mode (struct stat* st)				// print access mode of file 
{													// for example, drwxrw-r--
	if (S_ISDIR(st->st_mode))		printf("d");
	else if (S_ISLNK(st->st_mode))  printf("l");
	else							printf("-");
			
	for (int i = 8; i >= 0; i--)
		if ((st->st_mode >> i) & 1) {
			if (i % 3 == 2)			printf("r");
			else if (i % 3 == 1)	printf("w");
			else					printf("x");
		}
		else printf("-");
	printf(" ");
}
void file__print_mtime(struct stat* st)				// print last modificaton time
{													// for examle "Oct  1 23:35"		
	struct tm* time = localtime(&st->st_mtime);
	char time_buffer[100];
	strftime(time_buffer, 100, "%b %2e %H:%M", time);
	printf("%s ", time_buffer);
}
void file__print_user(struct stat* st, int n_format)		// print information about
{															// file owner in string or
	if (n_format) printf("%4d ", (int) st->st_uid);			// number format
	else {
		struct passwd* user = getpwuid(st->st_uid);
		if (user)	printf("%10s ", user->pw_name);
		else		printf("%d ", st->st_uid);
	}
}
void file__print_group(struct stat* st, int n_format)		// print information about
{															// group of file owner in
	if (n_format) printf("%4d ", (int) st->st_gid);			// string or number format
	else {
		struct group* group = getgrgid(st->st_gid);
		if (group)	printf("%10s ", group->gr_name);
		else		printf("%d ", st->st_gid);
	}
}
void file__print_link_target(struct stat* st, const char* full_linkname)     
{															// print target of link in
	size_t length = (size_t) st->st_size + 1;				// format " -> [TARGET]"
	char* buffer = (char*) calloc (sizeof(char), length);
	buffer[length - 1] = 0;
	readlink(full_linkname, buffer, length - 1);
	printf("-> %s", buffer);
	free(buffer);
}
void file__print_filename(struct stat* st, const char* filename)
{
	if (S_ISDIR(st->st_mode))								// pring filename with
		SET_FONT(CL_FT_BLUE);								// color decoration
	else if (S_ISLNK(st->st_mode))							// directories - blue color
		SET_FONT(CL_FT_MAGENTA);							// symbolic link - magentai
	else if (st->st_mode & S_IXUSR)
		SET_FONT(CL_FT_GREEN);

	printf("%s ", filename);
	SET_FONT(TY_FT_DEFAULT) ;
}

///////////////////////////////////////////////////////////////////////////////
////////////////////		HANDLER FUNCTIONS		/////////////////////////// 

int print_file_info(const char* filename, const char* directory,
				int l, int n, int i, int a, int s, int L)
{
	char full_filename[PATH_MAX] = {};
	sprintf(full_filename, "%s/%s", directory, filename);
	struct stat st;
	if (!lstat(full_filename, &st))
	{
		// check a flag
		if (!a && strlen(filename) > 0 && filename[0] == '.')
			return 0;

		if (i) file__print_inode(&st);

		if (s)file__print_size_kb(&st);
		
		if (l || n) {
			file__print_mode(&st);
			file__print_nlink(&st);
			file__print_user(&st, n);
			file__print_group(&st, n);
			file__print_size_b(&st);	
			file__print_mtime(&st);
			file__print_filename(&st, filename);
			if (S_ISLNK(st.st_mode)) file__print_link_target(&st, full_filename);
		}
		else 
		{
			file__print_filename(&st, filename);
			if (L && S_ISLNK(st.st_mode)) 
				file__print_link_target(&st, full_filename);
		}
		
		printf("\n");
		return 0;
	}
	return -1;
}

int print_dir_files(const char* dirname, int recursion, int depth,
				int l_flag, int n_flag, int i_flag, 
				int a_flag, int s_flag, int L_flag)
{
	if (recursion && depth == -1)
		return 0;

	DIR* directory = opendir(dirname);
	if (directory == NULL)
		return -1;

	struct dirent* iterator;
	int count = 0;
	while ( (iterator = readdir(directory)) != NULL) 
		count++;
	rewinddir(directory);
	
	char* names = (char*) calloc (PATH_MAX, count);

	int index = 0;
	while ( (iterator = readdir(directory)) != NULL)
	{
		strncpy(names + PATH_MAX  * index, iterator->d_name, 256);
		index++;
	}
	closedir(directory);
	qsort(names, count, PATH_MAX, mystrcmp);

	if (recursion && strcmp(dirname, "."))
		printf("\n");
	if (recursion)
		printf("%s:\n", dirname);

	char filename[PATH_MAX];
	char full_filename[PATH_MAX];
	filename[0] = 0;
	full_filename[0] = 0;

	for (int i = 0; i < count; i++)
	{	
		strncpy(filename, names + PATH_MAX * i, PATH_MAX);
		print_file_info(filename, dirname, l_flag, n_flag, i_flag, a_flag, s_flag, L_flag);
	}

	if (recursion)
		for (int i = 0; i < count; i++)
		{
			strncpy(filename, names + PATH_MAX * i, PATH_MAX);
			struct stat st;

			full_filename[0] = 0;
			sprintf(full_filename, "%s/%s", dirname, filename);

			if (!lstat(full_filename, &st) && S_ISDIR(st.st_mode) 
				&& strcmp(filename, ".") && strcmp(filename, ".."))
			{
				print_dir_files(full_filename, recursion, depth - 1,
								l_flag, n_flag, i_flag, a_flag, s_flag, L_flag);
			}
		}
	
	free(names);
	return 0;
}
//////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
	int opt = 0;
	int R = 0, l = 0, n = 0, i = 0, a = 0, s = 0, d = -1, L = 0;

	while ( (opt = getopt(argc, argv, "Rlniasd:L")) != -1) {
		if (opt == 'R')				// list subdirectories recursively
			R = 1;
		if (opt == 'l')				// use a long listing format
			l = 1;
		if (opt == 'n')				// like -l, but list numeric user and group IDs
			n = 1;
		if (opt == 'i')				// print the index number of each file
			i = 1;
		if (opt == 'a')				// do not ignore entries starting with .
			a = 1;
		if (opt	== 's')				// print the allocated size of each file, in blocks
			s = 1;
		if (opt == 'd')				// max depth "-d [DEPTH]", ignore zero value
			d = atoi(optarg);
		if (opt == 'L')				// if not -l read links
			L = 1;
	}
	if (R && d == -1)
		d = UINT8_MAX;
	
	if (optind == argc)
		print_dir_files(".", R, d, l, n, i, a, s, L);
	else if (optind + 1 == argc)
		print_dir_files(argv[optind], R, d, l, n, i, a, s, L);
	else {
		R = 1;
		if (d == -1) 
			d = 0;
		while (optind < argc) {
			char* path = argv[optind];
			print_dir_files(path, R, d, l, n, i, a, s, L);
			optind++;
		}
	}
	return 0;
}
