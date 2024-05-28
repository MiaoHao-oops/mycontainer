#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define STACK_SIZE (1024 *1024)
char child_stack[STACK_SIZE];
#define ROOT_PATH "/home/haooops/Documents/CST/Projects/mycontainer/rootfs"

//
// child process functions
//

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

int setup_child_network(unsigned int id)
{
    char cmd[100];

    // set ip address according to child_id
    snprintf(cmd, 100, "ip addr add 172.17.0.%d/16 dev eth0", id % 256);
    // NOTE: this command should be executed after the creation of the veth pair,
    //       the `while` loop acts as a semaphore
    while (system(cmd) != 0);
    system("ip link set dev eth0 up");

    return 0;
}

int child(void *arg)
{
    char host_name[10];
    unsigned int *child_id;

    if (prepare_root() < 0) {
        printf("prepare root directory failed!\n");
        exit(-1);
    }
    if (pivot_root() < 0) {
        printf("pivot root failed!\n");
        exit(-1);
    }

    // set a new hostname in the new UTS namespace, according to child_id
    child_id = arg;
    snprintf(host_name, 10, "%08x", *child_id);
    if (sethostname(host_name, strlen(host_name)) < 0) {
        printf("set host name to %s failed!\n", host_name);
        exit(-1);
    };

    // setup the network
    setup_child_network(*child_id);

    char *child_args[] = {"/bin/bash", NULL};
    execv("/bin/bash", child_args);

    return 0;
}

//
// parent process functions
//

unsigned int create_child_id()
{
    srand(time(NULL));
    // the child_id is limited to a 8-digit hex
    return rand() % 0x100000000;
}

int write_file(const char *dir_name, const char *file_name, const char *buf)
{
    char file_path[100];
    int fd;

    strncpy(file_path, dir_name, 100);
    strncat(file_path, file_name, strlen(file_name));
    fd = open(file_path, O_WRONLY);
    if (fd == -1) {
        printf("open file %s failed!\n", file_path);
        return -1;
    }
    if (write(fd, buf, strlen(buf)) == -1) {
        printf("write child pid to %s failed!\n", file_path);
        return -1;
    }

    return 0;
}

int setup_cgroup()
{
    int fd;
    const char subtree_control[] = "+cpuset +cpu +memory";
    const char subcgroup[] = "/sys/fs/cgroup/mycontainer/";
    DIR *dir;

    // create a sub-cgroup namely `mycontainer`
    if ((dir = opendir(subcgroup)) ==  NULL) {
        if (mkdir(subcgroup, 0755) == -1) {
            printf("create sub-cgroup failed!\n");
            return -1;
        }
    } else {
        closedir(dir);
    }

    // add cpuset, cpu and memory to subtree_control of the sub-cgroup
    if (write_file(subcgroup, "cgroup.subtree_control", subtree_control) < 0) {
        return -1;
    }

    return 0;
}

int setup_child_cgroup(pid_t pid, unsigned int child_id)
{
    int fd;
    char subcgroup[100];
    char buf[100];
    char child_id_s[10];

    // create a sub-cgroup under `mycontainer` with name of child_id
    snprintf(child_id_s, 10, "%08x/", child_id);
    strncpy(subcgroup, "/sys/fs/cgroup/mycontainer/", 100);
    strncat(subcgroup, child_id_s, 10);
    mkdir(subcgroup, 0755);

    // add the child process to the sub-sub-cgroup
    snprintf(buf, 100, "%d", pid);
    if (write_file(subcgroup, "cgroup.procs", buf) < 0) {
        return -1;
    }

    // set the limit of CPU usage through `cpu.max`
    strncpy(buf, "200000 1000000", 100);    // 20% CPU time
    if (write_file(subcgroup, "cpu.max", buf) < 0) {
        return -1;
    }

    // set the hard limit of memory usage through `memory.max`
    strncpy(buf, "8388608", 100);           // 8GB memory
    if (write_file(subcgroup, "memory.max", buf) < 0) {
        return -1;
    }

    // set the soft limit of memory usage through `memory.high`
    strncpy(buf, "1048576", 100);           // 1GB memory
    if (write_file(subcgroup, "memory.high", buf) < 0) {
        return -1;
    }

    return 0;
}

int cleanup_child_cgroup(unsigned int child_id)
{
    char subcgroup[100];
    
    snprintf(subcgroup, 100, "/sys/fs/cgroup/mycontainer/%08x/", child_id);
    rmdir(subcgroup);

    return 0;
}

int clone_container(unsigned int child_id)
{
    pid_t pid;
    int flags;

    flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNET;
    pid = clone(child, child_stack + STACK_SIZE, flags, &child_id);
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

int setup_network(pid_t pid, unsigned int child_id)
{
    char cmd[100];

    // the name of veth pair in parent namespace is generated according to child_id
    snprintf(cmd, 100, "ip link add name veth%08x type veth peer name eth0 netns %d", child_id, pid);
    system(cmd);
    // bind the parent veth on bridge docker0
    snprintf(cmd, 100, "ip link set dev veth%08x master docker0", child_id);
    system(cmd);
    snprintf(cmd, 100, "ip link set dev veth%08x up", child_id);
    system(cmd);

    return 0;
}

int main()
{
    pid_t pid;
    unsigned child_id;

    child_id = create_child_id();
    printf("child id: %08x\n", child_id);

    if ((pid = clone_container(child_id)) < 0) {
        printf("create child process failed!\n");
        exit(-1);
    }

    setup_network(pid, child_id);

    if (setup_cgroup() < 0) {
        printf("setup cgroup failed due to previous errors!\n");
        exit(-1);
    }

    if (setup_child_cgroup(pid, child_id)) {
        printf("setup child cgroup failed due to previous errors!\n");
        exit(-1);
    }

    waitpid(pid, NULL, 0);
    cleanup_child_cgroup(child_id);
    cleanup_rootfs();

    return 0;
}