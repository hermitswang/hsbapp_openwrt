
#include "debug.h"
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <syslog.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdbool.h>
#include "network_utils.h"
#include "hsb_config.h"
#include "utils.h"

int connect_nonb(const char *ip,int port,struct timeval *timeout) 
{
	int sockfd, flags, n;
	int sock_error;
	socklen_t		len;
	fd_set			rset, wset;
	struct timeval	tval ;
	struct sockaddr_in servaddr;	
	struct hostent *hent;
		
	if (NULL == ip)
		return -1;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		return -1;	

	hent = gethostbyname(ip);//	byname?

	if (NULL == hent) 
	{
		close(sockfd);
		return -1;
	}
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	//inet_aton( ip, &servaddr.sin_addr );	
	servaddr.sin_addr = *(struct in_addr *)hent->h_addr;
	
	flags = fcntl(sockfd, F_GETFL, 0);
	if (-1 == flags) 
	{
		close(sockfd);
		return -1;
	}
	if ( -1 == fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) )
	{
		close(sockfd);
		return -1;
	}

	if ( (n = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) < 0)
	{	
		if (errno != EINPROGRESS)
		{
			hsb_debug("connect=%m\n");			
			close(sockfd);
			return -1;
		}
	}

	// Do whatever we want while the connect is taking place. 
	if( 0 == n )
		return sockfd;	

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	wset = rset;
	tval.tv_sec = timeout->tv_sec;
	tval.tv_usec = timeout->tv_usec;

	n = select(sockfd+1, &rset, &wset, NULL, &tval) ;
	if ( n <= 0 )	
	{
		close(sockfd);		// timeout or error
		return -1;
	}

	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) 
	{		
		len = sizeof(sock_error);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sock_error, &len) < 0)
		{  
			close(sockfd);			
			return(-1);
		}
		if (sock_error)
		{
			errno = sock_error;
			hsb_debug("%s : %s\n" ,__func__ , strerror(errno));
			close(sockfd);
			return -1;
		}		
	} else {
		close(sockfd);
		return -1;
	}
	// restore file status flags 
	if ( fcntl(sockfd, F_SETFL, flags) < 0 )
	{
		close(sockfd);
		return -1;
	}
		
	return sockfd;
}

int open_tcp_listenfd(unsigned short port)
{
	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;
	
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)) < 0)
		goto error;

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	serveraddr.sin_port = htons((unsigned short)port);

	if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		goto error;

	if (listen(listenfd, 5) < 0)
		goto error;

	return listenfd;
error:
	close(listenfd);
	return -1;
}

int open_udp_listenfd(unsigned short port)
{
	int sockfd;
	const int on = 1;

	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

	return sockfd;
}

int open_udp_clientfd(void)
{
	return socket(AF_INET, SOCK_DGRAM, 0);
}

void make_sockaddr(struct sockaddr_in *dst, const struct in_addr *addr, unsigned short port)
{
	dst->sin_family = AF_INET;
	dst->sin_port = htons(port);
	memcpy(&dst->sin_addr, addr, sizeof(*addr));
}

int set_broadcast(int sockfd, bool enable)
{
	const int on = enable ? 1 : 0;
	int ret;

	ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

	return ret;
}	

int get_broadcast_address(int sockfd, struct in_addr *addr)
{
	struct sockaddr_in taddr;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, get_eth_interface(), sizeof(ifr.ifr_name));

	if (ioctl(sockfd, SIOCGIFBRDADDR, &ifr) < 0)
	{
		printf("get broadcast addr fail, sockfd=%d\n", sockfd);
		return -1;
	}

	memcpy(&taddr, &ifr.ifr_broadaddr, sizeof(struct sockaddr));

	memcpy(addr, &taddr.sin_addr, sizeof(struct in_addr));

	return 0;
}

int get_ip(struct in_addr *addr)
{
	struct sockaddr_in taddr;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, get_eth_interface(), sizeof(ifr.ifr_name));

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("socket fail\n");
		return -1;
	}

	if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0)
	{
		printf("get ip fail, sockfd=%d\n", sockfd);
		return -2;
	}

	memcpy(&taddr, &ifr.ifr_addr, sizeof(struct sockaddr));

	memcpy(addr, &taddr.sin_addr, sizeof(struct in_addr));

	close(sockfd);
	return 0;
}
