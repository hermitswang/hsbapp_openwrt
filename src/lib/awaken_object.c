
#include "awaken_object.h"
#include "hsb_config.h"
#include "utils.h"
#include "unix_socket.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static gboolean _nut_send_cmd_to_awaken_daemon (char *send_str)
{		
	if (NULL == send_str)
		return FALSE ;
	
	int fd = unix_socket_new();
	if ( fd < 0 )
		return FALSE;

	int ret = unix_socket_send_to(fd , hsb_awaken_daemon_config.unix_listen_path,
								send_str , strlen(send_str) );
	unix_socket_free(fd);
	hsb_debug("send cmd to awaken daemon: %s\n" , send_str);
	if ( ret == strlen(send_str) )
		return TRUE ;
	else
		return FALSE ;
}

static gboolean _nut_start(void)
{
	return _nut_send_cmd_to_awaken_daemon(HSB_CMD_AWAKEN_START_LISTENING);
}

static gboolean _nut_stop(void)
{
	return _nut_send_cmd_to_awaken_daemon(HSB_CMD_AWAKEN_STOP_LISTENING);
}

gboolean hsb_awaken_object_init(hsb_awaken_object * aobj)
{
	if ( NULL == aobj ) return FALSE;
	aobj->start = _nut_start;
	aobj->stop = _nut_stop;
	return TRUE ;
}
