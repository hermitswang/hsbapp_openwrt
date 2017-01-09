

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

#include <dlfcn.h>

#include "msp_cmn.h"
#include "qivw.h"
#include "msp_errors.h"

#include "ivw.h"

static const char *lgi_param = "appid = 587333fc,engine_start = ivw,ivw_res_path =fo|res/ivw/wakeupresource.jet, work_dir = /tmp/hsb/ivw";
static const char *ssb_param = "ivw_threshold=0:-20,sst=wakeup";
static bool working = false;
static struct ivw_rec ivwr;
static void on_ivw_result(int errcode);
static struct ivw_notifier notify = {
	on_ivw_result,
};

#define FEEDBACK_PATH	WORK_DIR"hsb_audio.listen"

static int feedback(int errcode)
{
	int fd = unix_socket_new();
	if (fd < 0)
		return -1;

	char buf[MAXPATH];
	snprintf(buf, sizeof(buf), "awaken result=%d", errcode);

	int ret = unix_socket_send_to(fd, FEEDBACK_PATH, buf, strlen(buf));

	unix_socket_free(fd);
	return 0;
}

static void on_ivw_result(int errcode)
{
	printf("ivw result: %d\n", errcode);
	feedback(errcode);

	if (working) {
		ivw_stop_listening(&ivwr);
		ivw_uninit(&ivwr);
		working = false;
	}
}

static int deal_awaken_cmd(daemon_listen_data *dla)
{
	int ret;
	char *buf = dla->cmd_buf;

	if (0 == check_cmd_prefix(buf, "start")) {
		if (working)
			return 0;

		ret = ivw_init(&ivwr, ssb_param, &notify);
		if (0 != ret) {
			printf("ivw init fail: %d\n", ret);
			return -1;
		}

		ret = ivw_start_listening(&ivwr);
		if (0 != ret) {
			printf("ivw start listening fail: %d\n", ret);
			ivw_stop_listening(&ivwr);
			ivw_uninit(&ivwr);
			return -2;
		}

		working = true;
	} else {
		printf("unknown cmd: [%s]\n", buf);
	}

	return 0;
}

Proc_MSPLogin _MSPLogin;
Proc_MSPLogout _MSPLogout;
Proc_QIVWSessionBegin _QIVWSessionBegin;
Proc_QIVWRegisterNotify _QIVWRegisterNotify;
Proc_QIVWSessionEnd _QIVWSessionEnd;
Proc_QIVWAudioWrite _QIVWAudioWrite;

static int load_lib(void)
{
	void *handle = dlopen(WORK_DIR"libmsc.so", RTLD_NOW);
	if (!handle) {
		hsb_critical("load lib failed\n");
		return -1;
	}

	Proc_MSPLogin login = (Proc_MSPLogin)dlsym(handle, "MSPLogin");
	Proc_MSPLogout logout = (Proc_MSPLogout)dlsym(handle, "MSPLogout");
	if (!login || !logout) {
		hsb_critical("MSPLogin/MSPLogout not found\n");
		return -2;
	}

	Proc_QIVWSessionBegin begin = (Proc_QIVWSessionBegin)dlsym(handle, "QIVWSessionBegin");
	Proc_QIVWSessionEnd end = (Proc_QIVWSessionEnd)dlsym(handle, "QIVWSessionEnd");
	Proc_QIVWRegisterNotify notify = (Proc_QIVWRegisterNotify)dlsym(handle, "QIVWRegisterNotify");
	Proc_QIVWAudioWrite write = (Proc_QIVWAudioWrite)dlsym(handle, "QIVWAudioWrite");

	if (!begin || !end || !notify || !write) {
		hsb_critical("begin/end/notify/write not found\n");
		return -3;
	}

	_MSPLogin = login;
	_MSPLogout = logout;

	_QIVWSessionBegin = begin;
	_QIVWRegisterNotify = notify;
	_QIVWSessionEnd = end;
	_QIVWAudioWrite = write;

	return 0;
}

int main(int argc, char *argv[])
{
	int opt, ret;

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
 
	if (load_lib())
		return -1;

	if (!daemon_init(&hsb_awaken_daemon_config, background))
	{
		hsb_critical("init awaken daemon error\n");
		return -1;
	}

	ret = _MSPLogin(NULL, NULL, lgi_param);
	if (MSP_SUCCESS != ret) {
		printf("MSPLogin failed, ret=%d\n", ret);
		return -1;
	}

	daemon_listen_data dla;
	while (1) {
		struct timeval tv = { 1, 0 };
again:
		daemon_select(hsb_awaken_daemon_config.unix_listen_fd, &tv, &dla);
		if (dla.recv_time != 0) {
			deal_awaken_cmd(&dla);

			goto again;
		}
	}

	ivw_stop_listening(&ivwr);
	ivw_uninit(&ivwr);
	_MSPLogout();
	return 0;
}


