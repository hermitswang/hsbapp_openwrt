
#include <string.h>
#include "channel.h"
#include "hsb_error.h"
#include "debug.h"

HSB_CHANNEL_DB_T *alloc_channel_db(void)
{
	HSB_CHANNEL_DB_T *pdb = g_slice_new0(HSB_CHANNEL_DB_T);
	if (!pdb)
		return NULL;
	
	g_queue_init(&pdb->queue);
	g_mutex_init(&pdb->mutex);

	return pdb;
}

int release_channel_db(HSB_CHANNEL_DB_T *pdb)
{
	g_slice_free(HSB_CHANNEL_DB_T, pdb);
	return HSB_E_OK;
}

int set_channel(HSB_CHANNEL_DB_T *db, char *name, uint32_t cid)
{
	GQueue *queue = &db->queue;
	uint32_t len = g_queue_get_length(queue);
	int id;
	HSB_CHANNEL_T *pchan;

	for (id = 0; id < len; id++) {
		pchan = (HSB_CHANNEL_T *)g_queue_peek_nth(queue, id);
		if (!pchan) {
			hsb_critical("channel null\n");
			continue;
		}

		if (0 == strncmp(pchan->name, name, sizeof(pchan->name)))
			break;
	}

	if (id < len) {
		pchan->id = cid;
		return HSB_E_OK;
	}

	pchan = g_slice_new0(HSB_CHANNEL_T);
	if (!pchan)
		return HSB_E_NO_MEMORY;

	strncpy(pchan->name, name, sizeof(pchan->name));
	pchan->id = cid;

	g_queue_push_tail(queue, pchan);

	return HSB_E_OK;
}

int del_channel(HSB_CHANNEL_DB_T *db, char *name)
{
	GQueue *queue = &db->queue;
	uint32_t len = g_queue_get_length(queue);
	int id;
	HSB_CHANNEL_T *pchan;

	for (id = 0; id < len; id++) {
		pchan = (HSB_CHANNEL_T *)g_queue_peek_nth(queue, id);
		if (!pchan) {
			hsb_critical("channel null\n");
			continue;
		}

		if (0 == strncmp(pchan->name, name, sizeof(pchan->name)))
		{
			g_queue_pop_nth(queue, id);
			g_slice_free(HSB_CHANNEL_T, pchan);
			return HSB_E_OK;
		}
	}

	return HSB_E_OTHERS;
}

int get_channel(HSB_CHANNEL_DB_T *db, char *name, uint32_t *cid)
{
	GQueue *queue = &db->queue;
	uint32_t len = g_queue_get_length(queue);
	int id;
	HSB_CHANNEL_T *pchan;

	for (id = 0; id < len; id++) {
		pchan = (HSB_CHANNEL_T *)g_queue_peek_nth(queue, id);
		if (!pchan) {
			hsb_critical("channel null\n");
			continue;
		}

		if (0 == strncmp(pchan->name, name, sizeof(pchan->name)))
		{
			*cid = pchan->id;
			return HSB_E_OK;
		}
	}

	return HSB_E_OTHERS;
}

int get_channel_num(HSB_CHANNEL_DB_T *db, int *num)
{
	GQueue *queue = &db->queue;

	*num = g_queue_get_length(queue);

	return HSB_E_OK;
}

int get_channel_by_id(HSB_CHANNEL_DB_T *db, int id, char *name, uint32_t *cid)
{
	GQueue *queue = &db->queue;
	uint32_t len = g_queue_get_length(queue);

	if (id >= len)
		return HSB_E_BAD_PARAM;

	HSB_CHANNEL_T *pchan = (HSB_CHANNEL_T *)g_queue_peek_nth(queue, id);
	memcpy(name, pchan->name, sizeof(pchan->name));
	*cid = pchan->id;

	return HSB_E_OK;
}

