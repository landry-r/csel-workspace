#include <linux/module.h>	    // needed by all modules
#include <linux/init.h>		    // needed for macros
#include <linux/kernel.h>	    // needed for debugging
#include <linux/moduleparam.h>  // needed for module parameters

#include <linux/platform_device.h> // needed for sysfs
#include <linux/uaccess.h>      // needed for sysfs

#include <linux/gpio.h>         // needed for gpio
#include <linux/timer.h>        // needed for timer
#include <linux/thermal.h>      // needed for cpu temperature

#define LED 10 // GPIO10

static struct thermal_zone_device *cpu_thermal;
static struct timer_list timer_led;
static struct timer_list timer_temp;

static struct class* sysfs_class = NULL;
static struct device* sysfs_device = NULL;

// Peripherals attributes
static int frequency = 2;   // Hz
static int mode = 1;        // 0: manual, 1: auto
static int temperature = 0; // °C

ssize_t frequency_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", frequency);
}

ssize_t frequency_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%d", &frequency);
    return count;
}
DEVICE_ATTR(frequency, 0660, frequency_show, frequency_store);

ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", mode);
}

ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%d", &mode);
    return count;
}
DEVICE_ATTR(mode, 0660, mode_show, mode_store);

ssize_t temperature_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", temperature);
}

ssize_t temperature_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    //we can't write temperature
    return count;
}
DEVICE_ATTR(temperature, 0444, temperature_show, temperature_store);

void timer_led_callback(struct timer_list *timer)
{
    // Regule temperature in auto mode
    if (mode == 1)
    {
        if (temperature >= 45) // >=45°C
        {
            frequency = 20; // 20Hz
        }
        else if (temperature < 45) // <45°C
        {
            frequency = 10; // 10Hz
        }
        else if (temperature < 40) // <40°C
        {
            frequency = 5; // 5Hz
        }
        else if (temperature < 35) // <35°C
        {
            frequency = 2; // 2Hz
        }
    }
    pr_info("frequency: %d\n", frequency);

    // Toggle LED
    gpio_set_value(LED, !gpio_get_value(LED));

    // Restart timer
    mod_timer(&timer_led, jiffies + msecs_to_jiffies(1000 / frequency));
}

void timer_temp_callback(struct timer_list *timer)
{
    int err;
    // Get temperature
    err = thermal_zone_get_temp(cpu_thermal, &temperature);
    if (err != 0)
    {
        pr_err("Failed to get temperature with error %d\n", err);
        return;
    }
    temperature /= 1000;
    pr_info("temperature: %d\n", temperature);

    // Restart timer
    mod_timer(&timer_temp, jiffies + msecs_to_jiffies(1000));
}

static int __init supervisor_init(void)
{
    int ret;
    int status = 0;

	pr_info ("Linux module supervisor loaded\n");

    // sysfs
    sysfs_class = class_create(THIS_MODULE, "mini_project");
    if (sysfs_class == NULL) {
        pr_err("Failed to create sysfs class\n");
        return -1;
    }
    sysfs_device = device_create(sysfs_class, NULL, 0, NULL, "supervisor");
    if (sysfs_device == NULL) {
        pr_err("Failed to create sysfs device\n");
        return -1;
    }
    if (status == 0) {
        status = device_create_file(sysfs_device, &dev_attr_frequency);
        if (status) {
            pr_err("Failed to create sysfs file\n");
            return -1;
        }
        status = device_create_file(sysfs_device, &dev_attr_mode);
        if (status) {
            pr_err("Failed to create sysfs file\n");
            return -1;
        }
        status = device_create_file(sysfs_device, &dev_attr_temperature);
        if (status) {
            pr_err("Failed to create sysfs file\n");
            return -1;
        }
    }

    // led
    if (gpio_request(LED, "led") < 0) {
        pr_err("Failed to request GPIO %d\n", LED);
        return -1;
    }
    if (gpio_direction_output(LED, 0) < 0) {
        pr_err("Failed to set GPIO %d as output\n", LED);
        return -1;
    }

    //Get termal zone device
    cpu_thermal = thermal_zone_get_zone_by_name("cpu-thermal");

    // timer
    timer_setup(&timer_led, timer_led_callback, 0);
    ret = mod_timer(&timer_led, jiffies + msecs_to_jiffies(1000));

    timer_setup(&timer_temp, timer_temp_callback, 0);
    ret = mod_timer(&timer_temp, jiffies + msecs_to_jiffies(1000));

	return 0;
}

static void __exit supervisor_exit(void)
{    
    // sysfs
    device_remove_file(sysfs_device, &dev_attr_frequency);
    device_remove_file(sysfs_device, &dev_attr_mode);
    device_remove_file(sysfs_device, &dev_attr_temperature);
    device_destroy(sysfs_class, 0);
    class_destroy(sysfs_class);

    // led
    gpio_set_value(LED, 0);
    gpio_free(LED);

    // timer
    del_timer(&timer_led);
    del_timer(&timer_temp);
    
	pr_info ("Linux module supervisor unloaded\n");
}

module_init (supervisor_init);
module_exit (supervisor_exit);

MODULE_AUTHOR ("Benjamin Caillet, Reynard Landry");
MODULE_DESCRIPTION ("Module supervisor");
MODULE_LICENSE ("GPL");

