
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include "unix_socket.h"
#include "daemon_control.h"
#include "hsb_config.h"
#include "debug.h"
#include "utils.h"

#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"

#include "asr.h"

#define	BUFFER_SIZE 2048
#define HINTS_SIZE  100
#define GRAMID_LEN	128
#define FRAME_LEN	640 

static void on_asr_error(int errcode);
static void on_asr_result(const char *result);

static struct asr_rec asrr = { 0 };
static struct asr_notifier notify = {
	on_asr_error,
	on_asr_result,
};

static bool working = false;
static const char *ssb_param = "sub = asr, result_type = plain, result_encoding = utf8";
static const char *login_params = "appid = 57e4884c, work_dir = /tmp/hsb";

static char grammar_id[GRAMID_LEN];

int upload_grammar(const char *file)
{
	FILE *fp = NULL;
	char *grammar = NULL;
	unsigned int grammar_len = 0;
	unsigned int read_len = 0;
	const char *ret_id = NULL;
	unsigned int ret_id_len = 0;
	int ret = -1;	

	fp = fopen(file, "rb");
	if (!fp) {
		hsb_warning("open grammar file %s failed!\n", file);
		goto grammar_exit;
	}
	
	fseek(fp, 0, SEEK_END);
	grammar_len = ftell(fp);
	fseek(fp, 0, SEEK_SET); 

	grammar = (char*)malloc(grammar_len + 1);
	if (!grammar) {
		hsb_warning("out of memory!\n");
		goto grammar_exit;
	}

	read_len = fread((void *)grammar, 1, grammar_len, fp);
	if (read_len != grammar_len)
	{
		hsb_warning("read grammar error!\n");
		goto grammar_exit;
	}
	grammar[grammar_len] = '\0';

	ret_id = MSPUploadData("usergram", grammar, grammar_len, "dtt = abnf, sub = asr", &ret);
	if (MSP_SUCCESS != ret)
	{
		printf("\nMSPUploadData failed, error code: %d.\n", ret);
		goto grammar_exit;
	}

	ret_id_len = strlen(ret_id);
	strncpy(grammar_id, ret_id, ret_id_len);
	hsb_debug("grammar_id: %s\n", grammar_id);

	ret = 0;

grammar_exit:
	if (NULL!= grammar)
		free(grammar);

	if (NULL != fp)
		fclose(fp);

	return ret;
}

#if 0

void run_asr(const char* audio_file, const char* params, char* grammar_id)
{
	const char*		session_id						= NULL;
	char			rec_result[BUFFER_SIZE]		 	= {'\0'};	
	char			hints[HINTS_SIZE]				= {'\0'}; //hints为结束本次会话的原因描述，由用户自定义
	unsigned int	total_len						= 0;
	int 			aud_stat 						= MSP_AUDIO_SAMPLE_CONTINUE;		//音频状态
	int 			ep_stat 						= MSP_EP_LOOKING_FOR_SPEECH;		//端点检测
	int 			rec_stat 						= MSP_REC_STATUS_SUCCESS;			//识别状态	
	int 			errcode 						= MSP_SUCCESS;

	FILE*			f_pcm 							= NULL;
	char*			p_pcm 							= NULL;
	long 			pcm_count 						= 0;
	long 			pcm_size 						= 0;
	long			read_size						= 0;

	if (NULL == audio_file)
		goto asr_exit;

	f_pcm = fopen(audio_file, "rb");
	if (NULL == f_pcm) 
	{
		printf("\nopen [%s] failed!\n", audio_file);
		goto asr_exit;
	}
	
	fseek(f_pcm, 0, SEEK_END);
	pcm_size = ftell(f_pcm); //获取音频文件大小 
	fseek(f_pcm, 0, SEEK_SET);		

	p_pcm = (char*)malloc(pcm_size);
	if (NULL == p_pcm)
	{
		printf("\nout of memory!\n");
		goto asr_exit;
	}

	read_size = fread((void *)p_pcm, 1, pcm_size, f_pcm); //读取音频文件内容
	if (read_size != pcm_size)
	{
		printf("\nread [%s] failed!\n", audio_file);
		goto asr_exit;
	}
	
	printf("\n开始语音识别 ...\n");
	session_id = QISRSessionBegin(grammar_id, params, &errcode);
	if (MSP_SUCCESS != errcode)
	{
		printf("\nQISRSessionBegin failed, error code:%d\n", errcode);
		goto asr_exit;
	}
	
	while (1) 
	{
		unsigned int len = 10 * FRAME_LEN; // 每次写入200ms音频(16k，16bit)：1帧音频20ms，10帧=200ms。16k采样率的16位音频，一帧的大小为640Byte
		int ret = 0;

		if (pcm_size < 2 * len) 
			len = pcm_size;
		if (len <= 0)
			break;
		
		aud_stat = MSP_AUDIO_SAMPLE_CONTINUE;
		if (0 == pcm_count)
			aud_stat = MSP_AUDIO_SAMPLE_FIRST;
		
		printf(">");
		ret = QISRAudioWrite(session_id, (const void *)&p_pcm[pcm_count], len, aud_stat, &ep_stat, &rec_stat);
		if (MSP_SUCCESS != ret)
		{
			printf("\nQISRAudioWrite failed, error code:%d\n",ret);
			goto asr_exit;
		}
			
		pcm_count += (long)len;
		pcm_size  -= (long)len;
		
		if (MSP_EP_AFTER_SPEECH == ep_stat)
			break;
		usleep(200*1000); //模拟人说话时间间隙，10帧的音频长度为200ms
	}
	errcode = QISRAudioWrite(session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_stat, &rec_stat);
	if (MSP_SUCCESS != errcode)
	{
		printf("\nQISRAudioWrite failed, error code:%d\n",errcode);
		goto asr_exit;	
	}

	while (MSP_REC_STATUS_COMPLETE != rec_stat) 
	{
		const char *rslt = QISRGetResult(session_id, &rec_stat, 0, &errcode);
		if (MSP_SUCCESS != errcode)
		{
			printf("\nQISRGetResult failed, error code: %d\n", errcode);
			goto asr_exit;
		}
		if (NULL != rslt)
		{
			unsigned int rslt_len = strlen(rslt);
			total_len += rslt_len;
			if (total_len >= BUFFER_SIZE)
			{
				printf("\nno enough buffer for rec_result !\n");
				goto asr_exit;
			}
			strncat(rec_result, rslt, rslt_len);
		}
		usleep(150*1000); //防止频繁占用CPU
	}
	printf("\n语音识别结束\n");
	printf("=============================================================\n");
	printf("%s",rec_result);
	printf("=============================================================\n");

asr_exit:
	if (NULL != f_pcm)
	{
		fclose(f_pcm);
		f_pcm = NULL;
	}
	if (NULL != p_pcm)
	{	
		free(p_pcm);
		p_pcm = NULL;
	}

	QISRSessionEnd(session_id, hints);
}

#endif

static void _stop(void)
{
	char *send_str = "stop";
	int fd = unix_socket_new();
	if (fd < 0)
		return;

	char path[MAXPATH];
	snprintf(path, sizeof(path), LINUX_WORK_DIR"%s", hsb_asr_daemon_config.unix_listen_path);

	unix_socket_send_to(fd , path,
				send_str , strlen(send_str) );
	unix_socket_free(fd);
}

static void on_asr_error(int errcode)
{
	printf("asr error: %d\n", errcode);

	if (working) {
		asr_stop_listening(&asrr);
		asr_uninit(&asrr);
		working = false;
	}
}

static void on_asr_result(const char *result)
{
	printf("asr result: %s\n", result);
	if (working) {
		_stop();
	}
}

static int deal_asr_cmd(daemon_listen_data *dla)
{
	int ret;
	char *buf = dla->cmd_buf;

	if (0 == check_cmd_prefix(buf, "start")) {
		if (working)
			return 0;

		ret = asr_init(&asrr, ssb_param, &notify);
		if (0 != ret) {
			printf("asr init fail: %d\n", ret);
			return -1;
		}

		ret = asr_start_listening(&asrr, grammar_id);
		if (0 != ret) {
			printf("asr_start_listening fail: %d\n", ret);
			asr_stop_listening(&asrr);
			asr_uninit(&asrr);
			return -2;
		}

		working = true;
                printf("started\n");
	} else if (0 == check_cmd_prefix(buf, "stop")) {
		printf("stopping\n");
		if (!working)
			return 0;

		asr_stop_listening(&asrr);
		asr_uninit(&asrr);

		working = false;
		printf("stopped\n");
	} else if (0 == check_cmd_prefix(buf, "set_grammar=")) {
		char *file = buf + strlen("set_grammar=");

		ret = upload_grammar(file);
		if (ret)
			hsb_warning("upload grammar fail, ret=%d\n", ret);
	} else {
		hsb_warning("asr_daemon get unknown cmd: [%s]\n", buf);
	}

	return 0;
}

int main(int argc, char* argv[])
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
 
	if (!daemon_init(&hsb_asr_daemon_config, background))
	{
		hsb_critical("init asr daemon error\n");
		return -1;
	}
	
	ret = MSPLogin(NULL, NULL, login_params);
	if (MSP_SUCCESS != ret)
	{
		printf("MSPLogin failed, error code: %d.\n",ret);
		return -2;
	}

	/* for test */
	upload_grammar("gm_continuous_digit.abnf");

	daemon_listen_data dla;
	while (1) {
		struct timeval tv = { 1, 0 };
again:
		daemon_select(hsb_asr_daemon_config.unix_listen_fd, &tv, &dla);
		if (dla.recv_time != 0) {
			deal_asr_cmd(&dla);

			goto again;
		}
	}

	MSPLogout();

	return 0;
}


