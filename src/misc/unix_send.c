

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
#include "network_utils.h"

static int deal_input_cmd(int sockfd, uint8_t *buf, int len, struct sockaddr_in *addr)
{
	uint32_t val = 0;

	printf("sendto %s [%d] bytes\n", inet_ntoa(addr->sin_addr), len);
	sendto(sockfd, buf, len, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));

	return 0;
}

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(struct sockaddr_in);

	if (argc < 2) {
		printf("Usage: %s ip port\n", argv[0]);
		return -2;
	}

	sockfd = open_udp_clientfd();
	if (sockfd < 0) {
		printf("open client fd fail\n");
		return -1;
	}

	int port = atoi(argv[2]);

	printf("send to port %d\n", port);

	addr.sin_family = AF_INET;
	inet_aton(argv[1], &addr.sin_addr);
	addr.sin_port = htons(port);

	int nread, inputfd = 0;
	fd_set readset;
	uint8_t buf[1024];
	while (1) {
		FD_ZERO(&readset);
		FD_SET(inputfd, &readset);
		ret = select(inputfd+1, &readset, NULL, NULL, NULL);
		if (ret <= 0) {
			printf("connfd select error %m\n");
			continue;
		}

		if (FD_ISSET(inputfd, &readset)) {
			nread = read(inputfd, buf, sizeof(buf));
			buf[nread] = 0;

			deal_input_cmd(sockfd, buf, nread-1, &addr);
		}
	}

	return 0;
}

