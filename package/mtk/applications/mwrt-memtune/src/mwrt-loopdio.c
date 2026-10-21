#include <fcntl.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 3 || (argv[2][0] != '0' && argv[2][0] != '1') || argv[2][1]) {
        fprintf(stderr, "usage: mwrt-loopdio /dev/loopN 0|1\n");
        return 2;
    }
    int fd = open(argv[1], O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror(argv[1]); return 1; }
    int rc = ioctl(fd, LOOP_SET_DIRECT_IO, (unsigned long)(argv[2][0] - '0'));
    if (rc < 0) perror("LOOP_SET_DIRECT_IO");
    close(fd);
    return rc < 0;
}
