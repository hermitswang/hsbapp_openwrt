/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: hsb_config.h 1120 2006-11-21 01:57:00Z wangxiaogang $
 * 
 */
#ifndef _HSB_CONFIG_H_
#define _HSB_CONFIG_H_

#define SYSLOG_DIR	"/var/log/"
#define PID_DIR	"/var/run/"
#define LINUX_WORK_DIR "/tmp/hsb/"
#define ANDROID_WORK_DIR "/dev/hsb/"
#define EXECUTE_PATH  "/opt/bin/"

#define LINUX_CONFIG_DIR	""
#define ANDROID_CONFIG_DIR	"/data/data/com.cg.voiceservice/"
#define HSB_CONFIG_FILE		"hsb.xml"

#define LINUX_ETH_INTERFACE	"br-lan"
#define ANDROID_ETH_INTERFACE	"wlan0"

#define MAX_TCP_CLIENT_NUM	(10)

#define MAXLINE 1024UL
#define MAXPATH 256UL

#define CHECK_BIT(a, b)		(a & (1 << b))

#define HSB_CHANNEL_MAX_NAME_LEN	(16)
#define HSB_SCENE_MAX_NAME_LEN		(16)

typedef struct {
	char *pid_file ;
	char *unix_listen_path;	
	int unix_listen_fd ;
} hsb_daemon_config ;

static hsb_daemon_config hsb_core_daemon_config = {
	.pid_file = "hsb_core_daemon.pid" ,
	.unix_listen_path = "hsb_core_daemon.listen" ,	
	.unix_listen_fd = -1 ,
} ;

static hsb_daemon_config hsb_asr_daemon_config = {
	.pid_file = "hsb_asr_daemon.pid" ,
	.unix_listen_path = "hsb_asr_daemon.listen" ,	
	.unix_listen_fd = -1 ,
} ;

static hsb_daemon_config hsb_awaken_daemon_config = {
	.pid_file = "hsb_awaken_daemon.pid" ,
	.unix_listen_path = "hsb_awaken_daemon.listen" ,	
	.unix_listen_fd = -1 ,
} ;

#endif

