/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: hsb_daemon_control.h 217 2005-08-29 05:41:23Z wang $
 * 
 */
#ifndef _DAEMON_CONTROL_H_
#define _DAEMON_CONTROL_H_

#include "hsb_config.h"
#include <sys/select.h>
#include <glib.h>
#include <sys/time.h>
#include <time.h>

typedef struct {	
	char reply_path[MAXPATH];
	char cmd_buf[MAXLINE];
	time_t recv_time ;
} daemon_listen_data;

// return : the listen unix fd of the daemon
gboolean daemon_init (hsb_daemon_config *my_daemon , gboolean background);

void daemon_exit (hsb_daemon_config *my_daemon );

int daemon_select (int unix_listen_fd , struct timeval *tv,daemon_listen_data *dla );

void daemon_listen_data_free(daemon_listen_data *dla);

#endif
