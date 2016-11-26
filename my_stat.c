#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int main(int argc, char** argv, char** env)
{
        char* filename = argv[0];
        if (argc > 1)
                filename = argv[1];
                
        struct stat st;
        if (stat(filename, &st) != 0)
        {
                printf("Can't get file info\n");
                return -1;
        }
        printf("uid: %d\n",  (int)st.st_uid);
        printf("gid: %d\n",  (int)st.st_gid);
        printf("size: %d\n", (int)st.st_size);
        
        printf("Access: %04o/", (~S_IFMT) & st.st_mode);
        for (int i = 8; i >= 0; i--)
                if ((st.st_mode >> i) & 1)
                {
                        if (i % 3 == 2)
                                printf("r");
                        else if (i % 3 == 1)
                                printf("w");
                        else
                                printf("x");
                }
                else
                {
                        printf("-");
                }
        printf("\n");

        printf("Last access: %s", asctime(localtime(&st.st_atime)));
        printf("Last modification: %s", asctime(localtime(&st.st_mtime)));
        printf("Last status change: %s", asctime(localtime(&st.st_ctime)));
        return 0;
}
