#include <linux/module.h>	// needed by all modules
#include <linux/init.h>		// needed for macros
#include <linux/kernel.h>	// needed for debugging
#include <linux/thermal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>

#include "ssd1306.h"

/*
 * status led - gpioa.10 --> gpio10
 * power led  - gpiol.10 --> gpio362
 */
#define GPIO_EXPORT   "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED      "/sys/class/gpio/gpio10"
#define LED           "10"

#define BUTTON_GPIO_PREFIX "/sys/class/gpio/gpio"

static struct task_struct* my_thread;

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

static int configure_gpio(const char* gpio, const char* direction, const char* edge)
{
    char gpio_direction_path[256];
    snprintf(gpio_direction_path, sizeof(gpio_direction_path), "%s%s/direction", BUTTON_GPIO_PREFIX, gpio);

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
    snprintf(gpio_edge_path, sizeof(gpio_edge_path), "%s%s/edge", BUTTON_GPIO_PREFIX, gpio);
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

static void handleButtonPress(int button, int* frequency)
{
    switch (button) {
        case 1: // Button K1 - Increase frequency
            if (*frequency < 20)
                (*frequency)++;
            break;
        case 2: // Button K2 - Reset frequency to default
            *frequency = 2;
            break;
        case 3: // Button K3 - Decrease frequency
            if (*frequency > 1)
                (*frequency)--;
            break;
    }
}

int process_thread(void* data)
{
    int frequency = 2;  // Hz

    long duty   = 50;    // %
    long period = 1000;  // ms
    period *= 1000000/frequency;  // in ns
    period /= frequency;

    // compute duty period...
    long p1 = period / 100 * duty;
    long p2 = period - p1;

    int led = open_led();
    pwrite(led, "1", sizeof("1"), 0);

    // Export button GPIOs
    if (export_gpio("0") == -1)
        return -1;
    if (export_gpio("2") == -1)
        return -1;
    if (export_gpio("3") == -1)
        return -1;

    // Configure button GPIOs
    if (configure_gpio("0", "in", "rising") == -1)
        return -1;
    if (configure_gpio("2", "in", "rising") == -1)
        return -1;
    if (configure_gpio("3", "in", "rising") == -1)
        return -1;

    int k = 0;

    char buttonStateK1;
    char buttonStateK2;
    char buttonStateK3;

    char mode;

    //Get access to cpu temps
    struct thermal_zone_device *tz;
    tz = thermal_zone_get_zone_by_name("cpu-thermal");

    int temperature;

    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    while (!kthread_should_stop()) {
        //Read auto-manuel conf

        int mode_config = open("/sys/module/supervisor/mode/value", O_RDONLY);  // A voir pour le chemin
        read(mode_config, &mode, 1);

        if (mode == '1') {  // Auto update
            thermal_zone_get_temp(tz, &temperature);
            if(temperature < 35){
                frequency = 2;
            }else if(temperature < 40){
                frequency = 5;
            }else if(temperature < 45){
                frequency = 10;
            }else{
                frequency = 20;
            }
        }else{ // Manual update

            //Read buttons
            int k1 = open("/sys/class/gpio/gpio0/value", O_RDONLY);  // Button K1
            int k2 = open("/sys/class/gpio/gpio2/value", O_RDONLY);  // Button K2
            int k3 = open("/sys/class/gpio/gpio3/value", O_RDONLY);  // Button K3

            read(k1, &buttonStateK1, 1);
            read(k2, &buttonStateK2, 1);
            read(k3, &buttonStateK3, 1);

            close(k1);
            close(k2);
            close(k3);

            if (buttonStateK1 == '1') {  // Button K1 pressed
                handleButtonPress(1, &frequency);
            }
            if (buttonStateK2 == '1') {  // Button K2 pressed
                handleButtonPress(2, &frequency);
            }
            if (buttonStateK3 == '1') {  // Button K3 pressed
                handleButtonPress(3, &frequency);
            }
        }

        //update LED flashing
        period = 1000;  // ms
        period *= 1000000;
        period /= frequency;

        // compute duty period...
        p1 = period / 100 * duty;
        p2 = period - p1;

        struct timespec t2;
        clock_gettime(CLOCK_MONOTONIC, &t2);
        long delta = (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);
        int toggle = ((k == 0) && (delta >= p1)) | ((k == 1) && (delta >= p2));

        // Update LED status
        if (toggle) {
            t1 = t2;
            k  = (k + 1) % 2;
            if (k == 0)
                pwrite(led, "1", sizeof("1"), 0);
            else
                pwrite(led, "0", sizeof("0"), 0);
        }

        //Update screen information
        ssd1306_set_position (0,0);
        ssd1306_puts("CSEL1a - SP.07");
        ssd1306_set_position (0,1);
        ssd1306_puts("  Demo - SW");
        ssd1306_set_position (0,2);
        ssd1306_puts("--------------");

        ssd1306_set_position (0,3);
        ssd1306_puts("Temp: 35'C"); //To Do -> update information
        ssd1306_set_position (0,4);
        ssd1306_puts("Freq: 1Hz");
        ssd1306_set_position (0,5);
        ssd1306_puts("Duty: 50%");

        usleep(10000);
    }

    return 0;
}

static int __init supervisor_init(void)
{
	pr_info ("Linux module supervisor loaded\n");
    //Init screen
    ssd1306_init();

    my_thread = kthread_run (process_thread, 0, "s/thread");

	return 0;
}

static void __exit supervisor_exit(void)
{
	pr_info ("Linux module supervisor unloaded\n");
    kthread_stop(my_thread);
}

module_init (supervisor_init);
module_exit (supervisor_exit);

MODULE_AUTHOR ("Benjamin Caillet et Landry Reynard");
MODULE_DESCRIPTION ("Module supervisor");
MODULE_LICENSE ("GPL");

