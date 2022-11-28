# RTOS _assignment

## Description
This assignment consists in creating a **scheduler** using Rate Monotonic policy that schedules three periodic tasks and one aperiodic task in background. These tasks have to write in the **kernel log** using a custom **kernel module** that allows to write on the kernel log using the **write**() function. Furthermore, it's possible to prevent the preemption between tasks by uncommenting some lines (read comments in the `scheduler.c` code) to use a **mutex semaphore** with priority ceiling access protocol.

## How to run
First of all, you need to have root privileges so open a terminal window and type:
```console
$ sudo su
```
Then from inside the `rtos_assignment` folder, you need to compile the kernel module using the Makefile, so simply type:
```console
$ make
```

To install the module, type:
```console
$ insmod my_module.ko
```

and to check if it was correctly installed, type:
```console
$ /sbin/lsmod
```

and in the list you should find a module called `my_module`. 

Read the major number associated to the driver (number next to `my`) by typing:
```console
$ cat /proc/devices
```

and create the special file by typing:
```console
$ mknod /dev/my c <majornumber> 0
```

Finally, compile the scheduler by typing:
```console
$ g++ -lpthread scheduler.c -o scheduler
```

and run it with:
```console
$ ./scheduler
```

To read the kernel log, use the command:
```console
$ dmesg
```