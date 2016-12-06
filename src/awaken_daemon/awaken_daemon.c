

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include "unix_socket.h"
#include "daemon_control.h"
#include "hsb_config.h"
#include "debug.h"
#include "utils.h"

#include "msp_cmn.h"
#include "qivw.h"
#include "msp_errors.h"

#include "ivw.h"

static void on_ivw_result(int errcode)
{
	printf("ivw result: %d\n", errcode);
	// TODO
}

static int demo_ivw(const char *session_begin_params)
{
	int ret;
	struct ivw_rec ivwr;
	struct ivw_notifier notify = {
		on_ivw_result,
	};

	ret = ivw_init(&ivwr, session_begin_params, &notify);
	if (0 != ret) {
		printf("ivw init fail: %d\n", ret);
		goto ivw_exit;
	}

	ret = ivw_start_listening(&ivwr);
	if (0 != ret) {
		printf("ivw start listening fail: %d\n", ret);
		goto ivw_exit;
	}

	while (1) {
		sleep(1);
	}

	ivw_stop_listening(&ivwr);
	ivw_uninit(&ivwr);

	return 0;

ivw_exit:

	ivw_stop_listening(&ivwr);
	ivw_uninit(&ivwr);

	return -1;
}

static int deal_awaken_cmd(daemon_listen_data *dla, struct ivw_rec *pivw)
{
	char *buf = dla->cmd_buf;

	if (check_cmd_prefix(buf, "sleep")) {
		ivw_start_listening(pivw);
	} else {
		printf("unknown cmd: [%s]\n", buf);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int opt, ret;

	const char *lgi_param = "appid = 57e4884c,engine_start = ivw,ivw_res_path =fo|res/ivw/wakeupresource.jet, work_dir = /tmp/hsb/ivw";
	const char *ssb_param = "ivw_threshold=0:-20,sst=wakeup";

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
 
	if (!daemon_init(&hsb_asr_daemon_config, background))
	{
		hsb_critical("init asr daemon error\n");
		return -1;
	}

	ret = MSPLogin(NULL, NULL, lgi_param);
	if (MSP_SUCCESS != ret) {
		printf("MSPLogin failed, ret=%d\n", ret);
		return -1;
	}

	struct ivw_rec ivwr;
	struct ivw_notifier notify = {
		on_ivw_result,
	};

	ret = ivw_init(&ivwr, ssb_param, &notify);
	if (0 != ret) {
		printf("ivw init fail: %d\n", ret);
		return -2;
	}

	daemon_listen_data dla;
	while (1) {
		struct timeval tv = { 1, 0 };
again:
		daemon_select(hsb_awaken_daemon_config.unix_listen_fd, &tv, &dla);
		if (dla.recv_time != 0) {
			deal_awaken_cmd(&dla, &ivwr);

			goto again;
		}
	}

	ivw_stop_listening(&ivwr);
	ivw_uninit(&ivwr);
	MSPLogout();
	return 0;
}


