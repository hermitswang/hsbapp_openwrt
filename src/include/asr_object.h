

#ifndef _ASR_OBJECT_H_
#define _ASR_OBJECT_H_

#include <glib.h>

#define HSB_CMD_ASR_START_LISTENING		"start"
#define HSB_CMD_ASR_STOP_LISTENING		"stop"
#define HSB_CMD_ASR_SET_GRAMMAR			"set_grammar="

typedef struct {
	gboolean (*start) (void);
	gboolean (*stop) (void);
	gboolean (*set_grammar)(const char *grammar);
} hsb_asr_object;

gboolean hsb_asr_object_init(hsb_asr_object *);

#endif
