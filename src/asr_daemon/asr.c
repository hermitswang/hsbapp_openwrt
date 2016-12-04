
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "msp_cmn.h"
#include "qisr.h"
#include "msp_errors.h"

#include "linuxrec.h"
#include "asr.h"

#define ASR_DBGON 1
#if ASR_DBGON == 1
#	define asr_dbg printf
#else
#	define asr_dbg
#endif

#define DEFAULT_FORMAT		\
{\
	WAVE_FORMAT_PCM,	\
	1,			\
	16000,			\
	32000,			\
	2,			\
	16,			\
	sizeof(WAVEFORMATEX)	\
}

/* internal state */
enum {
	ASR_STATE_INIT,
	ASR_STATE_STARTED
};


#define ASR_MALLOC	malloc
#define ASR_MFREE	free
#define ASR_MEMSET	memset

static int asr_write_audio_data(struct asr_rec *asrr, char *data, unsigned int len);

static void Sleep(size_t ms)
{
	usleep(ms*1000);
}

static void end_asr_on_error(struct asr_rec *asrr, int errcode)
{
	stop_record(asrr->recorder);

	const char *errstr;

	if (asrr->session_id) {
		QISRSessionEnd(asrr->session_id, "err");
			
		asrr->session_id = NULL;
	}

	asrr->state = ASR_STATE_INIT;

	if (asrr->notify.on_error)
		asrr->notify.on_error(errcode);

	asr_dbg("end_asr_on_error\n");
}

static void asr_cb(char *data, unsigned long len, void *user_para)
{
	struct asr_rec *asrr;

	if(len == 0 || data == NULL)
		return;

	asrr = (struct asr_rec *)user_para;

	if(asrr == NULL)
		return;

	if (asrr->state < ASR_STATE_STARTED)
		return; /* ignore the data if error happened */
	
	asr_write_audio_data(asrr, data, len);
}

int asr_init(
	struct asr_rec *asrr,
	const char *session_begin_params,
	const char *grammar_id,
	struct asr_notifier *notify)
{
	int errcode;
	record_dev_id devid = get_default_input_dev();
	WAVEFORMATEX wavfmt = DEFAULT_FORMAT;

	if (get_input_dev_num() == 0) {
		return -E_ASR_NOACTIVEDEVICE;
	}

	if (!asrr || !session_begin_params)
		return -E_ASR_INVAL;

	ASR_MEMSET(asrr, 0, sizeof(*asrr));
	asrr->state = ASR_STATE_INIT;
	asrr->audio_status = MSP_AUDIO_SAMPLE_FIRST;

	snprintf(asrr->session_params, sizeof(asrr->session_params), "%s", session_begin_params);

	asrr->grammar = grammar_id;
	asrr->notify = *notify;
	
	errcode = create_recorder(&asrr->recorder, asr_cb, (void*)asrr);
	if (asrr->recorder == NULL || errcode != 0) {
		asr_dbg("create recorder failed: %d\n", errcode);
		errcode = -E_ASR_RECORDFAIL;
		goto fail;
	}

	errcode = open_recorder(asrr->recorder, devid, &wavfmt);
	if (errcode != 0) {
		asr_dbg("recorder open failed: %d\n", errcode);
		errcode = -E_ASR_RECORDFAIL;
		goto fail;
	}

	return 0;

fail:
	if (asrr->recorder) {
		destroy_recorder(asrr->recorder);
		asrr->recorder = NULL;
	}

	ASR_MEMSET(&asrr->notify, 0, sizeof(asrr->notify));

	return errcode;
}

#if 0
int cb_asr_result_proc( const char *sessionID, const char *result, int resultLen, int resultStatus, void *userData)
{
	struct asr_rec *asrr = (struct asr_rec *)userData;

	return 0;
}

int cb_asr_status_proc(const char *sessionID, int type, int status, int param1, const void *param2, void *userData)
{

	return 0;
}

int cb_asr_error_proc(const char *sessionID, int errorCode, const char *detail, void *userData)
{

	return 0;
}
#endif

int asr_start_listening(struct asr_rec *asrr)
{
	int ret;
	const char *session_id = NULL;
	int errcode = MSP_SUCCESS;
	char sse_hints[256];

	if (asrr->state >= ASR_STATE_STARTED) {
		asr_dbg("already STARTED.\n");
		return -E_ASR_ALREADY;
	}

	session_id = QISRSessionBegin(asrr->grammar, asrr->session_params, &errcode);
	if (MSP_SUCCESS != errcode)
	{
		asr_dbg("\nQISRSessionBegin failed! error code:%d\n", errcode);
		return errcode;
	}

#if 0
	errcode = QISRRegisterNotify(session_id, cb_asr_result_proc, cb_asr_status_proc, cb_asr_error_proc, (void *)asrr);
	if (errcode != MSP_SUCCESS)
        {
                snprintf(sse_hints, sizeof(sse_hints), "QISRRegisterNotify errorCode=%d", errcode);
                printf("QISRRegisterNotify failed! error code:%d\n", errcode);
		QISRSessionEnd(session_id, sse_hints);
		asrr->session_id = NULL;
		return errcode;
        }
#endif

	asrr->session_id = session_id;
	asrr->audio_status = MSP_AUDIO_SAMPLE_FIRST;

	ret = start_record(asrr->recorder);
	if (ret != 0) {
		asr_dbg("start record failed: %d\n", ret);
                snprintf(sse_hints, sizeof(sse_hints), "start_record errorCode=%d", ret);
		QISRSessionEnd(session_id, sse_hints);
		asrr->session_id = NULL;
		return -E_ASR_RECORDFAIL;
	}

	asrr->state = ASR_STATE_STARTED;

	return 0;
}

/* after stop_record, there are still some data callbacks */
static void wait_for_rec_stop(struct recorder *rec, unsigned int timeout_ms)
{
	while (!is_record_stopped(rec)) {
		Sleep(1);
		if (timeout_ms != (unsigned int)-1)
			if (0 == timeout_ms--)
				break;
	}
}

int asr_stop_listening(struct asr_rec *asrr)
{
	int ret = 0;
	int ep_status, rec_status;

	if (asrr->state < ASR_STATE_STARTED) {
		asr_dbg("Not started or already stopped.\n");
		return 0;
	}

	ret = stop_record(asrr->recorder);
	if (ret != 0) {
		asr_dbg("Stop failed! \n");
		return -E_ASR_RECORDFAIL;
	}
	wait_for_rec_stop(asrr->recorder, (unsigned int)-1);

	asrr->state = ASR_STATE_INIT;

	ret = QISRAudioWrite(asrr->session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_status, &rec_status);
	if (ret != 0) {
		asr_dbg("write LAST_SAMPLE failed: %d\n", ret);
		QISRSessionEnd(asrr->session_id, "QISRAudioWrite fail");
		return ret;
	}

	QISRSessionEnd(asrr->session_id, "stop ok");
	asrr->session_id = NULL;
	return 0;
}

static int asr_write_audio_data(struct asr_rec *asrr, char *data, unsigned int len)
{
	int ret = 0;
	int ep_stat = MSP_EP_LOOKING_FOR_SPEECH;
	int rec_stat = MSP_REC_STATUS_SUCCESS;
	if (!asrr )
		return -E_ASR_INVAL;

	if (!data || !len)
		return 0;

	ret = QISRAudioWrite(asrr->session_id, data, len, asrr->audio_status, &ep_stat, &rec_stat);
	if (ret) {
		end_asr_on_error(asrr, ret);

		return ret;
	}

	if (MSP_EP_AFTER_SPEECH == ep_stat) {
		printf("after speech\n");

		char rec_result[256];

		memset(rec_result, 0, sizeof(rec_result));

		ret = QISRAudioWrite(asrr->session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_stat, &rec_stat);

		if (ret) {
			printf("QISRAudioWrite failed, err=%d\n", ret);
			end_asr_on_error(asrr, ret);
		}

		while (MSP_REC_STATUS_COMPLETE != rec_stat) {
			int errcode, total_len = 0;
			const char *rslt = QISRGetResult(asrr->session_id, &rec_stat, 0, &errcode);
	                if (MSP_SUCCESS != errcode)
			{
				printf("\nQISRGetResult failed, error code: %d\n", errcode);
				end_asr_on_error(asrr, errcode);
				return errcode;
			}

			if (NULL != rslt)
			{
				unsigned int rslt_len = strlen(rslt);
				total_len += rslt_len;
				if (total_len >= 256)
				{
					printf("\nno enough buffer for rec_result !\n");
					end_asr_on_error(asrr, errcode);
					return -1;
				}
				strncat(rec_result, rslt, rslt_len);
			}
			usleep(150*1000); //防止频繁占用CPU
		}

		printf("result: %s\n", rec_result);
		if (asrr->notify.on_result)
			asrr->notify.on_result(rec_result);

		return 0;
	}

	asrr->audio_status = MSP_AUDIO_SAMPLE_CONTINUE;

	return 0;
}

void asr_uninit(struct asr_rec *asrr)
{
	if (asrr->recorder) {
		if(!is_record_stopped(asrr->recorder))
			stop_record(asrr->recorder);
		close_recorder(asrr->recorder);
		destroy_recorder(asrr->recorder);
		asrr->recorder = NULL;
	}
}

