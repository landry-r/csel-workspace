/**
 * Copyright 2018 University of Applied Sciences Western Switzerland / Fribourg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Project: HEIA-FR / HES-SO MSE - MA-CSEL1 Laboratory
 *
 * Abstract: System programming -  file system
 *
 * Purpose: NanoPi silly status led control system
 *
 * Autĥor:  Daniel Gachet
 * Date:    07.11.2018
 */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <poll.h>

/*
 * status led - gpioa.10 --> gpio10
 * power led  - gpiol.10 --> gpio362
 */
#define GPIO_EXPORT   "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED      "/sys/class/gpio/gpio10"
#define LED           "10"

static void logFrequencyChange(int frequency)
{
    openlog("LED Control", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Frequency changed to %d Hz", frequency);
    closelog();
}

static int open_led()
{
    // unexport pin out of sysfs (reinitialization)
    int f = open(GPIO_UNEXPORT, O_WRONLY);
    write(f, LED, strlen(LED));
    close(f);

    // export pin to sysfs
    f = open(GPIO_EXPORT, O_WRONLY);
    write(f, LED, strlen(LED));
    close(f);

    // config pin
    f = open(GPIO_LED "/direction", O_WRONLY);
    write(f, "out", 3);
    close(f);

    // open gpio value attribute
    f = open(GPIO_LED "/value", O_RDWR);
    return f;
}

static int open_button(const char* gpio)
{
    int f = open(GPIO_EXPORT, O_WRONLY);
    write(f, gpio, strlen(gpio));
    close(f);

    f = open("/sys/class/gpio/gpio10/direction", O_WRONLY);
    write(f, "in", 2);
    close(f);

    f = open("/sys/class/gpio/gpio10/edge", O_WRONLY);
    write(f, "rising", 6);
    close(f);

    f = open("/sys/class/gpio/gpio10/value", O_RDONLY);
    return f;
}

static void handleButtonPress(int button, int* frequency)
{
    switch (button) {
        case 1: // Button K1 - Increase frequency
            (*frequency)++;
            logFrequencyChange(*frequency);
            break;
        case 2: // Button K2 - Reset frequency to default
            *frequency = 2;
            logFrequencyChange(*frequency);
            break;
        case 3: // Button K3 - Decrease frequency
            (*frequency)--;
            logFrequencyChange(*frequency);
            break;
    }
}

int main(int argc, char* argv[])
{
    int frequency = 2;  // Hz

    // Log initial frequency
    openlog("LED Control", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Initial frequency: %d Hz", frequency);
    closelog();

    long duty   = 50;    // %
    long period = 1000;  // ms
    if (argc >= 2) period = atoi(argv[1]);
    period *= 1000000;  // in ns

    // compute duty period...
    long p1 = period / 100 * duty;
    long p2 = period - p1;

    int led = open_led();
    pwrite(led, "1", sizeof("1"), 0);

    int k = 0;
    while (1) {
        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);

        int k1 = open_button("gpio11");  // Button K1
        int k2 = open_button("gpio12");  // Button K2
        int k3 = open_button("gpio13");  // Button K3

        struct pollfd buttons[3];
        buttons[0].fd = k1;
        buttons[0].events = POLLPRI;
        buttons[1].fd = k2;
        buttons[1].events = POLLPRI;
        buttons[2].fd = k3;
        buttons[2].events = POLLPRI;

        int ret = poll(buttons, 3, -1);
        if (ret > 0) {
            if (buttons[0].revents & POLLPRI) {  // Button K1 pressed
                handleButtonPress(1, &frequency);
            } else if (buttons[1].revents & POLLPRI) {  // Button K2 pressed
                handleButtonPress(2, &frequency);
            } else if (buttons[2].revents & POLLPRI) {  // Button K3 pressed
                handleButtonPress(3, &frequency);
            }
        }

        struct timespec t2;
        clock_gettime(CLOCK_MONOTONIC, &t2);

        long delta =
            (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);

        int toggle = ((k == 0) && (delta >= p1)) | ((k == 1) && (delta >= p2));
        if (toggle) {
            t1 = t2;
            k  = (k + 1) % 2;
            if (k == 0)
                pwrite(led, "1", sizeof("1"), 0);
            else
                pwrite(led, "0", sizeof("0"), 0);
        }
        usleep(100000);
    }

    return 0;
}

