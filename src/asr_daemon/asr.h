

#ifndef _ASR_H_
#define _ASR_H_

#define E_ASR_NOACTIVEDEVICE		1
#define E_ASR_NOMEM			2
#define E_ASR_INVAL			3
#define E_ASR_RECORDFAIL		4
#define E_ASR_ALREADY			5


struct asr_notifier {
	void (*on_result)(int errcode, const char *result);
};

struct asr_rec {
	struct asr_notifier notify;
	const char *session_id;
	const char *grammar;
	int audio_status;
	struct recorder *recorder;
	volatile int state;
	char session_params[256];
};


#ifdef __cplusplus
extern "C" {
#endif

int asr_init(
	struct asr_rec *asrr,
	const char *session_begin_params, 
	const char *grammar_id,
	struct asr_notifier *notify);

int asr_start_listening(struct asr_rec *asrr);

int asr_stop_listening(struct asr_rec *asrr);

void asr_uninit(struct asr_rec *asrr);

#ifdef __cplusplus
} /* extern "C" */	
#endif /* C++ */

#endif /* _ASR_H_ */

