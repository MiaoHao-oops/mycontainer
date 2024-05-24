## quick start

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

### Step 2: compile the project and run

1. compile

```sh
make
```

2. run as root

```sh
sudo ./mycontainer
```

3. check if the container is in a new PID namespace

```sh
ps -e
```

the shell will print:

```sh
/ # ps -e
PID   USER     COMMAND
    1 0        /bin/sh
    2 0        {ps} /bin/sh
/ # 
```
