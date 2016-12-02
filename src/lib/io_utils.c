

#include "errno.h"
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t read_timeout(int fd, void *buf, size_t count, struct timeval *tv)
{
	int total = 0, nread, ret;
	fd_set rset;
	do {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		ret = select(fd+1, &rset, NULL, NULL, tv);
		if (ret == 0)
			break;
		else if (ret < 0) {
			if (errno == EINTR)
				return total;
			else
				return -1;
		}

		nread = read(fd, buf + total, count - total);
		if (nread < 0) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		} else if (nread == 0)
			return -1;

		total += nread;
		if (total == count)
			break;
	} while (tv && (tv->tv_sec > 0 || tv->tv_usec > 0));

	return total;
}

ssize_t write_timeout(int fd, void *buf, size_t count, struct timeval *tv)
{
	int total = 0, nwrite, ret;
	fd_set wset;
	do {
		FD_ZERO(&wset);
		FD_SET(fd, &wset);
		ret = select(fd+1, NULL, &wset, NULL, tv);
		if (ret == 0)
			break;
		else if (ret < 0) {
			if (errno == EINTR)
				return total;
			else {
				printf("write_timeout select ret < 0\n");
				return -1;
			}
		}

		nwrite = write(fd, buf + total, count - total);
		if (nwrite < 0) {
			if (errno == EINTR)
				continue;
			else {
				printf("write_timeout nwrite < 0, [%d,%d], %d\n", nwrite, count - total, errno - EPIPE);
				return -1;
			}
		} else if (nwrite == 0)
			break;

		total += nwrite;
		if (total == count)
			break;
	} while (tv && (tv->tv_sec > 0 || tv->tv_usec > 0));

	return total;

}

int recvfrom_timeout(int fd, void *buf, size_t count, struct sockaddr *addr, socklen_t *slen, struct timeval *tv)
{
	int nread = 0, ret;
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	ret = select(fd+1, &rset, NULL, NULL, tv);
	if (ret <= 0)
		return ret;

	return recvfrom(fd, buf, count, 0, addr, slen);
}

