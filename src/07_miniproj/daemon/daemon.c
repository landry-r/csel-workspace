/**
 * Copyright 2015 University of Applied Sciences Western Switzerland / Fribourg
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Project: HEIA-FR / Embedded Systems Laboratory
 *
 * Abstract: Process and daemon samples
 *
 * Purpose: This module implements a simple daemon to be launched
 *          from a /etc/init.d/S??_* script
 *          -> this application requires /opt/daemon as root directory
 *
 * Autĥor:  Daniel Gachet
 * Date:    17.11.2015
 */
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "ssd1306.h"

#define S1 "0"
#define S2 "2"
#define S3 "3"

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define BUTTON_GPIO_PREFIX "/sys/class/gpio/gpio"

#define UNUSED(x) (void)(x)

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

int main(int argc, char* argv[])
{
    syslog(LOG_INFO, "daemon started\n");
    UNUSED(argc);
    UNUSED(argv);

    // 1. fork off the parent process
    fork_process();

    // 2. create new session
    if (setsid() == -1) {
        syslog(LOG_ERR, "ERROR while creating new session");
        exit(1);
    }

    // 3. fork again to get rid of session leading process
    fork_process();

    // 4. capture all required signals
    struct sigaction act = {
        .sa_handler = catch_signal,
    };
    sigaction(SIGHUP, &act, NULL);   //  1 - hangup
    sigaction(SIGINT, &act, NULL);   //  2 - terminal interrupt
    sigaction(SIGQUIT, &act, NULL);  //  3 - terminal quit
    sigaction(SIGABRT, &act, NULL);  //  6 - abort
    sigaction(SIGTERM, &act, NULL);  // 15 - termination
    sigaction(SIGTSTP, &act, NULL);  // 19 - terminal stop signal

    // 5. update file mode creation mask
    umask(0027);

    // 6. change working directory to appropriate place
    if (chdir("/opt") == -1) {
        syslog(LOG_ERR, "ERROR while changing to working directory");
        exit(1);
    }

    // 7. close all open file descriptors
    for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
        close(fd);
    }

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

    ssd1306_init();

    // Export button GPIOs
    if (export_gpio(S1) == -1) return -1;
    if (export_gpio(S2) == -1) return -1;
    if (export_gpio(S3) == -1) return -1;

    // Configure button GPIOs
    if (configure_gpio(S1, "in", "rising") == -1) return -1;
    if (configure_gpio(S2, "in", "rising") == -1) return -1;
    if (configure_gpio(S3, "in", "rising") == -1) return -1;

    char buttonStateS1;
    char buttonStateS2;
    char buttonStateS3;

    char frequency[3];
    char mode[3];
    char temperature[3];
    while (1) {
        int s1_file =
            open("/sys/class/gpio/gpio0/value", O_RDONLY);  // Button S1
        int s2_file =
            open("/sys/class/gpio/gpio2/value", O_RDONLY);  // Button S2
        int s3_file =
            open("/sys/class/gpio/gpio3/value", O_RDONLY);  // Button S3

        int fs_file   = open("/sys/class/mini_project/supervisor/frequency",
                           O_RDWR);  // Frequency from supervisor
        int mode_file = open("/sys/class/mini_project/supervisor/mode",
                             O_RDWR);  // Mode from supervisor
        int temp_file = open("/sys/class/mini_project/supervisor/temperature",
                             O_RDONLY);  // Temperature from supervisor

        if (s1_file != -1 && s2_file != -1 && s3_file != -1 && fs_file != -1 &&
            mode_file != -1 && temp_file != -1) {
            int ret = read(s1_file, &buttonStateS1, 1);
            if (ret == -1) {
                syslog(LOG_ERR, "ERROR while reading button S1");
                exit(1);
            }
            ret = read(s2_file, &buttonStateS2, 1);
            if (ret == -1) {
                syslog(LOG_ERR, "ERROR while reading button S2");
                exit(1);
            }
            ret = read(s3_file, &buttonStateS3, 1);
            if (ret == -1) {
                syslog(LOG_ERR, "ERROR while reading button S3");
                exit(1);
            }

            ret = read(fs_file, frequency, 2);
            if (ret == -1) {
                syslog(LOG_ERR, "ERROR while reading frequency");
                exit(1);
            }
            ret = read(mode_file, mode, 1);
            if (ret == -1) {
                syslog(LOG_ERR, "ERROR while reading mode");
                exit(1);
            }
            ret = read(temp_file, temperature, 2);
            if (ret == -1) {
                syslog(LOG_ERR, "ERROR while reading temperature");
                exit(1);
            }

            int old_frequency = atoi(frequency);
            int old_mode      = atoi(mode);

            if (buttonStateS3 == '1') {  // Button S3 pressed
                old_mode = (old_mode == 1) ? 0 : 1;
                // Update the mode with sysfs
                syslog(LOG_INFO, "Mode changed to %d", old_mode);
                write(mode_file, mode, 1);
            }
            if (old_mode == 0) {  // If manual mode is activated
                if (buttonStateS1 == '1' &&
                    old_frequency < 20) {  // Button S1 pressed
                    old_frequency++;

                    // Update frequency with sysfs
                    syslog(LOG_INFO, "Frequency changed to %d", old_frequency);
                    write(fs_file, frequency, 2);
                } else if (buttonStateS2 == '1' &&
                           old_frequency > 1) {  // Button S2 pressed
                    old_frequency--;

                    // Update frequency with sysfs
                    syslog(LOG_INFO, "Frequency changed to %d", old_frequency);
                    write(fs_file, frequency, 2);
                }
            }
        } else {
            syslog(LOG_ERR, "ERROR while opening files");
            exit(1);
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

        close(s1_file);
        close(s2_file);
        close(s3_file);

        close(fs_file);
        close(mode_file);
        close(temp_file);

        usleep(100000);
    }
    syslog(LOG_INFO,
           "daemon stopped. Number of signals catched=%d\n",
           signal_catched);
    closelog();

    return 0;
}