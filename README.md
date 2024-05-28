# mycontainer

A tiny toy container runtime written in 200 lines of C.

## quick start

### Prerequisites

`mycontainer` runs smoothly with the following prerequisites:

- `make`
- `gcc`
- `ip`
- `docker`
- Linux distribution with [control group v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)

The following build instructions were tested under Ubuntu 22.04.

### Step 1: make rootfs from docker image

First, we should make a rootfs for our container. There are many ways to build rootfs from scratch, namely, busybox, shell command lines, and etc. In this document, I will demostrate an unusual way to build a rootfs, which take advantages of **docker images**.

1. export an archive from a docker image(centos for example)

    ```sh
    docker export $(docker create centos) --output="centos.tar"
    ```

2. extract the archive to a directory

    ```sh
    mkdir rootfs
    tar -xf centos.tar -C rootfs
    ```

3. check the rootfs

    ```sh
    $ ls rootfs
    bin  etc   lib    lost+found  mnt  proc  run   srv  tmp  var
    dev  home  lib64  media       opt  root  sbin  sys  usr
    ```

### Step 2: compile and run

1. compile and run

    ```sh
    make run
    ```

    The shell will ask for your root password. After entering the password, the container will start up.

2. check if the container is in a new PID namespace

    ```sh
    ps -e
    ```

    the shell will show:

    ```sh
    [root@75a80a52 /]# ps -e
        PID TTY          TIME CMD
          1 ?        00:00:00 bash
         12 ?        00:00:00 ps
    ```

3. check the network

    ```sh
    ip a
    ```

    the shell will show:
    ```sh
    [root@75a80a52 /]# ip a
    1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN group default qlen 1000
        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    2: eth0@if30: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
        link/ether f2:bc:91:44:c8:40 brd ff:ff:ff:ff:ff:ff link-netnsid 0
        inet 172.17.0.82/16 scope global eth0
        valid_lft forever preferred_lft forever
        inet6 fe80::f0bc:91ff:fe44:c840/64 scope link 
        valid_lft forever preferred_lft forever
    ```

    The container is at ip address `172.17.0.82/16`.