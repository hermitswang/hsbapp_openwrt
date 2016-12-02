

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "network_utils.h"
#include "net_protocol.h"

static int probe_box(struct in_addr *addr)
{
	int sockfd, bid;
	struct sockaddr_in servaddr, baddr;

	sockfd = open_udp_listenfd(19002);

	if (get_broadcast_address(sockfd, "eth0", &servaddr.sin_addr) < 0)
	{
		close(sockfd);
		return -1;
	}
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(19001);
	//inet_aton("192.168.3.100", &servaddr.sin_addr);

	set_broadcast(sockfd, true);

	int n, ret;
	uint8_t sendline[16], recvline[16];
	socklen_t blen = sizeof(baddr);
	socklen_t servlen = sizeof(servaddr);

	memset(sendline, 0, sizeof(sendline));

	int send_len = 16;
	SET_CMD_FIELD(sendline, 0, uint16_t, 0x8801);
	SET_CMD_FIELD(sendline, 2, uint16_t, send_len);
	SET_CMD_FIELD(sendline, 4, uint8_t, 0);
	SET_CMD_FIELD(sendline, 5, uint8_t, 1);
	SET_CMD_FIELD(sendline, 8, uint32_t, 1);

	ret = sendto(sockfd, sendline, send_len, 0, (struct sockaddr *)&servaddr, servlen);

	struct timeval tv = { 2, 0 };
	n = recvfrom_timeout(sockfd, recvline, 1024, (struct sockaddr *)&baddr, &blen, &tv);
	if (n <= 0) {
		close(sockfd);
		return -2;
	}

	printf("recv %d bytes\n", n);

	uint16_t cmd = GET_CMD_FIELD(recvline, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(recvline, 2, uint16_t);
	bid = GET_CMD_FIELD(recvline, 8, uint32_t);

	printf("cmd: %x, len: %d, bid: %d\n", cmd, len, bid);

	char addstr[32];
	inet_ntop(AF_INET, &baddr.sin_addr, addstr, 16);

	printf("box address: %s\n", addstr);

	close(sockfd);

	memcpy(addr, &baddr.sin_addr, sizeof(*addr));

	return 0;
}

int main(int argc, char *argv[])
{
	struct in_addr addr;

	while (1) {
		if (probe_box(&addr))
			continue;
	}
	
	return 0;
}


