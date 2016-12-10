
#include "debug.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <glib.h>

int  unix_socket_new(void)
{	
	int sockfd = socket (AF_LOCAL, SOCK_DGRAM, 0);
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;	
	if ( sockfd < 0 )
		return -1;
	if (setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		close(sockfd);
		return -1 ;
	}		
	return sockfd;
}

void unix_socket_free(int sockfd)
{
	struct sockaddr_un unaddr;		
    int ret ;
	socklen_t	len = sizeof(unaddr);
	
	if ( sockfd == -1 )
		return ;
	
	bzero (&unaddr, len);
    ret = getsockname ( sockfd , (struct sockaddr*)&unaddr , &len );
	close(sockfd);
	if ( ret == 0 && strlen(unaddr.sun_path) > 0 )
		unlink(unaddr.sun_path);
	return ;
}


//add a unix domain listen fd
//if flags=1, use STREAM, if flags=0, use DGRAM
// return value > 0 mean success , -1 mean failed
int unix_socket_new_listen(const char *unix_path) 
{
	int listenfd;
	socklen_t clilen;
	struct sockaddr_un servaddr;	
	char tmp_name[] = "/tmp/.hsb_unXXXXXX";
	char *real_unix_path ;
	
	listenfd = socket (AF_LOCAL, SOCK_DGRAM, 0);	
	if (listenfd < 0)
		return -1;
	
	if ( unix_path == NULL )
	{
		int fd = hsb_mkstemp(tmp_name);
		if ( fd == -1 ) 
		{
			close(listenfd);
			return -1 ;
		}
		close(fd);		
		real_unix_path = tmp_name;
	} else 
		real_unix_path = (char*)unix_path;
	
	unlink (real_unix_path) ;
	if ( 0 == access(real_unix_path, F_OK) ){
		close(listenfd);
		return -1;
	}
	
	bzero (&servaddr, sizeof (servaddr));
	servaddr.sun_family = AF_LOCAL;
	strcpy (servaddr.sun_path,real_unix_path);
		
	if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0) 
	{
		unlink(real_unix_path);
		close(listenfd);
		return -1;
	} 
	// bind ok
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;	
	if (setsockopt(listenfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		close(listenfd);
		return -1 ;
	}		

	return listenfd;
}

// send a cmd string to unix socket(DGRAM) listenfd
// success return value is the sended string length , else return -1
int unix_socket_send_to(int send_socket , const char *to_unix_path, 
						const char *data , ssize_t send_len)
{
	struct sockaddr_un to_addr;	
	int ret;

	g_return_val_if_fail (send_socket >= 0 , -1 );
	g_return_val_if_fail (to_unix_path != NULL , -1 );
	g_return_val_if_fail (data != NULL   , -1 );

	bzero (&to_addr, sizeof (to_addr));
	to_addr.sun_family = AF_LOCAL;
	strcpy (to_addr.sun_path , to_unix_path); 
	ret = sendto(send_socket , data , send_len ,0, (struct sockaddr *)&to_addr,sizeof(to_addr));
	if (ret < send_len) {
		hsb_critical("%s(%s): %m \n",__FUNCTION__ , to_unix_path);
	}

	return ret;
}

