/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: thread_util.c 1040 2006-01-26 02:24:49Z falls $
 * 
 */

#include "hsb_config.h"
#include "thread_utils.h"
#include "utils.h"
#include "debug.h"
#include "unix_socket.h"
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

gboolean thread_control_init(thread_data_control *mycontrol)
{	
	if (pthread_mutex_init(&(mycontrol->mutex), NULL))
		return FALSE;
	if (pthread_cond_init(&(mycontrol->cond), NULL))
		return FALSE;

	mycontrol->data_queue = g_queue_new();
	mycontrol->active = FALSE;
	return TRUE;
}

gboolean thread_control_destroy(thread_data_control *mycontrol) 
{
	int mystatus;
	if (pthread_mutex_destroy(&(mycontrol->mutex)))
		return FALSE;
	if (pthread_cond_destroy(&(mycontrol->cond)))
		return FALSE;

	g_queue_free(mycontrol->data_queue);
	mycontrol->active = FALSE;
	return TRUE;
}

gboolean thread_control_activate(thread_data_control *mycontrol) 
{
	int mystatus;
	if (pthread_mutex_lock(&(mycontrol->mutex)))
		return FALSE;

	mycontrol->active = TRUE ;
	if (pthread_mutex_unlock(&(mycontrol->mutex)))
		return FALSE;

	if (pthread_cond_broadcast(&(mycontrol->cond)))
		return FALSE;

	return TRUE;
}

gboolean thread_control_deactivate(thread_data_control *mycontrol) 
{
	if (!mycontrol) 
		return FALSE;

	if (pthread_mutex_lock(&(mycontrol->mutex)))
		return FALSE;

	mycontrol->active = FALSE ;
	pthread_mutex_unlock(&(mycontrol->mutex));
	pthread_cond_broadcast(&(mycontrol->cond));
	return TRUE;
}

gboolean thread_control_wakeup(thread_data_control *mycontrol) 
{	
	if (!mycontrol) 
		return FALSE;

	if (0 == pthread_cond_signal(&mycontrol->cond))
		return TRUE;
	else
		return FALSE;
}

gboolean thread_control_push_data(thread_data_control *mycontrol , gpointer data) 
{
	if (!mycontrol || !data) 
		return FALSE;
	
	if (pthread_mutex_lock(&mycontrol->mutex))
		return FALSE;

	g_queue_push_tail(mycontrol->data_queue,data);
	if (pthread_mutex_unlock(&mycontrol->mutex))
		return FALSE;

	return thread_control_wakeup(mycontrol);
}

gboolean thread_control_push_data_head(thread_data_control *mycontrol , gpointer data) 
{
	if (!mycontrol || !data) 
		return FALSE;
	
	if (pthread_mutex_lock(&mycontrol->mutex))
		return FALSE;

	g_queue_push_head(mycontrol->data_queue,data);
	if (pthread_mutex_unlock(&mycontrol->mutex))
		return FALSE;

	return thread_control_wakeup(mycontrol);
}

