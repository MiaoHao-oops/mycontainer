## quick start

1. compile

```sh
make
```

2. run as root

```sh
sudo ./mycontainer
```

3. mount `/proc` in the container

```sh
mount -t proc proc /proc
```

4. check if the container is in a new PID namespace

```sh
ps -e
```

the shell will output:

```sh
/ # ps -e
PID   USER     COMMAND
    1 0        /bin/sh
    3 0        {ps} /bin/sh
/ # 
```
