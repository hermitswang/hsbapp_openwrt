
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "msp_cmn.h"
#include "qivw.h"
#include "msp_errors.h"

#include "linuxrec.h"
#include "ivw.h"


#define IVW_DBGON 1
#if IVW_DBGON == 1
#	define ivw_dbg printf
#else
#	define ivw_dbg
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
	IVW_STATE_INIT,
	IVW_STATE_STARTED
};


#define IVW_MALLOC	malloc
#define IVW_MFREE	free
#define IVW_MEMSET	memset

static int ivw_write_audio_data(struct ivw_rec *ivwr, char *data, unsigned int len);

extern Proc_QIVWSessionBegin _QIVWSessionBegin;
extern Proc_QIVWRegisterNotify _QIVWRegisterNotify;
extern Proc_QIVWSessionEnd _QIVWSessionEnd;
extern Proc_QIVWAudioWrite _QIVWAudioWrite;

static void Sleep(size_t ms)
{
	usleep(ms*1000);
}


static void end_ivw_on_error(struct ivw_rec *ivwr, int errcode)
{
	stop_record(ivwr->recorder);
	
	if (ivwr->session_id) {
		_QIVWSessionEnd(ivwr->session_id, "err");
		ivwr->session_id = NULL;
	}

	ivwr->state = IVW_STATE_INIT;

	if (ivwr->notify.on_result)
		ivwr->notify.on_result(errcode);

	ivw_dbg("end_ivw_on_error\n");
}

static void ivw_cb(char *data, unsigned long len, void *user_para)
{
	int errcode;
	struct ivw_rec *ivwr;

	if(len == 0 || data == NULL)
		return;

	ivwr = (struct ivw_rec *)user_para;

	if(ivwr == NULL)
		return;

	if (ivwr->state < IVW_STATE_STARTED)
		return; /* ignore the data if error happened */
	
	errcode = ivw_write_audio_data(ivwr, data, len);
	if (errcode) {
		end_ivw_on_error(ivwr, errcode);
		return;
	}
}

int ivw_init(
	struct ivw_rec *ivwr,
	const char *session_begin_params, 
	struct ivw_notifier *notify)
{
	int errcode;
	record_dev_id devid = get_default_input_dev();
	WAVEFORMATEX wavfmt = DEFAULT_FORMAT;

	if (get_input_dev_num() == 0) {
		return -E_IVW_NOACTIVEDEVICE;
	}

	if (!ivwr || !session_begin_params)
		return -E_IVW_INVAL;

	IVW_MEMSET(ivwr, 0, sizeof(*ivwr));
	ivwr->state = IVW_STATE_INIT;
	ivwr->audio_status = MSP_AUDIO_SAMPLE_FIRST;

	snprintf(ivwr->session_params, sizeof(ivwr->session_params), "%s", session_begin_params);

	ivwr->notify = *notify;
	
	errcode = create_recorder(&ivwr->recorder, ivw_cb, (void*)ivwr);
	if (ivwr->recorder == NULL || errcode != 0) {
		ivw_dbg("create recorder failed: %d\n", errcode);
		errcode = -E_IVW_RECORDFAIL;
		goto fail;
	}

	errcode = open_recorder(ivwr->recorder, devid, &wavfmt);
	if (errcode != 0) {
		ivw_dbg("recorder open failed: %d\n", errcode);
		errcode = -E_IVW_RECORDFAIL;
		goto fail;
	}

	return 0;

fail:
	if (ivwr->recorder) {
		destroy_recorder(ivwr->recorder);
		ivwr->recorder = NULL;
	}

	IVW_MEMSET(&ivwr->notify, 0, sizeof(ivwr->notify));

	return errcode;
}

int cb_ivw_msg_proc( const char *sessionID, int msg, int param1, int param2, const void *info, void *userData )
{
	struct ivw_rec *ivwr = (struct ivw_rec *)userData;

	if (MSP_IVW_MSG_ERROR == msg)
	{
		printf("\n\nMSP_IVW_MSG_ERROR errCode = %d\n\n", param1);
		if (ivwr->notify.on_result)
			ivwr->notify.on_result(param1);
	}
	else if (MSP_IVW_MSG_WAKEUP == msg)
	{
		printf("\n\nMSP_IVW_MSG_WAKEUP result = %s\n\n", (char *)info);
		if (ivwr->notify.on_result)
			ivwr->notify.on_result(0);
	}

	return 0;
}

int ivw_start_listening(struct ivw_rec *ivwr)
{
	int ret;
	const char *session_id = NULL;
	int errcode = MSP_SUCCESS;
	char sse_hints[256];

	if (ivwr->state >= IVW_STATE_STARTED) {
		ivw_dbg("already STARTED.\n");
		return -E_IVW_ALREADY;
	}

	session_id = _QIVWSessionBegin(NULL, ivwr->session_params, &errcode);
	if (MSP_SUCCESS != errcode)
	{
		ivw_dbg("\nQISRSessionBegin failed! error code:%d\n", errcode);
		return errcode;
	}

	errcode = _QIVWRegisterNotify(session_id, cb_ivw_msg_proc, (void *)ivwr);
	if (errcode != MSP_SUCCESS)
        {
                snprintf(sse_hints, sizeof(sse_hints), "QIVWRegisterNotify errorCode=%d", errcode);
                printf("QIVWRegisterNotify failed! error code:%d\n", errcode);
		_QIVWSessionEnd(session_id, sse_hints);
		ivwr->session_id = NULL;
		return errcode;
        }

	ivwr->session_id = session_id;
	ivwr->audio_status = MSP_AUDIO_SAMPLE_FIRST;

	ret = start_record(ivwr->recorder);
	if (ret != 0) {
		ivw_dbg("start record failed: %d\n", ret);
                snprintf(sse_hints, sizeof(sse_hints), "start_record errorCode=%d", ret);
		_QIVWSessionEnd(session_id, sse_hints);
		ivwr->session_id = NULL;
		return -E_IVW_RECORDFAIL;
	}

	ivwr->state = IVW_STATE_STARTED;

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

int ivw_stop_listening(struct ivw_rec *ivwr)
{
	int ret = 0;

	if (ivwr->state < IVW_STATE_STARTED) {
		ivw_dbg("Not started or already stopped.\n");
		return 0;
	}

	ret = stop_record(ivwr->recorder);
	if (ret != 0) {
		ivw_dbg("Stop failed! \n");
		return -E_IVW_RECORDFAIL;
	}
	wait_for_rec_stop(ivwr->recorder, (unsigned int)-1);

	ivwr->state = IVW_STATE_INIT;

	ret = _QIVWAudioWrite(ivwr->session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST);
	if (ret != 0) {
		ivw_dbg("write LAST_SAMPLE failed: %d\n", ret);
		_QIVWSessionEnd(ivwr->session_id, "QIVWAudioWrite fail");
		return ret;
	}

	_QIVWSessionEnd(ivwr->session_id, "stop ok");
	ivwr->session_id = NULL;
	return 0;
}

static int ivw_write_audio_data(struct ivw_rec *ivwr, char *data, unsigned int len)
{
	int ret = 0;
	if (!ivwr )
		return -E_IVW_INVAL;

	if (!data || !len)
		return 0;

	ret = _QIVWAudioWrite(ivwr->session_id, data, len, ivwr->audio_status);
	if (ret) {
		end_ivw_on_error(ivwr, ret);
		return ret;
	}

	ivwr->audio_status = MSP_AUDIO_SAMPLE_CONTINUE;

	return 0;
}

void ivw_uninit(struct ivw_rec *ivwr)
{
	if (ivwr->recorder) {
		if(!is_record_stopped(ivwr->recorder))
			stop_record(ivwr->recorder);
		close_recorder(ivwr->recorder);
		destroy_recorder(ivwr->recorder);
		ivwr->recorder = NULL;
	}
}

