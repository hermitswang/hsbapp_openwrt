/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: util.h 1178 2007-04-03 05:18:24Z kf701 $
 * 
 */
#ifndef _UTIL_H
#define _UTIL_H 

#include "hsb_param.h"
#include <glib.h>
#include <stdint.h>
#include <stdbool.h>

int create_pid_file(char *pidfile) ;

int get_pid_from_file(char *pid_file);

size_t encrypt_b64(guchar* dest,guchar* src,size_t size);
size_t decode_b64( guchar *desc ,const guchar *str, int length) ;

ssize_t readline1(int fd, void *vptr, size_t maxlen) ;

gboolean check_cmd_prefix(const char *cmd , const char *prefix);

void hsb_syslog(const char *fmt , ... );

gint hsb_mkstemp (gchar *tmpl);

time_t get_uptime(void);
uint64_t get_msec(void);

bool is_android(void);
const char *get_work_dir(void);
const char *get_eth_interface(void);
const char *get_uart_interface(void);
void get_config_file(char *path);

int str_to_mac(char *buf, uint8_t *pmac);
int mac_to_str(uint8_t *mac, char *buf);

#endif
