
#include <string.h>
#include "hsb_error.h"
#include "remote_ctl.h"
#include "debug.h"

typedef struct {
	HSB_REMOTE_KEY_T	key;
	uint8_t			data[4];
} HSB_KEY_MAP_T;

HSB_KEY_MAP_T cochip_n9201_key_map[] = {
	{ HSB_REMOTE_KEY_0, { 0x30 } },
	{ HSB_REMOTE_KEY_1, { 0x31 } },
	{ HSB_REMOTE_KEY_2, { 0x32 } },
	{ HSB_REMOTE_KEY_3, { 0x33 } },
	{ HSB_REMOTE_KEY_4, { 0x34 } },
	{ HSB_REMOTE_KEY_5, { 0x35 } },
	{ HSB_REMOTE_KEY_6, { 0x36 } },
	{ HSB_REMOTE_KEY_7, { 0x37 } },
	{ HSB_REMOTE_KEY_8, { 0x38 } },
	{ HSB_REMOTE_KEY_9, { 0x39 } },
	{ HSB_REMOTE_KEY_OK, { 0x43 } },
	{ HSB_REMOTE_KEY_BACK, { 0x42 } },
	{ HSB_REMOTE_KEY_INVALID, { 0 } },
};

typedef struct {
	uint16_t		id;
	int			data_len;
	HSB_KEY_MAP_T		*key_map;
} HSB_REMOTE_DRIVER_T;

static HSB_REMOTE_DRIVER_T _remote_drv[] = {
	{ 0, 1, cochip_n9201_key_map },
	{ 1, 2, NULL },
	{ 2, 3, NULL },
};

#define MAX_REMOTE_DRIVER_NUM	(sizeof(_remote_drv) / sizeof(_remote_drv[0]))

int remote_key_mapping(uint16_t drvid, HSB_REMOTE_KEY_T key, HSB_ACTION_T *act)
{
	if (drvid >= MAX_REMOTE_DRIVER_NUM) {
		hsb_critical("unknown remote driver id\n");
		return HSB_E_BAD_PARAM;
	}

	HSB_REMOTE_DRIVER_T *pdrv = &_remote_drv[drvid];
	int data_len = pdrv->data_len;
	HSB_KEY_MAP_T *key_map = pdrv->key_map;
	int key_id = 0;

	if (!key_map) {
		act->id = HSB_ACT_TYPE_REMOTE_CONTROL;
		act->param1 = drvid;
		act->param2 = (uint32_t)key;
		return HSB_E_OK;
	}

	while (key_map[key_id].key != HSB_REMOTE_KEY_INVALID) 
	{
		//printf("%d %d\n", key_map[key_id].key, key);
		if (key_map[key_id].key == key) {
			memcpy(&act->param2, key_map[key_id].data, data_len);
			act->param1 = drvid;
			act->id = HSB_ACT_TYPE_REMOTE_CONTROL;
			return HSB_E_OK;
		}

		key_id ++;
	}

	hsb_critical("remote key [%d] map failed\n", key);
	return HSB_E_OTHERS;
}


