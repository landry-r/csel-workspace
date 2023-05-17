/* Multiprocessing et Ordonnanceur
** ===============================
** Exercice 03:
** CGroups
** ------------------------------------
** Créer une application composée
** processus qui utilisent 100%
** d'un CPU.

mount -t tmpfs none /sys/fs/cgroup
mkdir /sys/fs/cgroup/memory
mount -t cgroup -o memory memory /sys/fs/cgroup/memory
mkdir /sys/fs/cgroup/memory/mem
echo $$ > /sys/fs/cgroup/memory/mem/tasks
echo 20M > /sys/fs/cgroup/memory/mem/memory.limit_in_bytes
mkdir /sys/fs/cgroup/cpuset
mount -t cgroup -o cpu,cpuset cpuset /sys/fs/cgroup/cpuset
mkdir /sys/fs/cgroup/cpuset/high
mkdir /sys/fs/cgroup/cpuset/low
*/

#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
// Socketpair
#include <sys/socket.h>
// CPU
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    printf("Hello exercice 03 !\n");

    pid_t pid;
    // CPU à utiliser
    int cpu_high = 1;
    int cpu_low = 2;
    int cpu_high_usage = 750; // Diviser par 10 pour avoir le pourcentage 
    int cpu_low_usage = 250; // Diviser par 10 pour avoir le pourcentage

    // Création du processus 1
    pid = fork();
    printf("Create process %d\n", getpid());
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } 
    
    if (pid == 0) {// Processus enfant
        // Limiter l'utilisation du CPU
        int cpuFile = open("/sys/fs/cgroup/cpuset/low/cpuset.cpus", O_WRONLY);
        int memFile = open("/sys/fs/cgroup/cpuset/low/cpuset.mems", O_WRONLY);
        int tasksFile = open("/sys/fs/cgroup/cpuset/low/tasks", O_WRONLY);
        int shareFile = open("/sys/fs/cgroup/cpuset/low/cpu.shares", O_WRONLY);
        if (cpuFile == -1 || memFile == -1 || tasksFile == -1 || shareFile == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        char str[10];
        sprintf(str, "%d", cpu_low);
        int w_cpu = write(cpuFile, str, strlen(str));
        int w_mem = write(memFile, "0", 1);
        // Write PID as string to tasks file
        sprintf(str, "%d", getpid());
        int w_task = write(tasksFile, str, strlen(str));
        // Write CPU shares
        sprintf(str, "%d", cpu_low_usage);
        int w_share = write(shareFile, str, strlen(str));
        if (w_cpu == -1 || w_mem == -1 || w_task == -1 || w_share == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        w_cpu = close(cpuFile);
        w_mem = close(memFile);
        w_task = close(tasksFile);
        w_share = close(shareFile);
        if (w_cpu == -1 || w_mem == -1 || w_task == -1 || w_share == -1) {
            perror("close");
            exit(EXIT_FAILURE);
        }

        // Utilisation 100% du CPU
        while (1) {
            // Occupation du CPU
        }

        //exit(EXIT_SUCCESS);
    }
    
    else if (pid > 0) {// Processus parent
        // Limiter l'utilisation du CPU
        int cpuFile = open("/sys/fs/cgroup/cpuset/high/cpuset.cpus", O_WRONLY);
        int memFile = open("/sys/fs/cgroup/cpuset/high/cpuset.mems", O_WRONLY);
        int tasksFile = open("/sys/fs/cgroup/cpuset/high/tasks", O_WRONLY);
        int shareFile = open("/sys/fs/cgroup/cpuset/high/cpu.shares", O_WRONLY);
        if (cpuFile == -1 || memFile == -1 || tasksFile == -1 || shareFile == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        char str[10];
        sprintf(str, "%d", cpu_high);
        int w_cpu = write(cpuFile, str, strlen(str));
        int w_mem = write(memFile, "0", 1);
        // Write PID as string to tasks file
        sprintf(str, "%d", getpid());
        int w_task = write(tasksFile, str, strlen(str));
        // Write CPU shares
        sprintf(str, "%d", cpu_high_usage);
        int w_share = write(shareFile, str, strlen(str));
        if (w_cpu == -1 || w_mem == -1 || w_task == -1 || w_share == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        w_cpu = close(cpuFile);
        w_mem = close(memFile);
        w_task = close(tasksFile);
        w_share = close(shareFile);
        if (w_cpu == -1 || w_mem == -1 || w_task == -1 || w_share == -1) {
            perror("close");
            exit(EXIT_FAILURE);
        }

        // Utilisation 100% du CPU
        while (1) {
            // Occupation du CPU
        }

        //exit(EXIT_SUCCESS);
    }
/*
    // Création du processus 2
    pid = fork();
    printf("Create process %d\n", getpid());
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 2) {// Processus enfant 2
        printf("Create child process %d\n", getpid());
        // Limiter l'utilisation du CPU
        int cpuFile = open("/sys/fs/cgroup/cpuset/low/cpuset.cpus", O_WRONLY);
        int memFile = open("/sys/fs/cgroup/cpuset/low/cpuset.mems", O_WRONLY);
        if (cpuFile == -1 || memFile == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        write(cpuFile, &cpu_low, sizeof(cpu_low));
        close(cpuFile);
        write(memFile, "0", 1);
        close(memFile);

        exit(EXIT_SUCCESS);
    }
*/
    // Attente de la fin des processus enfants
    int status;
    int err = wait(&status);
    if (err == -1) {
        perror("wait");
        exit(EXIT_FAILURE);
    }

    printf("End of program\n");

    return 0;
}