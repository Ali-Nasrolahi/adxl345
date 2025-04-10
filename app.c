#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "uadxl.h"

#define DEVICE_PATH      "/dev/adxl0"
#define SYSFS_BASE       "/sys/class/adxl_class/adxl0"
#define SYSFS_ATTR(attr) SYSFS_BASE "/" attr

// ANSI color codes for better output
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

// Logging macros with colors and timestamps
#define LOG_SUCCESS(msg)                                                                          \
    do {                                                                                          \
        time_t now = time(NULL);                                                                  \
        printf("[%s%.*s%s] %s%s%s\n", COLOR_GREEN, 8, ctime(&now) + 11, COLOR_RESET, COLOR_GREEN, \
               msg, COLOR_RESET);                                                                 \
    } while (0)

#define LOG_FAILURE(msg)                                                                        \
    do {                                                                                        \
        time_t now = time(NULL);                                                                \
        fprintf(stderr, "[%s%.*s%s] %s%s: %s%s\n", COLOR_RED, 8, ctime(&now) + 11, COLOR_RESET, \
                COLOR_RED, msg, strerror(errno), COLOR_RESET);                                  \
    } while (0)

#define LOG_INFO(msg)                                                                           \
    do {                                                                                        \
        time_t now = time(NULL);                                                                \
        printf("[%s%.*s%s] %s%s%s\n", COLOR_BLUE, 8, ctime(&now) + 11, COLOR_RESET, COLOR_BLUE, \
               msg, COLOR_RESET);                                                               \
    } while (0)

#define LOG_VALUE(msg, value)                                                             \
    do {                                                                                  \
        time_t now = time(NULL);                                                          \
        printf("[%s%.*s%s] %s%s: %s%d%s\n", COLOR_CYAN, 8, ctime(&now) + 11, COLOR_RESET, \
               COLOR_CYAN, msg, COLOR_MAGENTA, value, COLOR_RESET);                       \
    } while (0)

// Test configuration
#define NUM_SAMPLES     5
#define SAMPLE_DELAY_MS 200

void print_test_header(const char *test_name)
{
    printf("\n%s=== %s ===%s\n", COLOR_YELLOW, test_name, COLOR_RESET);
}

void print_test_footer(bool success)
{
    if (success) {
        printf("%sTest completed successfully%s\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%sTest completed with errors%s\n", COLOR_RED, COLOR_RESET);
    }
}

void test_ioctl(int fd)
{
    print_test_header("IOCTL FUNCTIONALITY TEST");
    bool overall_success = true;
    int value;

    // Test enable/disable
    if (ioctl(fd, ADXL_IOCTL_ENABLE) == 0) {
        LOG_SUCCESS("Device enabled");
    } else {
        LOG_FAILURE("Failed to enable device");
        overall_success = false;
    }

    if (ioctl(fd, ADXL_IOCTL_DISABLE) == 0) {
        LOG_SUCCESS("Device disabled");
    } else {
        LOG_FAILURE("Failed to disable device");
        overall_success = false;
    }

    // Test rate configuration
    if (ioctl(fd, ADXL_IOCTL_GET_RATE, &value) == 0) {
        LOG_VALUE("Current data rate", value);
    } else {
        LOG_FAILURE("Failed to get data rate");
        overall_success = false;
    }

    int test_rates[] = {6, 12, 25, 50, 100, 200, 400};
    for (size_t i = 0; i < sizeof(test_rates) / sizeof(test_rates[0]); i++) {
        value = test_rates[i];
        if (ioctl(fd, ADXL_IOCTL_SET_RATE, &value) == 0) {
            LOG_VALUE("Data rate set to", value);

            // Verify the setting
            int readback;
            if (ioctl(fd, ADXL_IOCTL_GET_RATE, &readback) == 0 && readback == value) {
                LOG_SUCCESS("Rate setting verified");
            } else {
                LOG_FAILURE("Rate setting verification failed");
                overall_success = false;
            }
        } else {
            LOG_FAILURE("Failed to set data rate");
            overall_success = false;
        }
    }

    // Test range configuration
    if (ioctl(fd, ADXL_IOCTL_GET_RANGE, &value) == 0) {
        LOG_VALUE("Current measurement range", value);
    } else {
        LOG_FAILURE("Failed to get measurement range");
        overall_success = false;
    }

    int test_ranges[] = {2, 4, 8, 16};
    for (size_t i = 0; i < sizeof(test_ranges) / sizeof(test_ranges[0]); i++) {
        value = test_ranges[i];
        if (ioctl(fd, ADXL_IOCTL_SET_RANGE, &value) == 0) {
            LOG_VALUE("Range set to", value);

            // Verify the setting
            int readback;
            if (ioctl(fd, ADXL_IOCTL_GET_RANGE, &readback) == 0 && readback == value) {
                LOG_SUCCESS("Range setting verified");
            } else {
                LOG_FAILURE("Range setting verification failed");
                overall_success = false;
            }
        } else {
            LOG_FAILURE("Failed to set measurement range");
            overall_success = false;
        }
    }

    // Test calibration
    LOG_INFO("Starting calibration...");
    if (ioctl(fd, ADXL_IOCTL_CALIBRATE) == 0) {
        LOG_SUCCESS("Calibration completed");
    } else {
        LOG_FAILURE("Calibration failed");
        overall_success = false;
    }

    print_test_footer(overall_success);
}

void test_acceleration_readings(int fd)
{
    print_test_header("ACCELERATION READING TEST");
    bool overall_success = true;

    // Enable the device
    if (ioctl(fd, ADXL_IOCTL_ENABLE) != 0) {
        LOG_FAILURE("Failed to enable device for reading test");
        return;
    }

    LOG_INFO("Reading acceleration data...");

    for (int i = 0; i < NUM_SAMPLES; i++) {
        char buf[64];
        int x, y, z;

        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (sscanf(buf, "%d,%d,%d", &x, &y, &z) == 3) {
                printf("%sSample %d: X=%-6d Y=%-6d Z=%-6d%s\n", COLOR_CYAN, i + 1, x, y, z,
                       COLOR_RESET);
            } else {
                LOG_FAILURE("Data format mismatch");
                overall_success = false;
            }
        } else {
            LOG_FAILURE("Read failed");
            overall_success = false;
        }

        usleep(SAMPLE_DELAY_MS * 1000);
    }

    // Disable the device
    if (ioctl(fd, ADXL_IOCTL_DISABLE) != 0) {
        LOG_FAILURE("Failed to disable device after reading test");
        overall_success = false;
    }

    print_test_footer(overall_success);
}

void test_sysfs_interface()
{
    print_test_header("SYSFS INTERFACE TEST");
    bool overall_success = true;

    typedef struct {
        const char *name;
        bool writable;
        const char *test_values[4];
        int num_values;
    } sysfs_attribute;

    sysfs_attribute attributes[] = {
        {"range", true, {"2", "4", "8", "16"}, 4},
        {"rate", true, {"6", "12", "25", "50"}, 4},
        {"x", false, {NULL}, 0},
        {"y", false, {NULL}, 0},
        {"z", false, {NULL}, 0},
    };

    for (size_t i = 0; i < sizeof(attributes) / sizeof(attributes[0]); i++) {
        char path[256];
        char buf[64];
        ssize_t n;

        snprintf(path, sizeof(path), SYSFS_ATTR("%s"), attributes[i].name);
        printf("\nTesting attribute: %s%s%s\n", COLOR_YELLOW, attributes[i].name, COLOR_RESET);

        /* ------------------- Read Test ------------------- */
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            LOG_FAILURE("  Failed to open attribute for reading");
            overall_success = false;
            continue;
        }

        n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            LOG_FAILURE("  Failed to read attribute");
            overall_success = false;
            close(fd);
            continue;
        }

        buf[n] = '\0';
        printf("  Current value: %s%s%s", COLOR_CYAN, buf, COLOR_RESET);
        close(fd);

        /* ------------------- Write Test (if applicable) ------------------- */
        if (!attributes[i].writable) {
            printf("  (Read-only attribute)\n");
            continue;
        }

        for (int j = 0; j < attributes[i].num_values; j++) {
            const char *test_value = attributes[i].test_values[j];

            /* Attempt to write new value */
            fd = open(path, O_WRONLY);
            if (fd < 0) {
                LOG_FAILURE("  Failed to open for writing");
                overall_success = false;
                continue;
            }

            if (write(fd, test_value, strlen(test_value)) != (ssize_t)strlen(test_value)) {
                LOG_FAILURE("  Failed to write attribute");
                overall_success = false;
                close(fd);
                continue;
            }
            close(fd);

            printf("  Set to %s%s%s: ", COLOR_GREEN, test_value, COLOR_RESET);

            /* Verify the write was successful */
            fd = open(path, O_RDONLY);
            if (fd < 0) {
                LOG_FAILURE("  Failed to verify write");
                overall_success = false;
                continue;
            }

            n = read(fd, buf, sizeof(buf) - 1);
            if (n <= 0) {
                LOG_FAILURE("  Failed to read back value");
                overall_success = false;
                close(fd);
                continue;
            }

            buf[n] = '\0';
            bool success = (strcmp(buf, test_value) == 0);
            printf("Readback %s%s%s\n", success ? COLOR_GREEN : COLOR_RED, buf, COLOR_RESET);

            if (!success) { overall_success = false; }
            close(fd);
        }
    }

    print_test_footer(overall_success);
}

void print_help()
{
    printf("\n%sADXL345 Driver Test Application%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("Usage: This application tests all functionality of the ADXL345 driver\n");
    printf("It performs the following tests:\n");
    printf("  1. IOCTL interface testing (enable/disable, rate/range settings)\n");
    printf("  2. Acceleration data reading\n");
    printf("  3. Sysfs attribute interface testing\n\n");
}

int main()
{
    print_help();

    LOG_INFO("Starting ADXL345 driver tests");

    // Open the device
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        LOG_FAILURE("Failed to open device");
        return EXIT_FAILURE;
    }
    LOG_SUCCESS("Device opened successfully");

    // Run tests
    test_ioctl(fd);
    test_acceleration_readings(fd);

    // Close the device
    if (close(fd) == 0) {
        LOG_SUCCESS("Device closed successfully");
    } else {
        LOG_FAILURE("Failed to close device");
    }

    // Test sysfs interface
    test_sysfs_interface();

    LOG_INFO("All tests completed");
    return EXIT_SUCCESS;
}