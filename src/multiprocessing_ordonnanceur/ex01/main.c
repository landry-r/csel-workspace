/* Multiprocessing et Ordonnanceur
** ===============================
** Exercice 01:
** Processus, signaux et communication
** ------------------------------------
** Communication entre processus parent et enfant
** Capturer tous les signaux et les ignorer,
** Le message exit permet de terminer le processus.
** Chaque processus utilise son propre coeur.
*/

#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
// Socketpair
#include <sys/socket.h>
// CPU
#include <sched.h>

#define BUFFER_SIZE 100

void catch_signal(int sig)
{
    // Print the signal number
    printf("Signal %d catched\n", sig);
}

void ignore_signal(int sigals[], int size)
{
    int i, ret;

    //struct sigaction sa = {.sa_handler = SIG_IGN, }; // Ignore signal
    struct sigaction sa = {.sa_handler = catch_signal, }; // Catch signal
    sigemptyset(&sa.sa_mask);
    // For each signals
    for (i = 0; i < size; i++) {
        ret = sigaction(sigals[i], &sa, NULL);
        if (ret == -1) {
            perror("sigaction");
        }
    }
}

void child(int sockets[2])
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);  // Set child process to use CPU 1
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }

    close(sockets[0]);  // Close unused socket

    // Send message to parent
    char* msg = "Hello from child process!";
    if (send(sockets[1], msg, BUFFER_SIZE, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    //wait 1 seconds
    int slp = sleep(1);
    if (slp > 0) {
        printf("Sleep interrupted by signal\n");
        sleep(slp);
    }

    // Send message to parent
    msg = "Hello again!";
    if (send(sockets[1], msg, BUFFER_SIZE, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    //wait 1 seconds
    slp = sleep(1);
    if (slp > 0) {
        printf("Sleep interrupted by signal\n");
        sleep(slp);
    }

    // Send exit message to parent
    msg = "exit";
    if (send(sockets[1], msg, BUFFER_SIZE, 0) < 0) {
        perror("send");
        exit(EXIT_SUCCESS);
    }
    close(sockets[1]);  // Close socket
}

void parent(int sockets[2])
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // Set parent process to use CPU 0
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }

    close(sockets[1]);  // Close unused socket
    char buffer[BUFFER_SIZE];
    int n;

    while (1) {
        n = recv(sockets[0], buffer, BUFFER_SIZE, 0);
        if (n < 0) {
            perror("p-recv");
            if (errno != EINTR) {
                exit(EXIT_FAILURE);
            }
            continue;
        }
        
        printf("Message from child: %s\n", buffer);

        if (strcmp(buffer, "exit") == 0) {
            exit(EXIT_SUCCESS); // end process
            //kill(0, SIGKILL); // end application
        }

        // Send answer to child
        /*char* msg = "Hello from parent process!";
        if (send(sockets[0], msg, BUFFER_SIZE, 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }*/
    }
    close(sockets[0]);  // Close socket
}

int main(void)
{
    //List of signals
    int signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGABRT, SIGTERM};
    int size = sizeof(signals) / sizeof(signals[0]);
    int i, ret;

    printf("Hello from main process!\n");
    ignore_signal(signals, size);

    //Generate all signals
    for (i = 0; i < size; i++) {
        ret = kill(0, signals[i]);
        if (ret == -1) {
            perror("kill");
        }
    }

    pid_t pid;
    int sockets[2];

    int err = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    if (err < 0) {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {  // Child process
        child(sockets);
    } else if (pid > 0) {  // Parent process
        parent(sockets);
    }

    printf("End of process!\n");

    return 0;
}
