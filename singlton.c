#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

void long_printf(const char* str)
{
	for (char* iter = (char*) str; *iter; iter++)
	{
		printf("%c", *iter);
		fflush(stdout);
		usleep(500000);
	}
	printf("\n");
}

int lock()
{
    int fd = -1;
    while( (fd = open("lock", O_CREAT | O_EXCL, 0666)) == -1)
        usleep(100000);

    return fd;
}

void unlock(int fd)
{
    close(fd);
    unlink("lock");
}
unsigned get_lock_count_and_add()
{
    int lock_id = lock();

    unsigned count = 0;

    int fd = open("lock_count", O_RDONLY);
    if (fd != -1)
        read(fd, &count, sizeof(unsigned));
    close(fd);

    count ++;
    fd = open("lock_count", O_WRONLY | O_CREAT, 0666);
    if (fd != -1)
        write(fd, &count, sizeof(unsigned));
    close(fd);

    unlock(lock_id);

    return count - 1;
}

void dec_lock_count()
{
    int lock_id = lock();

    unsigned count = 0;

    int fd = open("lock_count", O_RDONLY);
    if (fd != -1)
        read(fd, &count, sizeof(unsigned));
    close(fd);

    count --;
    fd = open("lock_count", O_WRONLY | O_CREAT, 0666);
    if (fd != -1)
        write(fd, &count, sizeof(unsigned));
    close(fd);

    unlock(lock_id);
}

int main(void)
{
    int lock_count = get_lock_count_and_add();

    if (lock_count == 0)
        long_printf("The first .....");
    else
        long_printf("The second ....");

    dec_lock_count();
	return 0;
}
