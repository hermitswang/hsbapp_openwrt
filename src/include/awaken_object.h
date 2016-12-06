

#ifndef _AWAKEN_OBJECT_H_
#define _AWAKEN_OBJECT_H_

#include <glib.h>

#define HSB_CMD_AWAKEN_START_LISTENING		"start"
#define HSB_CMD_AWAKEN_STOP_LISTENING		"stop"

typedef struct {
	gboolean (*start) (void);
	gboolean (*stop) (void);
} hsb_awaken_object;

gboolean hsb_awaken_object_init(hsb_awaken_object *);

#endif
