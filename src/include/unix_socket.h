/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: unix_socket.h 1120 2006-11-21 01:57:00Z wangxiaogang $
 * 
 */

#ifndef _UNIX_SOCKET_H
#define _UNIX_SOCKET_H
#include <sys/types.h>

int unix_socket_new(void);

void unix_socket_free(int sockfd);

int unix_socket_send_to(int send_socket , const char *to_unix_path, 
						const char *data , ssize_t send_len);

int unix_socket_new_listen(const char *unix_path) ;

#endif
