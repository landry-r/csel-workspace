#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <fcntl.h>
#include <grp.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "ssd1306.h"

#define S1 "0"
#define S2 "2"
#define S3 "3"

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define BUTTON_GPIO_PREFIX "/sys/class/gpio/gpio"

#define UNUSED(x) (void)(x)

static char frequency[3];
static char mode[3];
static char temperature[3];
int s1_file;  // Button S1
int s2_file;  // Button S2
int s3_file;  // Button S3
int fs_file;    // Frequency from supervisor
int mode_file;  // Mode from supervisor
int temp_file;  // Temperature from supervisor

static int signal_catched = 0;

static int export_gpio(const char* gpio)
{
    // unexport pin out of sysfs (reinitialization)
    int f = open(GPIO_UNEXPORT, O_WRONLY);
    write(f, gpio, strlen(gpio));
    close(f);

    f = open(GPIO_EXPORT, O_WRONLY);
    if (f == -1) {
        perror("Failed to open GPIO export");
        return -1;
    }

    if (write(f, gpio, strlen(gpio)) == -1) {
        perror("Failed to export GPIO");
        close(f);
        return -1;
    }

    close(f);
    return 0;
}

static int configure_gpio(const char* gpio,
                          const char* direction,
                          const char* edge)
{
    char gpio_direction_path[256];
    snprintf(gpio_direction_path,
             sizeof(gpio_direction_path),
             "%s%s/direction",
             BUTTON_GPIO_PREFIX,
             gpio);

    int f = open(gpio_direction_path, O_WRONLY);
    if (f == -1) {
        perror("Failed to open GPIO direction");
        return -1;
    }

    if (write(f, direction, strlen(direction)) == -1) {
        perror("Failed to configure GPIO direction");
        close(f);
        return -1;
    }

    close(f);

    char gpio_edge_path[256];
    snprintf(gpio_edge_path,
             sizeof(gpio_edge_path),
             "%s%s/edge",
             BUTTON_GPIO_PREFIX,
             gpio);
    f = open(gpio_edge_path, O_WRONLY);
    if (f == -1) {
        perror("Failed to open GPIO edge");
        return -1;
    }

    if (write(f, edge, strlen(edge)) == -1) {
        perror("Failed to configure GPIO edge");
        close(f);
        return -1;
    }

    close(f);
    return 0;
}

static void fork_process()
{
    pid_t pid = fork();
    switch (pid) {
        case 0:
            break;  // child process has been created
        case -1:
            syslog(LOG_ERR, "ERROR while forking");
            exit(1);
            break;
        default:
            exit(0);  // exit parent process with success
    }
}

static void catch_signal(int signal)
{
    syslog(LOG_INFO, "signal=%d catched\n", signal);
    signal_catched++;
}

static void fork_process()
{
    pid_t pid = fork();
    switch (pid) {
        case 0:
            break;  // child process has been created
        case -1:
            syslog(LOG_ERR, "ERROR while forking");
            exit(1);
            break;
        default:
            exit(0);  // exit parent process with success
    }
}

int daemon_process(int sockets[2])
{
    close(sockets[1]);  // Close unused socket
    // Export button GPIOs
    if (export_gpio(S1) == -1) return -1;
    if (export_gpio(S2) == -1) return -1;
    if (export_gpio(S3) == -1) return -1;

    // Configure button GPIOs
    if (configure_gpio(S1, "in", "rising") == -1) return -1;
    if (configure_gpio(S2, "in", "rising") == -1) return -1;
    if (configure_gpio(S3, "in", "rising") == -1) return -1;

    if (ssd1306_init() < 0) {
        perror("ssd1306_init");
        return -1;
    }

    char buttonStateS1;
    char buttonStateS2;
    char buttonStateS3;

    // Read buttons
    s1_file = open("/sys/class/gpio/gpio0/value", O_RDONLY);  // Button S1
    s2_file = open("/sys/class/gpio/gpio2/value", O_RDONLY);  // Button S2
    s3_file = open("/sys/class/gpio/gpio3/value", O_RDONLY);  // Button S3

    fs_file   = open("/sys/class/mini_project/supervisor/frequency",
                   O_RDWR);  // Frequency from supervisor
    mode_file = open("/sys/class/mini_project/supervisor/mode",
                     O_RDWR);  // Mode from supervisor
    temp_file = open("/sys/class/mini_project/supervisor/temperature",
                     O_RDONLY);  // Temperature from supervisor

    while (1) {
        if (s1_file != -1 && s2_file != -1 && s3_file != -1 && fs_file != -1 && mode_file != -1 && temp_file != -1) {
            read(s1_file, &buttonStateS1, 1);
            read(s2_file, &buttonStateS2, 1);
            read(s3_file, &buttonStateS3, 1);

            read(fs_file, frequency, 2);
            read(mode_file, mode, 2);
            read(temp_file, temperature, 2);

            int old_frequency = atoi(frequency);
            int old_mode      = atoi(mode);

            if (buttonStateS3 == '1') {  // Button S3 pressed
                old_mode = (old_mode == 1) ? 0 : 1;
                // Update the mode with sysfs
                sprintf(mode, "%d", old_mode);
                write(mode_file, mode, 1);
            }
            if (old_mode == 0) {  // If manual mode is activated
                if (buttonStateS1 == '1' &&
                    old_frequency < 20) {  // Button S1 pressed
                    old_frequency++;

                    // Update frequency with sysfs
                    sprintf(frequency, "%d", old_frequency);
                    printf("%s\n", frequency);
                    write(fs_file, frequency, 2);
                } else if (buttonStateS2 == '1' &&
                           old_frequency > 1) {  // Button S2 pressed
                    old_frequency--;

                    // Update frequency with sysfs
                    sprintf(frequency, "%d", old_frequency);
                    write(fs_file, frequency, 2);
                }
            }
            /*char buffer[256];
            int n = read(sockets[0], buffer, sizeof(buffer));
            if (n > 0) {
                char type[24];
                char value[3];
                // separate type and value with :
                sscanf(buffer, "%s[^:]:%s", type, value);
                if (strcmp(type, "frequency") == 0) {
                    // Update frequency with sysfs
                    sprintf(frequency, "%s", value);
                    write(fs_file, frequency, 2);
                } else if (strcmp(type, "mode") == 0) {
                    // Update mode with sysfs
                    sprintf(mode, "%s", value);
                    write(mode_file, mode, 1);
                }
            }*/
        }

        // i2c screen
        ssd1306_set_position(0, 0);
        ssd1306_puts("CSEL1a - SP.07");
        ssd1306_set_position(0, 1);
        ssd1306_puts("  Miniproject");
        ssd1306_set_position(0, 2);
        ssd1306_puts("--------------");

        ssd1306_set_position(0, 3);
        ssd1306_puts("Temp: ");
        ssd1306_puts(temperature);
        ssd1306_puts("'C");
        ssd1306_set_position(0, 4);
        ssd1306_puts("Freq: ");
        ssd1306_puts(frequency);
        ssd1306_puts("Hz ");
        ssd1306_set_position(0, 5);
        ssd1306_puts("Mode: ");
        if (atoi(mode) == 0) {
            ssd1306_puts("Manual");
        } else {
            ssd1306_puts("Auto  ");
        }

        usleep(100000);
    }

    close(sockets[0]);
    close(s1_file);
    close(s2_file);
    close(s3_file);

    close(fs_file);
    close(mode_file);
    close(temp_file);
    return 0;
}

int socket_process(int sockets[2])
{
    close(sockets[0]);  // Close unused socket
    // Créer socket du serveur
    int socket_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_server < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Configurer l'adresse du serveur
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(8080);

    // Lier le socket à l'adresse du serveur
    if (bind(socket_server,
             (struct sockaddr*)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("Erreur lors de la liaison du socket à l'adresse du serveur");
        exit(EXIT_FAILURE);
    }

    // Mettre le serveur en écoute pour les connexions entrantes
    if (listen(socket_server, 5) < 0) {
        perror("Erreur lors de la mise en écoute du socket");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accepter une nouvelle connexion entrante
        int socket_client = accept(socket_server, NULL, NULL);
        if (socket_client < 0) {
            perror("Erreur lors de l'acceptation de la connexion entrante");
            exit(EXIT_FAILURE);
        }

        // Gérer la communication avec le client
        char buffer[256];
        int n;

        while (1) {
            // Lire les données envoyées par le client
            n = read(socket_client, buffer, sizeof(buffer));
            if (n < 0) {
                perror("Erreur lors de la lecture des données du client");
                exit(EXIT_FAILURE);
            }

            if (n == 0) {
                // Fin de la connexion avec le client
                close(socket_client);
                break;
            }

            if (n > 0) {
                printf("Message reçu du client : %s\n", buffer);
                syslog(LOG_NOTICE, "Message reçu du client : %s", buffer);

                /*char type[24];
                int value = 0;
                // separate type and value with :
                sscanf(buffer, "%[^:]:%s", type, value);
                if (strcmp(type, "frequency") == 0) {
                    // Update frequency with sysfs
                    sprintf(buffer, "%s", value);
                    write(fs_file, buffer, 2);
                } else if (strcmp(type, "mode") == 0) {
                    // Update mode with sysfs
                    sprintf(buffer, "%s", value);
                    write(mode_file, buffer, 1);
                }*/
                if (send(sockets[1], buffer, sizeof(buffer), 0) < 0) {
                    perror(
                        "Erreur lors de l'envoi des données.");
                    exit(EXIT_FAILURE);
                }

                // Répondre au client
                const char* message = "Message reçu par le serveur";
                n = write(socket_client, message, strlen(message));
                if (n < 0) {
                    perror("Erreur lors de l'envoi de la réponse au client");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    close(sockets[1]);
    close(socket_server);

    return 0;
}

int main(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);
	
	// 1. fork off the parent process
    fork_process();

<<<<<<< HEAD
    // 1. fork off the parent process
    /*
        fork_process();

        // 2. create new session
        if (setsid() == -1) {
                syslog (LOG_ERR, "ERROR while creating new session");
                exit (1);
        }

        // 3. fork again to get rid of session leading process
        fork_process();
    */
    printf("Daemon started with PID=%d\n", getpid());
=======
    // 2. create new session
    if (setsid() == -1) {
        syslog(LOG_ERR, "ERROR while creating new session");
        exit(1);
    }

    // 3. fork again to get rid of session leading process
    fork_process();
	
>>>>>>> refs/remotes/origin/main
    // 4. capture all required signals
    struct sigaction act = {
        .sa_handler = catch_signal,
    };
    /*sigaction(SIGHUP, &act, NULL);   //  1 - hangup
    sigaction(SIGINT, &act, NULL);   //  2 - terminal interrupt
    sigaction(SIGQUIT, &act, NULL);  //  3 - terminal quit
    sigaction(SIGABRT, &act, NULL);  //  6 - abort
    sigaction(SIGTERM, &act, NULL);  // 15 - termination
    sigaction(SIGTSTP, &act, NULL);  // 19 - terminal stop signal
    */
    // 5. update file mode creation mask
    umask(0027);

    // 6. change working directory to appropriate place
    if (chdir("/opt") == -1) {
        syslog(LOG_ERR, "ERROR while changing to working directory");
        exit(1);
    }

    // 7. close all open file descriptors
    for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--) {
        close(i);
    }
    printf("Daemon started with PID=%d\n", getpid());
    // 8. redirect stdin, stdout and stderr to /dev/null
    if (open("/dev/null", O_RDWR) != STDIN_FILENO) {
        syslog(LOG_ERR, "ERROR while opening '/dev/null' for stdin");
        exit(1);
    }
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
        syslog(LOG_ERR, "ERROR while opening '/dev/null' for stdout");
        exit(1);
    }
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO) {
        syslog(LOG_ERR, "ERROR while opening '/dev/null' for stderr");
        exit(1);
    }

    pid_t pid;
    int sockets[2];

<<<<<<< HEAD
    int err = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    if (err < 0) {
        perror("socketpair");
        exit(EXIT_FAILURE);
=======
    // Configure button GPIOs
    if (configure_gpio(S1, "in", "rising") == -1)
        return -1;
    if (configure_gpio(S2, "in", "rising") == -1)
        return -1;
    if (configure_gpio(S3, "in", "rising") == -1)
        return -1;

    char buttonStateS1;
    char buttonStateS2;
    char buttonStateS3;

    char frequency[3];
    char mode[3];
    char temperature[3];

    while (1) {
        //Read buttons
        int s1_file = open("/sys/class/gpio/gpio0/value", O_RDONLY);  // Button S1
        int s2_file = open("/sys/class/gpio/gpio2/value", O_RDONLY);  // Button S2
        int s3_file = open("/sys/class/gpio/gpio3/value", O_RDONLY);  // Button S3

        int fs_file = open("/sys/class/mini_project/supervisor/frequency", O_RDWR);  // Frequency from supervisor
        int mode_file = open("/sys/class/mini_project/supervisor/mode", O_RDWR);  // Mode from supervisor
        int temp_file = open("/sys/class/mini_project/supervisor/temperature", O_RDONLY);  // Temperature from supervisor

        //printf("%d %d %d %d %d %d \n", s1_file,s2_file,s3_file,fs_file,mode_file,temp_file);

        if( s1_file!=-1 && s2_file!=-1 && s3_file!=-1 && fs_file!=-1 && mode_file!=-1 && temp_file!=-1 ){ //Giga check
            printf("Enter \n");
            read(s1_file, &buttonStateS1, 1);
            read(s2_file, &buttonStateS2, 1);
            read(s3_file, &buttonStateS3, 1);

            read(fs_file, frequency, 2);
            read(mode_file, mode, 2);
            read(temp_file, temperature, 2);

            int old_frequency = atoi(frequency);
            int old_mode = atoi(mode);

            //printf("%c %c %c\n",buttonStateS1,buttonStateS2,buttonStateS3);

           // printf("%s %s %s\n",frequency,mode,temperature);

            if (buttonStateS3 == '1') {  // Button S3 pressed
                old_mode = (old_mode == 1) ? 0 : 1;
                //Update the mode with sysfs
                sprintf(mode, "%d", old_mode);
                write(mode_file, mode, 1);
            }
            if (old_mode == 0){  // If manual mode is activated
                if (buttonStateS1 == '1' && old_frequency < 20) {  // Button S1 pressed
                    old_frequency++;

                    //Update frequency with sysfs
                    sprintf(frequency, "%d", old_frequency);
                    printf("%s\n",frequency);
                    write(fs_file, frequency, 2);
                }else if (buttonStateS2 == '1' && old_frequency > 1) {  // Button S2 pressed
                    old_frequency--;

                    //Update frequency with sysfs
                    sprintf(frequency, "%d", old_frequency);
                    write(fs_file, frequency, 2);
                }
            }
        }
        close(s1_file);
        close(s2_file);
        close(s3_file);

        close(fs_file);
        close(mode_file);
        close(temp_file);

        usleep(100000);
>>>>>>> refs/remotes/origin/main
    }

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "ERROR while forking");
        exit(1);
    }

    if (pid == 0) {  // Socket process
        int ret = socket_process(sockets);
        if (ret < 0) {
            perror("socket");
            return -1;
        }
    } else if (pid > 0) {  // Main process
        int ret = daemon_process(sockets);
        if (ret < 0) {
            perror("daemon");
            return -1;
        }
    }

    return 0;
}
