#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>

int main(int argc, char* argv[])
{
    struct input_event ev[64];
    int fevdev = -1;
    int result = 0;
    int size = sizeof(struct input_event);
    int rd;
    int value;
    char name[256] = "Unknown";
    char *device = "/dev/input/by-id/usb-dijkstra.xyz_Gojira_Beta-event-if02";


    fevdev = open(device, O_RDONLY);
    if (fevdev == -1) {
        printf("Failed to open event device.\n");
        exit(1);
    }

    result = ioctl(fevdev, EVIOCGNAME(sizeof(name)), name);
    printf ("Reading From : %s (%s)\n", device, name);

    printf("Getting exclusive access: ");
    result = ioctl(fevdev, EVIOCGRAB, 1);
    printf("%s\n", (result == 0) ? "SUCCESS" : "FAILURE");

    while (1)
    {
        if ((rd = read(fevdev, ev, size * 64)) < size) {
            break;
        }

        value = ev[0].value;

        if (value != ' ' && ev[1].value == 1 && ev[1].type == 1) {
            printf ("Code[%d]\n", (ev[1].code));
        }
    }

    printf("Exiting.\n");
    result = ioctl(fevdev, EVIOCGRAB, 1);
    close(fevdev);
    return 0;
}
