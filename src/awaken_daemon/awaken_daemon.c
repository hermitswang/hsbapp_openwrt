

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "msp_cmn.h"
#include "qivw.h"
#include "msp_errors.h"

#include "ivw.h"

static void on_ivw_result(int errcode)
{
	printf("ivw result: %d\n", errcode);
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

int main(int argc, char* argv[])
{
	int         ret       = MSP_SUCCESS;
	const char *lgi_param = "appid = 57e4884c,engine_start = ivw,ivw_res_path =fo|res/ivw/wakeupresource.jet, work_dir = ."; //使用唤醒需要在此设置engine_start = ivw,ivw_res_path =fo|xxx/xx 启动唤醒引擎
	const char *ssb_param = "ivw_threshold=0:-20,sst=wakeup";

	ret = MSPLogin(NULL, NULL, lgi_param);
	if (MSP_SUCCESS != ret)
	{
		printf("MSPLogin failed, error code: %d.\n", ret);
		goto exit ;//登录失败，退出登录
	}

	demo_ivw(ssb_param);
exit:
	printf("按任意键退出 ...\n");
	getchar();
	MSPLogout(); //退出登录
	return 0;
}


