#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 *1024)
char child_stack[STACK_SIZE];
const char path[] = "/home/haooops/Documents/CST/Projects/mycontainer/rootfs";

int child(void *arg)
{
        char *child_args[] = {"/bin/sh", NULL};
        chdir(path);
        chroot(".");
        execv("/bin/sh", child_args);
        return 0;
}

int main()
{
        pid_t pid = clone(child, child_stack + STACK_SIZE, SIGCHLD | CLONE_NEWPID | CLONE_NEWNET, NULL);
        if (pid < 0) {
                printf("create child process failed!\n");
        }

        waitpid(pid, NULL, 0);
        return 0;
}