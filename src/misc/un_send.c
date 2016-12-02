#include "debug.h"
#include <stdio.h>
#include "utils.h"
#include <time.h>
#include "unix_socket.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

void mysend (char *un_path , char *send_str , int read)
{
	int fd = unix_socket_new_listen(NULL);

	int ret = unix_socket_send_to(fd , un_path, send_str, strlen(send_str) );
	if ( ret < 0 )
		printf("send error\n");
	else
		printf("send %d bytes\n",ret);

	if ( read > 0 )
	{
		printf("ready to read\n");
		char buf[1024];
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;	
		setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
		int nread = recvfrom ( fd ,buf ,1024 ,0,0,0) ;
		if ( nread > 0 )
		{
			buf[nread] = 0 ;
			printf("reply=%s\n", buf );
		}
	}

	unix_socket_free(fd);
}

int main( int argc , char *argv[])
{
    debug_verbose=4;

	if ( argv[3] )
	    mysend(argv[1] , argv[2], atoi(argv[3]) ) ;
	else
		mysend(argv[1] , argv[2], 0 );
    return 0;

}
