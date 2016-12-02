#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdint.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include "thread_utils.h"
#include <stdbool.h>

static void dump_buf(void *buf, int len)
{
	uint8_t *data = (uint8_t *)buf;
	int id;

	for (id = 0; id < len; id++) {
		printf("%02X ", data[id]);
		if ((id + 1) % 16 == 0)
			printf("\n");
	}
	
	printf("\n");
}

static void listen_thread(unsigned short port)
{
	int fd = open_udp_listenfd(port);
	fd_set rset;
	int ret, cnt;
	struct timeval tv;
	struct sockaddr_in dev_addr;
	socklen_t dev_len = sizeof(dev_addr);
	uint8_t rbuf[1024];
	uint32_t yes = 1;
	struct ip_mreq mreq;

	set_broadcast(fd, true);

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		printf("reuse addr failed\n");
		return;
	}


#if 1
	char ip[32];
	for (cnt = 1; cnt <= 100; cnt++) {
		snprintf(ip, sizeof(ip), "234.%d.%d.%d", cnt, cnt, cnt);

		mreq.imr_multiaddr.s_addr = inet_addr(ip);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
			printf("join multicast group failed, %s\n", ip);
			return;
		}
	}
#endif

	printf("listen on port %d\n", port);

	while (1) {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);

		ret = select(fd+1, &rset, NULL, NULL, NULL);

		if (ret < 0) {
			printf("select ret=%d", ret);
			continue;
		}

		if (0 == ret) {	/* timeout */
			printf("listen timeout\n");
			break;
		}

		ret = recvfrom(fd, rbuf, sizeof(rbuf), 0, (struct sockaddr *)&dev_addr, &dev_len);

		printf("recv %d bytes from %s\n", ret, inet_ntoa(dev_addr.sin_addr));
		//dump_buf(rbuf, ret);
	}

	return;
}

int main(int argc, char *argv[])
{
	int port;

	if (argc < 2) {
		printf("Usage: %s port\n", argv[0]);
		return -1;
	}

	port = atoi(argv[1]);

	listen_thread(port);

	return 0;
}
