#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h> 
#include <errno.h>

#define MAX_LENGTH 200

char thermal_file[MAX_LENGTH] = "/sys/devices/virtual/thermal/thermal_zone0/temp";
char fan_file[MAX_LENGTH] = "/sys/devices/virtual/thermal/cooling_device0/cur_state";

int start_speed = 128;
int start_temp = 50;
int max_temp = 78;
int max_speed = 255;
int temp_div = 1000;
int interval = 5;
int debug_mode = 0;
static volatile sig_atomic_t stopping;

/**
 * 底层读文件
 */
static int read_value(const char* path, int* value) {
    FILE* fp = fopen(path, "r");
    int ret, saved_errno;

    if (fp == NULL)
        return -1;

    errno = 0;
    ret = fscanf(fp, "%d", value);
    saved_errno = errno;
    if (ret != 1 && saved_errno == 0)
        saved_errno = ferror(fp) ? EIO : (feof(fp) ? ENODATA : EINVAL);
    fclose(fp);
    errno = saved_errno;
    return ret == 1 ? 0 : -1;
}

/**
 * 底层写文件
 */
static int write_value(const char* path, int value) {
    FILE* fp = fopen(path, "w");
    int ret;

    if (fp == NULL)
        return -1;

    ret = fprintf(fp, "%d\n", value);
    if (fclose(fp) != 0)
        return -1;
    return ret > 0 ? 0 : -1;
}

/**
 * 读取温度
 */
int get_temperature(char* thermal_file ,int div) {
    int value;

    if (div <= 0) {
        errno = EINVAL;
        return -1;
    }
    if (read_value(thermal_file, &value) != 0)
        return -1;
    if (value < 0)
        errno = ERANGE;
    return value / div;
}

/**
 * 读取风扇速度
 */
int get_fanspeed(char* fan_file) {
    int value;

    return read_value(fan_file, &value) == 0 ? value : -1;
}

/**
 * 设置风扇转速
 */
int set_fanspeed(int fan_speed ,char* fan_file) {
    return write_value(fan_file, fan_speed);
}

/**
 * 计算风扇转速
 */
int calculate_speed(int current_temp ,int max_temp ,int min_temp ,int max_speed ,int min_speed) {
    if (current_temp <= min_temp)
        return min_speed;
    if (current_temp >= max_temp)
        return max_speed;

    int fan_speed = ( current_temp - min_temp ) * ( max_speed - min_speed ) / ( max_temp - min_temp ) + min_speed;
    return fan_speed;
}

/**
 * 判断文件是否存在方法
 */
static int file_exist(const char* name) {
    struct stat buffer;
    return stat(name ,&buffer);
}

/**
 *  信号处理函数
 */
void handle_termination(int signum) {
    stopping = 1;
}

/**
 * 注册信号处理函数
 */
void register_signal_handlers( ) {
    struct sigaction sa;
    memset(&sa ,0 ,sizeof(sa));
    sa.sa_handler = handle_termination;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT ,&sa ,NULL);
    sigaction(SIGTERM ,&sa ,NULL);
}

/**
 * 主函数
 */
int main(int argc ,char* argv[ ]) {
    int opt;
    int last_speed = -1;
    unsigned long failed_samples = 0;

    while (( opt = getopt(argc ,argv ,"T:F:s:t:x:m:d:i:D:") ) != -1) {
        switch (opt) {
            case 'T':
                snprintf(thermal_file ,sizeof(thermal_file) ,"%s" ,optarg);
                break;
            case 'F':
                snprintf(fan_file ,sizeof(fan_file) ,"%s" ,optarg);
                break;
            case 's':
                start_speed = atoi(optarg);
                break;
            case 't':
                start_temp = atoi(optarg);
                break;
            case 'x':
                max_temp = atoi(optarg);
                break;
            case 'm':
                max_speed = atoi(optarg);
                break;
            case 'd':
                temp_div = atoi(optarg);
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            case 'D':
                debug_mode = atoi(optarg);
                break;
            default:
                fprintf(stderr ,"Usage: %s [option]\n"
                    "          -T sysfs         # temperature sysfs file, default is '%s'\n"
                    "          -F sysfs         # fan sysfs file, default is '%s'\n"
                    "          -s speed         # initial speed for fan startup, default is %d\n"
                    "          -t temperature   # minimum curve temperature, default is %d°C\n"
                    "          -x temperature   # maximum curve temperature, default is %d°C\n"
                    "          -m speed         # fan maximum speed, default is %d\n"
                    "          -d div           # temperature divide, default is %d\n"
                    "          -i seconds       # polling interval, default is %d\n"
                    "          -D 0|1           # debug output\n" ,argv[0] ,thermal_file ,fan_file ,start_speed ,start_temp ,max_temp ,max_speed ,temp_div ,interval);
                exit(EXIT_FAILURE);
        }
    }

    if (start_temp <= 0 || max_temp <= start_temp || start_speed <= 0 ||
        max_speed < start_speed || max_speed > 255 || temp_div <= 0 ||
        interval <= 0) {
        fprintf(stderr, "Invalid fan curve parameters\n");
        exit(EXIT_FAILURE);
    }

    if (file_exist(fan_file) != 0 || file_exist(thermal_file) != 0) {
        fprintf(stderr ,"File: '%s' or '%s' not exist\n" ,fan_file ,thermal_file);
        exit(EXIT_FAILURE);
    }

    register_signal_handlers( );

    /* Ensure the fan starts reliably before settling on the linear curve. */
    if (set_fanspeed(max_speed, fan_file) != 0) {
        fprintf(stderr, "Unable to start fan through '%s'\n", fan_file);
        exit(EXIT_FAILURE);
    }
    last_speed = max_speed;
    usleep(500000);

    while (!stopping) {
        int temperature = get_temperature(thermal_file ,temp_div);
        int fan_speed;

        if (temperature < 0) {
            int read_errno = errno;
            fan_speed = max_speed;
            if (!failed_samples)
                fprintf(stderr, "Temperature read failed (%s, errno=%d: %s); forcing maximum fan speed\n",
                        thermal_file, read_errno, strerror(read_errno));
            failed_samples++;
        } else {
            if (failed_samples)
                fprintf(stderr, "Temperature read recovered after %lu failed samples\n", failed_samples);
            fan_speed = calculate_speed(temperature, max_temp, start_temp,
                                        max_speed, start_speed);
            failed_samples = 0;
        }

        if (fan_speed != last_speed) {
            if (set_fanspeed(fan_speed, fan_file) != 0) {
                fprintf(stderr, "Unable to write fan speed to '%s'\n", fan_file);
            } else {
                last_speed = fan_speed;
            }
        }

        if (debug_mode) {
            fprintf(stdout, "Temperature: %d°C, Fan Speed: %d\n",
                    temperature, get_fanspeed(fan_file));
            fflush(stdout);
        }
        sleep(interval);
    }

    set_fanspeed(0, fan_file);
    return 0;
}
