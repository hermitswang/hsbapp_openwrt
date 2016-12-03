

#ifndef _IVW_H_
#define _IVW_H_

#define E_IVW_NOACTIVEDEVICE		1
#define E_IVW_NOMEM			2
#define E_IVW_INVAL			3
#define E_IVW_RECORDFAIL		4
#define E_IVW_ALREADY			5


struct ivw_notifier {
	void (*on_result)(int errcode);
};

struct ivw_rec {
	struct ivw_notifier notify;
	const char *session_id;
	int audio_status;
	struct recorder *recorder;
	volatile int state;
	char session_params[256];
};


#ifdef __cplusplus
extern "C" {
#endif

int ivw_init(
	struct ivw_rec *ivwr,
	const char *session_begin_params, 
	struct ivw_notifier *notify);

int ivw_start_listening(struct ivw_rec *ivwr);

int ivw_stop_listening(struct ivw_rec *ivwr);

void ivw_uninit(struct ivw_rec *ivwr);

#ifdef __cplusplus
} /* extern "C" */	
#endif /* C++ */

#endif /* _IVW_H_ */

