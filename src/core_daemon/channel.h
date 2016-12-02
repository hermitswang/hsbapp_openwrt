
#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <glib.h>
#include <stdint.h>
#include "hsb_config.h"

typedef struct {
	char		name[HSB_CHANNEL_MAX_NAME_LEN];
	uint32_t	id;
} HSB_CHANNEL_T;

typedef struct {
	GQueue	queue;
	GMutex	mutex;
} HSB_CHANNEL_DB_T;

HSB_CHANNEL_DB_T *alloc_channel_db(void);
int release_channel_db(HSB_CHANNEL_DB_T *pdb);
int set_channel(HSB_CHANNEL_DB_T *db, char *name, uint32_t cid);
int del_channel(HSB_CHANNEL_DB_T *db, char *name);
int get_channel(HSB_CHANNEL_DB_T *db, char *name, uint32_t *cid);

int get_channel_num(HSB_CHANNEL_DB_T *db, int *num);
int get_channel_by_id(HSB_CHANNEL_DB_T *db, int id, char *name, uint32_t *cid);

#endif /* _CHANNEL_H_ */
