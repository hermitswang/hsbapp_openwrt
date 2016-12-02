
#ifndef _FD_UTILS_H_
#define _FD_UTILS_H_

#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t read_timeout(int fd, void *buf, size_t count, struct timeval *tv);

ssize_t write_timeout(int fd, void *buf, size_t count, struct timeval *tv);

int recvfrom_timeout(int fd, void *buf, size_t count, struct sockaddr *addr, socklen_t *slen, struct timeval *tv);
#endif
