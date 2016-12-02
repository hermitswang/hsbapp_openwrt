
#include <getopt.h>
#include <unistd.h>
#include <glib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/socket.h>
#include "hsb_param.h"
#include "hsb_config.h"
#include "thread_utils.h"
#include "daemon_control.h"
#include "debug.h"
#include "utils.h"
#include "unix_socket.h"
#include "network.h"
#include "device.h"


int main (int argc, char **argv)
{
	int opt ;
	gboolean background = FALSE;    

	debug_verbose = DEBUG_DEFAULT_LEVEL;
	opterr = 0;
	while ((opt = getopt (argc, argv, "d:b")) != -1)
		switch (opt) {
		case 'b':
			background = TRUE;
			break;            
		case 'd':
			debug_verbose = atoi(optarg);
			break;
		default:
			break;
		}
    
	if (!daemon_init(&hsb_core_daemon_config, background))
	{
		hsb_critical("init core daemon error\n");
		exit(0);
	}

	if (init_network_module())
	{
		hsb_critical("init net error\n");
		exit(0);
	}

	if (init_dev_module())
	{
		hsb_critical("init dev error\n");
		exit(0);
	}

	daemon_listen_data dla;
	while (1) {
		struct timeval tv = { 1, 0 };
again:
		daemon_select(hsb_core_daemon_config.unix_listen_fd, &tv, &dla);
		if (dla.recv_time != 0) {
			goto again;
		} else {
			check_timer_and_delay();
		}
	}

	return 0;
}


