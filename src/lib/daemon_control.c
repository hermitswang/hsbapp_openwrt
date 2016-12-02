/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: hsb_daemon_control.c 910 2005-12-19 06:55:28Z wang $
 * 
 */

#include "hsb_config.h"
#include "daemon_control.h"
#include "debug.h"
#include "unix_socket.h"
#include <glib.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>
#include "utils.h"

gboolean daemon_init(hsb_daemon_config *my_daemon, gboolean background)
{
	int fd ;
	const char *work_dir = get_work_dir();

	if ( background == TRUE )
		if ( -1 == daemon(0,0) )
			return FALSE;

	umask(0022);
	mkdir(work_dir, 0755);

	if ( -1 == chdir(work_dir) )
		return FALSE ;

	if (total_cfg_read() == FALSE)
		return FALSE;

	if (create_pid_file(my_daemon->pid_file)) 
		return FALSE;

	fd = unix_socket_new_listen( my_daemon->unix_listen_path ) ;
	if (-1 == fd)
	{
		unlink(my_daemon->pid_file);
		unlink(my_daemon->unix_listen_path);
		return FALSE;
	} 

	my_daemon->unix_listen_fd = fd;

	return TRUE;
}

void daemon_exit(hsb_daemon_config *my_daemon)
{	
	unlink(my_daemon->pid_file);
	unlink(my_daemon->unix_listen_path);	
	exit(0);
}

int daemon_select(int unix_listen_fd, struct timeval *tv, daemon_listen_data *dla)
{
	fd_set readset;
	int max_fd , select_ret ;

select_start:
	// clear dla first
	memset(dla, 0, sizeof(*dla));

	FD_ZERO(&readset);
	FD_SET(unix_listen_fd , &readset);
	max_fd = unix_listen_fd;
	select_ret = select(max_fd+1 , &readset , NULL , NULL , tv);
	if ( select_ret < 0 ) {
		hsb_critical("daemon select error %m\n");
		return select_ret;
	}
	
	if (FD_ISSET(unix_listen_fd, &readset))
	{
		int nread;
		struct sockaddr_un un_from_addr;
		socklen_t sock_len = sizeof(un_from_addr);

		bzero(&un_from_addr, sock_len);		
		nread = recvfrom(unix_listen_fd, dla->cmd_buf, sizeof(dla->cmd_buf),
					0 , (struct sockaddr*)&un_from_addr,&sock_len ) ;		
		if ( nread <= 0 ) {
			if ( nread < 0 )
				hsb_critical ( "recvfrom error : %m\n");
		} else {
			dla->cmd_buf[nread] = 0;
			strcpy(dla->reply_path, un_from_addr.sun_path);
			dla->recv_time = time(NULL);
		}
	}

	return select_ret;
}

void daemon_listen_data_free(daemon_listen_data *dla)
{
	if ( dla )
		g_free(dla);
}

