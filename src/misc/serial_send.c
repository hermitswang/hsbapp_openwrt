

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	int serialfd, ret;

	if (argc < 3) {
		printf("Usage: %s <interface> <bardrate>", argv[0]);
		return -1;
	}

	serialfd = open_serial_fd(argv[1], atoi(argv[2]));

	if (serialfd < 0) {
		printf("open serial fd fail\n");
		return -1;
	}

	int nread, inputfd = 0;
	fd_set readset;
	uint8_t buf[1024];
	while (1) {
		FD_ZERO(&readset);
		FD_SET(inputfd, &readset);
		FD_SET(serialfd, &readset);
		ret = select(serialfd+1, &readset, NULL, NULL, NULL);
		if (ret <= 0) {
			printf("select error %m\n");
			continue;
		}

		if (FD_ISSET(inputfd, &readset)) {
			nread = read(inputfd, buf, sizeof(buf));
			buf[nread] = 0;

			if (nread - 1 > 0)
				write(serialfd, buf, nread - 1);
		}

		if (FD_ISSET(serialfd, &readset)) {
			nread = read(serialfd, buf, sizeof(buf));
			buf[nread] = 0;

			printf("nread=%d\n", nread);
			int i;
			for (i = 0; i < nread; i++)
				printf("%02X ", buf[i]);
			printf("\n");
		}
	}

	return 0;
}

