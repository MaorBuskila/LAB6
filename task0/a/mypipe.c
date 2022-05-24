#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define MSGSIZE 16
char *msg1 = "hello, world #1";

int main() {
    char inbuf[MSGSIZE];
    int p[2], pid, nbytes;

    if (pipe(p) < 0)
        _exit(1);

    /* continued */
    /* write pipe */
    if (!(pid = fork())) {
        close(p[0]);
        write(p[1], msg1, MSGSIZE);
    } else {
        // Adding this line will
        close(p[1]);
        read(p[0],inbuf, MSGSIZE);
        printf("%s\n" , inbuf);
    }
    return 0;
}