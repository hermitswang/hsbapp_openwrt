

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "unix_socket.h"
#include "hsb_config.h"

#define LISTEN_PATH	LINUX_WORK_DIR"test2.listen"
#define SEND_PATH	LINUX_WORK_DIR"test.listen"

int deal_sock_cmd(int fd, uint8_t *buf, int len)
{

	return 0;
}

int deal_input_cmd(int fd, char *buf)
{

	return 0;
}

int main(int argc, char *argv[])
{
	int sockfd = unix_socket_new_listen(LISTEN_PATH);
	fd_set rset;
	uint8_t buf[1024];
	int ret, nread, inputfd = 0;

	while (1) {
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		FD_SET(inputfd, &rset);

		ret = select(sockfd + 1, &rset, NULL, NULL, NULL);

		if (ret <= 0)
			continue;

		if (FD_ISSET(sockfd, &rset)) {
			nread = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
			
			deal_sock_cmd(sockfd, buf, nread);
		}

		if (FD_ISSET(inputfd, &rset)) {
			nread = read(inputfd, buf, sizeof(buf));
			buf[nread] = 0;

			deal_input_cmd(sockfd, buf);
		}
	}
	
	return 0;
}


