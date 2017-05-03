/* File: tracer.c */

#include "tracer.h"
#include <stdio.h> 
#include "immintrin.h"
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

void start_pin_tracing()
{
        asm volatile(".byte 0xbb,0x11,0x22,0x33,0x44,0x64,0x67,0x90" : : : "ebx");
}

void stop_pin_tracing()
{
        asm volatile(".byte 0xbb,0x11,0x22,0x33,0x55,0x64,0x67,0x90" : : : "ebx");
}

struct perf_event_attr pe;
int fd[4];
uint64_t id[4];

struct read_format {
    uint64_t nr;
    struct {
        uint64_t value;
        uint64_t id;
    } values[4];
};

long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
        int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
            group_fd, flags);
    return ret;
}


uint64_t create_config(uint8_t event, uint8_t umask, uint8_t cmask) {
    uint64_t config = ((cmask << 24) | (umask << 8) | (event));
    return config;
}

void start_perf_tracing()
{
    printf("Starting Perf Tracing!\n");
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_RAW;
    pe.size = sizeof(struct perf_event_attr);
// All unhalted core execution cycles
    pe.config = create_config(0x3C, 0x00, 0);
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    fd[0] = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd[0] == -1) {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        exit(EXIT_FAILURE);
    }
    ioctl(fd[0], PERF_EVENT_IOC_ID, &id[0]);

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_RAW;
    pe.size = sizeof(struct perf_event_attr);
// Load misses causes a page walk
    pe.config = create_config(0x08, 0x01, 0);
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    fd[1] = syscall(__NR_perf_event_open, &pe, 0, -1, fd[0], 0); // <-- here
    ioctl(fd[1], PERF_EVENT_IOC_ID, &id[1]);

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_RAW;
    pe.size = sizeof(struct perf_event_attr);
// Cycles in Page Walk
    pe.config = create_config(0x08, 0x10, 1);
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    fd[2] = syscall(__NR_perf_event_open, &pe, 0, -1, fd[0], 0); // <-- here
    ioctl(fd[2], PERF_EVENT_IOC_ID, &id[2]);

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_RAW;
    pe.size = sizeof(struct perf_event_attr);
    // Total Instructions Retired
    pe.config = create_config(0xC0, 0x00, 0);
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    fd[3] = syscall(__NR_perf_event_open, &pe, 0, -1, fd[0], 0); // <-- here
    ioctl(fd[3], PERF_EVENT_IOC_ID, &id[3]);

    ioctl(fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    ioctl(fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

}

void stop_perf_tracing()
{
    printf("Stopping perf tracing!\n");
    ioctl(fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

    char buf[4096];
    int i;
    struct read_format* rf = (struct read_format*) buf;
    uint64_t val[4];
    int ret = read(fd[0], buf, sizeof(buf));
    for (i = 0; i < rf->nr; i++) {
        if (rf->values[i].id == id[0]) {
            val[0] = rf->values[i].value;
        } else if (rf->values[i].id == id[1]) {
            val[1] = rf->values[i].value;
        } else if (rf->values[i].id == id[2]) {
            val[2] = rf->values[i].value;
        } else if (rf->values[i].id == id[3]) {
            val[3] = rf->values[i].value;
        }
    }
    close(fd[0]);
    close(fd[1]);
    close(fd[2]);
    close(fd[3]);
    printf("Total CPU Cycles | Total Instructions Retired | Total PageWalk Cycles | Total DTLB misses\n");
    printf("%llu %llu %llu %llu \n", val[0], val[3], val[2], val[1]);  
}

