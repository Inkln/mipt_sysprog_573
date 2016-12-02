#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>

#include <time.h>

#define N 4

int child(mqd_t gq, mqd_t bq)
{
	srand(time(0));
    int status = rand() % 2;
    if (status == 0) {
        printf("Hello world\n");
        mq_send(gq, NULL, NULL, NULL);
        fflush(stdout);
    } else {
    	printf("Bye world\n");
    	mq_send(bq, NULL, NULL, NULL);
    	fflush(stdout);
    }
}

int main(void)
{
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = N;
	attr.mq_msgsize = 0;
	attr.mq_curmsgs = 0;

    mqd_t good = mq_open("good", O_NONBLOCK | O_RDWR | O_CREAT, 0666, NULL);
    if (good == -1) {
		perror("Good create");
    }
	mqd_t bad = mq_open("bad", O_NONBLOCK | O_RDWR | O_CREAT, 0666, NULL);
	if (bad == -1) {
		perror("Bad create");
	}

	pid_t pid = 0;
    for (int i = 0; i < N; i++) {
		pid = fork();
		if (pid == 0) {
			child(good, bad);
			break;
		}
    }
    if (pid == 0)
		return 0;

    for (int i = 0; i < N; i++)
		wait(NULL);

    int ans = 0;
    int hello = 0;
    int bye = 0;
    long long int prio = 0;

    while (ans < N)
	{
        if (mq_receive(good, NULL, NULL, &prio) != -1) {
			ans += 1;
			hello += 1;
        } else perror("Good");
        if (mq_receive(bad, NULL, NULL, &prio) != -1) {
			ans += 1;
			bye += 1;
        } else perror("Bad");
        sleep(1);
	}
	printf("Hello=%d, bye=%d\n", hello, bye);
	mq_close(good);
	mq_close(bad);
	return 0;
}
