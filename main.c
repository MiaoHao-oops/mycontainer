#define _GNU_SOURCE
#include <fcntl.h>
#include <string.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <limits.h>
#include <sys/mman.h>

#define STACK_SIZE (1024 *1024)
char child_stack[STACK_SIZE];
#define ROOT_PATH "/home/haooops/Documents/CST/Projects/mycontainer/rootfs"
#define HOST_NAME "mycontainer"

int prepare_root()
{
    // remount "/" and "rootfs", create directory to put old root
    mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL);
    mount(ROOT_PATH, ROOT_PATH, NULL, MS_BIND | MS_REC, NULL);
    mkdir(ROOT_PATH "/old_root", 0777);

    return 0;
}

int pivot_root()
{
    // change directory to "rootfs" and execute syscall "pivot_root"
    chdir(ROOT_PATH);
    syscall(SYS_pivot_root, ROOT_PATH, ROOT_PATH "/old_root");

    // umount old root and remove its directory
    umount2("./old_root", MNT_DETACH);
    remove("./old_root");

    // change directory to the new root
    chdir("/");
    
    // mount important file systems 'proc' and 'sys', equal cmd:
    //   - mkdir /proc && mount -t proc proc /proc
    //   - mkdir /sys && mount -t sysfs sysfs /sys
    mkdir("/proc", 0777);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0777);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    return 0;
}

int child(void *arg)
{
    if (prepare_root() < 0) {
        printf("prepare root directory failed!\n");
        return -1;
    }
    if (pivot_root() < 0) {
        printf("pivot root failed!\n");
        return -1;
    }

    // set a new hostname in the new UTS namespace
    sethostname(HOST_NAME, strlen(HOST_NAME));

    char *child_args[] = {"/bin/bash", NULL};
    execv("/bin/bash", child_args);

    return 0;
}

int clone_container()
{
    pid_t pid;
    int flags;

    flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNET;
    pid = clone(child, child_stack + STACK_SIZE, flags, NULL);
    printf("child pid: %d\n", pid);

    return pid;
}

int cleanup_rootfs()
{
    rmdir(ROOT_PATH "old_root");
    umount(ROOT_PATH);
    umount("/");

    return 0;
}

int setup_cgroup(pid_t pid)
{
    int fd;
    char buf[100];

    // create a sub-cgroup namely `mycontainer`
    mkdir("/sys/fs/cgroup/mycontainer", 0755);

    // add the child process to the sub-cgroup `mycontainer`
    fd = open("/sys/fs/cgroup/mycontainer/cgroup.procs", O_WRONLY);
    if (fd == -1) {
        printf("open file /sys/fs/cgroup/mycontainer/cgroup.procs failed!\n");
        return -1;
    }
    snprintf(buf, 100, "%d", pid);
    if (write(fd, buf, strlen(buf)) == -1) {
        printf("write child pid to /sys/fs/cgroup/mycontainer/cgroup.procs failed!\n");
        return -1;
    }

    // set the limit of CPU usage through `cpu.max`
    fd = open("/sys/fs/cgroup/mycontainer/cpu.max", O_WRONLY);
    if (fd == -1) {
        printf("open file /sys/fs/cgroup/mycontainer/cgroup.max failed!\n");
        return -1;
    }
    strncpy(buf, "200000 1000000", 100);
    if (write(fd, buf, strlen(buf)) == -1) {
        printf("write child pid to /sys/fs/cgroup/mycontainer/cgroup.maxs failed!\n");
        return -1;
    }

    return 0;
}

int main()
{
    pid_t pid;

    if ((pid = clone_container()) < 0) {
        printf("create child process failed!\n");
        exit(-1);
    }

    if (setup_cgroup(pid) < 0) {
        printf("setup cgroup failed due to previous errors!\n");
        exit(-1);
    }
    waitpid(pid, NULL, 0);
    cleanup_rootfs();

    return 0;
}