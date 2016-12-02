/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: thread_util.h 1040 2006-01-26 02:24:49Z falls $
 * 
 */
#ifndef _HSB_THREAD_UTIL_H_
#define _HSB_THREAD_UTIL_H_

#include <pthread.h>
#include <glib.h>

typedef void * (*thread_entry_func)(void *) ;

typedef struct {
	GQueue *data_queue ;
	pthread_mutex_t mutex ;
	pthread_cond_t cond ;
	gboolean active ;
	pthread_t thread_id;	
} thread_data_control;

gboolean thread_control_activate(thread_data_control *mycontrol) ;

gboolean thread_control_deactivate ( thread_data_control *mycontrol) ;

gboolean thread_control_destroy(thread_data_control *mycontrol) ;

gboolean thread_control_init(thread_data_control *mycontrol) ;

gboolean thread_control_wakeup(thread_data_control *mycontrol) ;

gboolean thread_control_push_data(thread_data_control *mycontrol , gpointer data) ;

gboolean thread_control_push_data_head(thread_data_control *mycontrol , gpointer data) ;

#endif
