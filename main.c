#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 *1024)
char child_stack[STACK_SIZE];
#define ROOT_PATH "./rootfs"

int child(void *arg)
{
        char *child_args[] = {"/bin/bash", NULL};
        chdir(ROOT_PATH);
        chroot(".");
        mkdir("/proc", 0777);
        mount("proc", "/proc", "proc", 0, NULL);
        execv("/bin/bash", child_args);
        return 0;
}

int main()
{
        pid_t pid;
        int flags;
        
        flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNET;
        pid = clone(child, child_stack + STACK_SIZE, flags, NULL);
        printf("child pid: %d\n", pid);
        if (pid < 0) {
                printf("create child process failed!\n");
        }

        waitpid(pid, NULL, 0);
        umount(ROOT_PATH "/proc");
        rmdir(ROOT_PATH "/proc");
        return 0;
}