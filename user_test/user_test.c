#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define DEVICE_NODE "/dev/vchar_dev"

/* function to check entry point open of vchar driver */
int open_chardev() {
    int fd = open(DEVICE_NODE, O_RDWR);
    if(fd < 0) {
        printf("Can not open the device file\n");
        exit(1);
    }
    return fd;
}

/*function to check entry point release of vchar driver */
void close_chardev(int fd) {
    close(fd);
}
void read_data_chardev() {
	int ret =0;
	int user_buf;
	
	int fd = open_chardev();
	// system call read
	ret = read(fd, &user_buf, sizeof(user_buf));
	close_chardev(fd);

	if(ret < 0)
		printf("Could not generate random number from %s\n", DEVICE_NODE);
	else
		printf("Read a random number from %s: %d\n", DEVICE_NODE, user_buf);
}

int main() {
    int ret = 0;
    char option = 'q';
    int fd = -1;
    printf("Select below options:\n");
    printf("\tg (to generate a random number from device node)\n");
    printf("\tq (to quit the application)\n");
    while (1) {
        printf("Enter your option: ");
        scanf(" %c", &option);

        switch (option) {
            case 'g':
                read_data_chardev();
		break;
            case 'q':
                return 0;
		
            default:
                printf("press again %c\n", option);
                break;
        }
    };
}
