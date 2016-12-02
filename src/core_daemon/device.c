
#include <glib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "device.h"
#include "debug.h"
#include "hsb_error.h"
#include "hsb_config.h"
#include "thread_utils.h"
#include "scene.h"
#include "utils.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
	GQueue			queue;
	GMutex			mutex;

	GQueue			offq;

	GQueue			driverq;
	uint32_t		dev_id;

	HSB_WORK_MODE_T		work_mode;
	uint32_t		sec_today;

	thread_data_control	async_thread_ctl;
} HSB_DEVICE_CB_T;

static HSB_DEVICE_CB_T gl_dev_cb = { 0 };

#define HSB_DEVICE_CB_LOCK()	do { \
	g_mutex_lock(&gl_dev_cb.mutex); \
} while (0)

#define HSB_DEVICE_CB_UNLOCK()	do { \
	g_mutex_unlock(&gl_dev_cb.mutex); \
} while (0)

#define INT_TO_BUF(val, buf)	do { \
	snprintf(buf, sizeof(buf), "%d", val); \
} while (0)

static int _add_node(xmlNodePtr parent, const char *name, char *val)
{
	xmlNodePtr child, value;

	child = xmlNewNode(NULL, BAD_CAST name);
	if (!child)
		return HSB_E_NO_MEMORY;

	value = xmlNewText(BAD_CAST val);
	if (!value) {
		xmlFree(child);
		return HSB_E_NO_MEMORY;
	}

	xmlAddChild(child, value);
	xmlAddChild(parent, child);

	return HSB_E_OK;
}

static xmlNodePtr make_dev_node(HSB_DEV_T *pdev)
{
	char buf[128];
	int id, num;

	xmlNodePtr node = xmlNewNode(NULL, BAD_CAST"device");

	/* set id */
	INT_TO_BUF(pdev->id, buf);
	xmlNewProp(node, BAD_CAST"id", BAD_CAST buf);

	/* set type */
	INT_TO_BUF(pdev->info.dev_type, buf);
	_add_node(node, "type", buf);

	/* set drvid */
	INT_TO_BUF(pdev->drvid, buf);
	_add_node(node, "drvid", buf);

	/* set mac */
	uint8_t *mac = pdev->info.mac;
	mac_to_str(mac, buf);

	_add_node(node, "mac", buf);

	/* set name */
	_add_node(node, "name", pdev->config.name);

	/* set location */
	_add_node(node, "location", pdev->config.location);

	/* set channel */
	HSB_CHANNEL_DB_T *pchan = pdev->pchan_db;
	if (pchan)
	{
		xmlNodePtr channel;
		get_channel_num(pchan, &num);
		for (id = 0; id < num; id++)
		{
			char name[64];
			uint32_t cid;
			get_channel_by_id(pchan, id, name, &cid);

			channel = xmlNewNode(NULL, BAD_CAST"channel");
			_add_node(channel, "cname", name);

			INT_TO_BUF(cid, buf);
			_add_node(channel, "cid", buf);

			xmlAddChild(node, channel);
		}
	}

	/* set timer */
	HSB_TIMER_T *ptimer = NULL;
	HSB_TIMER_STATUS_T *pstatus = NULL;
	xmlNodePtr timer;
	for (id = 0; id < HSB_DEV_MAX_TIMER_NUM; id++)
	{
		ptimer = &pdev->timer[id];
		pstatus = &pdev->timer_status[id];

		if (!pstatus->active)
			continue;

		timer = xmlNewNode(NULL, BAD_CAST"timer");
		
		INT_TO_BUF(id, buf);
		xmlNewProp(timer, BAD_CAST"id", BAD_CAST buf);

		INT_TO_BUF(ptimer->work_mode, buf);
		_add_node(timer, "work_mode", buf);

		INT_TO_BUF(ptimer->flag, buf);
		_add_node(timer, "flag", buf);

		INT_TO_BUF(ptimer->year, buf);
		_add_node(timer, "year", buf);

		INT_TO_BUF(ptimer->mon, buf);
		_add_node(timer, "mon", buf);

		INT_TO_BUF(ptimer->mday, buf);
		_add_node(timer, "mday", buf);

		INT_TO_BUF(ptimer->hour, buf);
		_add_node(timer, "hour", buf);

		INT_TO_BUF(ptimer->min, buf);
		_add_node(timer, "min", buf);

		INT_TO_BUF(ptimer->sec, buf);
		_add_node(timer, "sec", buf);

		INT_TO_BUF(ptimer->wday, buf);
		_add_node(timer, "wday", buf);

		INT_TO_BUF(ptimer->act_id, buf);
		_add_node(timer, "act_id", buf);

		INT_TO_BUF(ptimer->act_param1, buf);
		_add_node(timer, "act_param1", buf);

		INT_TO_BUF(ptimer->act_param2, buf);
		_add_node(timer, "act_param2", buf);

		xmlAddChild(node, timer);
	}

	return node;
}

static xmlNodePtr make_scene_node(HSB_SCENE_T *pscene)
{
	char buf[128];
	int id, num;

	xmlNodePtr node = xmlNewNode(NULL, BAD_CAST"scene");

	INT_TO_BUF(pscene->act_num, buf);
	xmlNewProp(node, BAD_CAST"act_num", BAD_CAST buf);
	xmlNewProp(node, BAD_CAST"name", BAD_CAST pscene->name);

	HSB_SCENE_ACTION_T *paction = NULL;
	num = pscene->act_num;
	for (id = 0; id < num; id++)
	{
		paction = &pscene->actions[id];
		xmlNodePtr action = xmlNewNode(NULL, BAD_CAST"action");

		INT_TO_BUF((paction->has_cond ? 1 : 0), buf);
		xmlNewProp(action, BAD_CAST"has_cond", BAD_CAST buf);

		INT_TO_BUF(paction->delay, buf);
		xmlNewProp(action, BAD_CAST"delay", BAD_CAST buf);

		INT_TO_BUF(paction->act_num, buf);
		xmlNewProp(action, BAD_CAST"act_num", BAD_CAST buf);

		if (paction->has_cond) {
			HSB_SCENE_CONDITION_T *pcond = &paction->condition;
			xmlNodePtr cond = xmlNewNode(NULL, BAD_CAST"condition");
			
			INT_TO_BUF(pcond->devid, buf);
			_add_node(cond, "devid", buf);

			INT_TO_BUF(pcond->id, buf);
			_add_node(cond, "id", buf);

			INT_TO_BUF(pcond->val, buf);
			_add_node(cond, "val", buf);

			INT_TO_BUF(pcond->expr, buf);
			_add_node(cond, "expr", buf);

			xmlAddChild(action, cond);
		}

		int cnt;
		HSB_SCENE_ACT_T *pact = NULL;
		for (cnt = 0; cnt < paction->act_num; cnt++)
		{
			pact = &paction->acts[cnt];
			xmlNodePtr act = xmlNewNode(NULL, BAD_CAST"act");

			
			INT_TO_BUF(pact->flag, buf);
			_add_node(act, "flag", buf);

			INT_TO_BUF(pact->devid, buf);
			_add_node(act, "devid", buf);

			INT_TO_BUF(pact->id, buf);
			_add_node(act, "id", buf);

			INT_TO_BUF(pact->param1, buf);
			_add_node(act, "param1", buf);

			INT_TO_BUF(pact->param2, buf);
			_add_node(act, "param2", buf);

			xmlAddChild(action, act);
		}

		xmlAddChild(node, action);
	}

	return node;
}

int save_config(void)
{
	int ret;
	xmlDocPtr doc;
	xmlNodePtr root, node;

	doc = xmlNewDoc(BAD_CAST"1.0");
	if (!doc)
		return HSB_E_NO_MEMORY;

	root = xmlNewNode(NULL, BAD_CAST"hsb");
	if (!root) {
		xmlFreeDoc(doc);
		return HSB_E_NO_MEMORY;
	}

	xmlDocSetRootElement(doc, root);

	int id, len;
	GQueue *queue;
	HSB_DEV_T *pdev;

	queue = &gl_dev_cb.queue;
	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++)
	{
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		node = make_dev_node(pdev);

		/* add to root */
		xmlAddChild(root, node);
	}

	queue = &gl_dev_cb.offq;
	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++)
	{
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		node = make_dev_node(pdev);

		/* add to root */
		xmlAddChild(root, node);
	}

	/* add scene */
	uint32_t num;
	HSB_SCENE_T *pscene = NULL;
	get_scene_num(&num);
	for (id = 0; id < num; id++)
	{
		ret = get_scene(id, &pscene);
		if (HSB_E_OK != ret)
			continue;

		node = make_scene_node(pscene);

		xmlAddChild(root, node);
	}

	char file[128];
	get_config_file(file);

	xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);

	xmlFreeDoc(doc);

	return HSB_E_OK;
}

static int parse_dev(xmlNodePtr node, HSB_DEV_T **ppdev)
{
	GQueue *devq, *offq;
	HSB_DEV_T *pdev;

	devq = &gl_dev_cb.queue;
	offq = &gl_dev_cb.offq;

	xmlNodePtr cur;
	xmlChar *key;
	uint32_t devid;

	if (!ppdev)
		return HSB_E_BAD_PARAM;

	key = xmlGetProp(node, "id");
	devid = atoi(key);
	xmlFree(key);

	pdev = alloc_dev(devid);
	if (!pdev)
		return HSB_E_NO_MEMORY;

	cur = node->xmlChildrenNode;
	while (cur) {
		key = NULL;
		if (cur->xmlChildrenNode)
			key = xmlNodeGetContent(cur->xmlChildrenNode);

		if (0 == xmlStrcmp(cur->name, BAD_CAST"type")) {
			if (!key) {
				hsb_critical("parse fail: type not found\n");
				goto fail;
			}

			pdev->info.dev_type = atoi(key);
			
		} else if (0 == xmlStrcmp(cur->name, BAD_CAST"drvid")) {
			if (!key) {
				hsb_critical("parse fail: drvid not found\n");
				goto fail;
			}

			pdev->drvid = atoi(key);

		} else if (0 == xmlStrcmp(cur->name, BAD_CAST"mac")) {
			if (!key) {
				hsb_critical("parse fail: mac not found\n");
				goto fail;
			}

			str_to_mac(key, pdev->info.mac);	

		} else if (0 == xmlStrcmp(cur->name, BAD_CAST"name")) {
			if (!key) {
				hsb_critical("parse fail: name not found\n");
				goto fail;
			}

			strncpy(pdev->config.name, key, sizeof(pdev->config.name));
		} else if (0 == xmlStrcmp(cur->name, BAD_CAST"location")) {
			if (!key) {
				hsb_critical("parse fail: location not found\n");
				goto fail;
			}

			strncpy(pdev->config.location, key, sizeof(pdev->config.location));
		} else if (0 == xmlStrcmp(cur->name, BAD_CAST"channel")) {
			xmlNodePtr chan = cur->xmlChildrenNode;
			xmlChar *pbuf;
			uint8_t cname[64];
			uint32_t cid;
			uint8_t flag = 0;

			while (chan) {
				pbuf = xmlNodeGetContent(chan->xmlChildrenNode);

				if (0 == xmlStrcmp(chan->name, BAD_CAST"cname")) {
					strcpy(cname, pbuf);
					flag |= (1 << 0);
				} else if (0 == xmlStrcmp(chan->name, BAD_CAST"cid")) {
					cid = atoi(pbuf);
					flag |= (1 << 1);
				}

				if (pbuf)
					xmlFree(pbuf);

				chan = chan->next;
			}

			if (flag == 0x03) {
				if (!pdev->pchan_db)
					pdev->pchan_db = alloc_channel_db();

				HSB_CHANNEL_DB_T *pchan = pdev->pchan_db;

				set_channel(pchan, cname, cid);
			}
		} else if (0 == xmlStrcmp(cur->name, BAD_CAST"timer")) {
			xmlNodePtr timer = cur->xmlChildrenNode;
			xmlChar *pbuf;
			HSB_TIMER_T *ptimer = NULL;
			HSB_TIMER_STATUS_T *pstatus = NULL;
			int id;

			pbuf = xmlGetProp(cur, "id");
			if (pbuf && (id = atoi(pbuf)) < HSB_DEV_MAX_TIMER_NUM)
			{
				xmlFree(pbuf);
				ptimer = &pdev->timer[id];
				pstatus = &pdev->timer_status[id];

				pstatus->active = true;
				ptimer->id = id;
				while (timer) {
					pbuf = xmlNodeGetContent(timer->xmlChildrenNode);

					if (0 == xmlStrcmp(timer->name, BAD_CAST"work_mode")) {
						ptimer->work_mode = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"flag")) {
						ptimer->flag = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"year")) {
						ptimer->year = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"mon")) {
						ptimer->mon = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"mday")) {
						ptimer->mday = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"hour")) {
						ptimer->hour = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"min")) {
						ptimer->min = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"sec")) {
						ptimer->sec = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"wday")) {
						ptimer->wday = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"act_id")) {
						ptimer->act_id = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"act_param1")) {
						ptimer->act_param1 = atoi(pbuf);
					}else if (0 == xmlStrcmp(timer->name, BAD_CAST"act_param2")) {
						ptimer->act_param2 = atoi(pbuf);
					}

					if (pbuf)
						xmlFree(pbuf);

					timer = timer->next;
				}
			}
		}

		if (key)
			xmlFree(key);

		cur = cur->next;
	}

	*ppdev = pdev;

	hsb_debug("add a device [%d] to offq\n", devid);
	g_queue_push_tail(offq, pdev);

	return HSB_E_OK;
fail:
	if (key)
		xmlFree(key);

	destroy_dev(pdev);

	return HSB_E_OTHERS;
}

static int parse_scene(xmlNodePtr node)
{
	xmlNodePtr cur;
	xmlChar *key;

	HSB_SCENE_T *pscene = alloc_scene();
	if (!pscene)
		return HSB_E_NO_MEMORY;

	key = xmlGetProp(node, "act_num");
	pscene->act_num = atoi(key);
	xmlFree(key);
	
	key = xmlGetProp(node, "name");
	strcpy(pscene->name, key);
	xmlFree(key);
	
	cur = node->xmlChildrenNode;
	int id = 0;
	while (cur) {

		if (0 == xmlStrcmp(cur->name, BAD_CAST"action")) {
			HSB_SCENE_ACTION_T *paction = &pscene->actions[id];

			key = xmlGetProp(cur, "has_cond");
			paction->has_cond = atoi(key) > 0 ? true : false;
			xmlFree(key);

			key = xmlGetProp(cur, "delay");
			paction->delay = atoi(key);
			xmlFree(key);

			key = xmlGetProp(cur, "act_num");
			paction->act_num = atoi(key);
			xmlFree(key);

			xmlNodePtr child = cur->xmlChildrenNode;
			int act_id = 0;
			while (child) {
				if (0 == xmlStrcmp(child->name, BAD_CAST"act")) {
					HSB_SCENE_ACT_T *pact = &paction->acts[act_id];

					xmlNodePtr act = child->xmlChildrenNode;

					while (act) {
						key = xmlNodeGetContent(act->xmlChildrenNode);

						if (0 == xmlStrcmp(act->name, BAD_CAST"flag")) {
							pact->flag = atoi(key);
						} else if (0 == xmlStrcmp(act->name, BAD_CAST"devid")) {
							pact->devid = atoi(key);
						} else if (0 == xmlStrcmp(act->name, BAD_CAST"id")) {
							pact->id = atoi(key);
						} else if (0 == xmlStrcmp(act->name, BAD_CAST"param1")) {
							pact->param1 = atoi(key);
						} else if (0 == xmlStrcmp(act->name, BAD_CAST"param2")) {
							pact->param2 = atoi(key);
						}

						if (key)
							xmlFree(key);

						act = act->next;
					}

					act_id++;
				} else if (0 == xmlStrcmp(child->name, BAD_CAST"condition")) {
					HSB_SCENE_CONDITION_T *pcond = &paction->condition;

					xmlNodePtr cond = child->xmlChildrenNode;

					while (cond) {
						key = xmlNodeGetContent(cond->xmlChildrenNode);

						if (0 == xmlStrcmp(cond->name, BAD_CAST"devid")) {
							pcond->devid = atoi(key);
						} else if (0 == xmlStrcmp(cond->name, BAD_CAST"id")) {
							pcond->id = atoi(key); 
						} else if (0 == xmlStrcmp(cond->name, BAD_CAST"val")) {
							pcond->val = atoi(key); 
						} else if (0 == xmlStrcmp(cond->name, BAD_CAST"expr")) {
							pcond->expr = atoi(key); 
						}

						if (key)
							xmlFree(key);

						cond = cond->next;
					}
				}

				child = child->next;
			}

			id++;
		}

		cur = cur->next;
	}

	add_scene(pscene);

	return HSB_E_OK;
}

static int load_config(void)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	HSB_DEV_T *pdev = NULL;
	int ret;
	uint32_t max_devid = 1;

	char file[128];
	get_config_file(file);

	doc = xmlParseFile(file);
	if (!doc) {
		hsb_critical("xml parse failed\n");
		return HSB_E_OTHERS;
	}

	cur = xmlDocGetRootElement(doc);
	if (!cur) {
		hsb_critical("load config root empty\n");
		return HSB_E_OTHERS;
	}

	cur = cur->xmlChildrenNode;

	while (cur) {
		if (0 == xmlStrcmp(cur->name, (const xmlChar *)"device")) {
			ret = parse_dev(cur, &pdev);
			if (ret != HSB_E_OK)
				hsb_critical("parse dev fail, ret=%d\n", ret);
			else {
				if (pdev->id >= max_devid)
					max_devid = pdev->id + 1; 
			}
		} else if (0 == xmlStrcmp(cur->name, (const xmlChar *)"scene")) {
			ret = parse_scene(cur);
			if (ret != HSB_E_OK)
				hsb_critical("parse scene fail, ret=%d\n", ret);
		}

		cur = cur->next;
	}

	xmlFreeDoc(doc);

	gl_dev_cb.dev_id = max_devid;

	return HSB_E_OK;
}

int get_dev_id_list(uint32_t *dev_id, int *dev_num)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev;
	int num = 0;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			num = 0;
			break;
		}

		dev_id[num] = pdev->id;
		num++;
	}

	HSB_DEVICE_CB_UNLOCK();

	*dev_num = num;

	return 0;
}

HSB_DEV_T *find_dev(uint32_t dev_id)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->id == dev_id)
			return pdev;
	}

	return NULL;
}

int report_all_device(void *data)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			break;
		}

		HSB_RESP_T resp = { 0 };

		resp.type = HSB_RESP_TYPE_EVENT;
		resp.reply = NULL;
		resp.u.event.devid = pdev->id;
		resp.u.event.id = HSB_EVT_TYPE_DEV_UPDATED;
		resp.u.event.param1 = HSB_DEV_UPDATED_TYPE_ONLINE;
		resp.u.event.param2 = pdev->info.dev_type;


		notify_resp(&resp, data);
	}

	HSB_DEVICE_CB_UNLOCK();

	return 0;
}


static int link_device(HSB_DEV_T *pdev)
{
	if (pdev->driver->id != 3)
		return HSB_E_OK;

	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdevice = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdevice = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdevice) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev == pdevice)
			continue;

		if (0 < strlen(pdev->config.location) &&
		    0 < strlen(pdevice->config.location) &&
		    0 == strncmp(pdevice->config.location,
				 pdev->config.location,
				 sizeof(pdev->config.location)) &&
		    pdevice->info.dev_type == HSB_DEV_TYPE_REMOTE_CTL)
		{
			pdev->ir_dev = pdevice;
			break;
		}
	}

	if (id == len)
		pdev->ir_dev = NULL;

	return HSB_E_OK;
}

static int update_link(HSB_DEV_T *pdev)
{
	if (pdev->info.dev_type != HSB_DEV_TYPE_REMOTE_CTL)
		return HSB_E_OK;

	bool online = (pdev->state == HSB_DEV_STATE_ONLINE) ? true : false;

	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdevice = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdevice = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdevice) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev == pdevice)
			continue;

		if (!online) {
			if (pdevice->ir_dev == pdev) {
				pdevice->ir_dev = NULL;
				link_device(pdevice);
			}
		} else {
			if (0 < strlen(pdev->config.location) &&
			    0 < strlen(pdevice->config.location) &&
			    0 == strncmp(pdevice->config.location,
				 pdev->config.location,
				 sizeof(pdev->config.location)))
			{
				pdevice->ir_dev = pdev;
			}
		}
	}

	return HSB_E_OK;
}

int get_dev_cfg(uint32_t dev_id, HSB_DEV_CONFIG_T *cfg)
{
	HSB_DEV_T *pdev = find_dev(dev_id);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	memcpy(cfg, &pdev->config, sizeof(*cfg));

	return HSB_E_OK;
}

int set_dev_cfg(uint32_t dev_id, const HSB_DEV_CONFIG_T *cfg)
{
	HSB_DEV_T *pdev = find_dev(dev_id);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	memcpy(&pdev->config, cfg, sizeof(*cfg));

	link_device(pdev);
	update_link(pdev);

	save_config();

	return HSB_E_OK;
}

int set_dev_channel(uint32_t devid, char *name, uint32_t cid)
{
	int ret;
	HSB_DEV_T *pdev = find_dev(devid);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	HSB_CHANNEL_DB_T *pdb = pdev->pchan_db;

	if (!pdb)
		return HSB_E_NOT_SUPPORTED;

	ret = set_channel(pdb, name, cid);

	save_config();

	return ret;
}

int del_dev_channel(uint32_t devid, char *name)
{
	int ret;
	HSB_DEV_T *pdev = find_dev(devid);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	HSB_CHANNEL_DB_T *pdb = pdev->pchan_db;

	if (!pdb)
		return HSB_E_NOT_SUPPORTED;

	ret = del_channel(pdb, name);

	save_config();

	return ret;
}

int get_dev_channel(uint32_t devid, char *name, uint32_t *cid)
{
	HSB_DEV_T *pdev = find_dev(devid);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	HSB_CHANNEL_DB_T *pdb = pdev->pchan_db;

	if (!pdb)
		return HSB_E_NOT_SUPPORTED;

	return get_channel(pdb, name, cid);
}

int get_dev_channel_num(uint32_t devid, int *num)
{
	HSB_DEV_T *pdev = find_dev(devid);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	HSB_CHANNEL_DB_T *pdb = pdev->pchan_db;

	if (!pdb)
		return HSB_E_NOT_SUPPORTED;

	return get_channel_num(pdb, num);
}

int get_dev_channel_by_id(uint32_t devid, int id, char *name, uint32_t *cid)
{
	HSB_DEV_T *pdev = find_dev(devid);

	if (!pdev)
		return HSB_E_BAD_PARAM;

	HSB_CHANNEL_DB_T *pdb = pdev->pchan_db;

	if (!pdb)
		return HSB_E_NOT_SUPPORTED;

	return get_channel_by_id(pdb, id, name, cid);
}

static HSB_DEV_DRV_T *_get_dev_drv(uint32_t devid)
{
	HSB_DEV_DRV_T *pdrv = NULL;
	HSB_DEV_T *pdev = NULL;

	HSB_DEVICE_CB_LOCK();

	pdev = find_dev(devid);

	if (!pdev || !pdev->driver) {
		goto fail;
	}

	pdrv = pdev->driver;

	HSB_DEVICE_CB_UNLOCK();

	return pdrv;
fail:
	return NULL;
}

int get_dev_info(uint32_t dev_id, HSB_DEV_T *dev)
{
	int ret = 0;
	HSB_DEV_T *pdev = NULL;

	HSB_DEVICE_CB_LOCK();

	pdev = find_dev(dev_id);
	if (pdev) {
		memcpy(dev, pdev, sizeof(*dev));
	} else {
		ret = -1;
	}

	HSB_DEVICE_CB_UNLOCK();

	return ret;
}


int get_dev_status(HSB_STATUS_T *status)
{
	int ret = HSB_E_NOT_SUPPORTED;

	HSB_DEV_T *pdev = find_dev(status->devid);

	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	if (pdev->op && pdev->op->get_status)
		return pdev->op->get_status(status);

	return ret;
}

int sync_dev_status(HSB_DEV_T *pdev, const HSB_STATUS_T *status)
{
	int cnt, id, val;

	for (cnt = 0; cnt < status->num; cnt++) {
		id = status->id[cnt];
		val = status->val[cnt];

		if (id > 8)
			continue;

		pdev->status.val[id] =  val;
	}

	return HSB_E_OK;
}

int load_dev_status(HSB_DEV_T *pdev, HSB_STATUS_T *status)
{
	int id;

	for (id = 0; id < pdev->status.num; id++) {
		status->id[id] = id;
		status->val[id] = pdev->status.val[id];
	}

	status->num = pdev->status.num;

	return HSB_E_OK;
}

int set_dev_status(const HSB_STATUS_T *status)
{
	int ret = HSB_E_NOT_SUPPORTED;

	if (0 == status->devid && status->id[0] == HSB_STATUS_TYPE_WORK_MODE)
		return set_box_work_mode(status->val[0]);

	HSB_DEV_T *pdev = find_dev(status->devid);

	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	if (pdev->op && pdev->op->set_status) {
		ret = pdev->op->set_status(status);

		if (HSB_E_OK == ret)
			sync_dev_status(pdev, status);
			
	}

	return ret;
}

int set_dev_action(const HSB_ACTION_T *act)
{
	int ret = HSB_E_NOT_SUPPORTED;

	HSB_DEV_T *pdev = find_dev(act->devid);

	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	if (pdev->op && pdev->op->set_action)
		return pdev->op->set_action(act);

	return ret;
}

int get_status(HSB_DEV_T *pdev, HSB_STATUS_T *status)
{
	int ret = HSB_E_NOT_SUPPORTED;

	if (pdev && pdev->op && pdev->op->get_status)
		return pdev->op->get_status(status);

	return ret;
}

int set_status(HSB_DEV_T *pdev, const HSB_STATUS_T *status)
{
	int ret = HSB_E_NOT_SUPPORTED;

	if (pdev && pdev->op && pdev->op->set_status)
		return pdev->op->set_status(status);

	return ret;
}

int set_action(HSB_DEV_T *pdev, const HSB_ACTION_T *act)
{
	int ret = HSB_E_NOT_SUPPORTED;

	if (pdev && pdev->op && pdev->op->set_action)
		return pdev->op->set_action(act);

	return ret;
}


static HSB_DEV_DRV_T *_find_drv(uint32_t drv_id)
{
	GQueue *queue = &gl_dev_cb.driverq;
	HSB_DEV_DRV_T *pdrv = NULL;
	int len, id;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdrv = (HSB_DEV_DRV_T *)g_queue_peek_nth(queue, id);
		if (!pdrv) {
			hsb_critical("drv null\n");
			continue;
		}

		if (pdrv->id == drv_id)
			return pdrv;
	}

	return NULL;

}

int probe_dev(uint32_t drv_id)
{
	HSB_DEV_DRV_T *pdrv = _find_drv(drv_id);

	if (!pdrv)
		return HSB_E_BAD_PARAM;

	if (pdrv->op && pdrv->op->probe)
		return pdrv->op->probe();

	return HSB_E_OK;
}

int add_dev(uint32_t drv_id, HSB_DEV_TYPE_T dev_type, HSB_DEV_CONFIG_T *cfg)
{
	HSB_DEV_DRV_T *pdrv = _find_drv(drv_id);

	if (!pdrv)
		return HSB_E_BAD_PARAM;

	if (pdrv->op && pdrv->op->add_dev)
		return pdrv->op->add_dev(dev_type, cfg);

	return HSB_E_NOT_SUPPORTED;
}

int del_dev(uint32_t devid)
{
	HSB_DEV_T *pdev = find_dev(devid);
	if (!pdev)
		return HSB_E_BAD_PARAM;

	HSB_DEV_DRV_T *pdrv = pdev->driver;
	if (!pdrv)
		return HSB_E_BAD_PARAM;

	if (pdrv->op && pdrv->op->del_dev)
		return pdrv->op->del_dev(devid);

	return HSB_E_NOT_SUPPORTED;
}

static uint32_t alloc_dev_id(void)
{
	uint32_t dev_id;

	HSB_DEVICE_CB_LOCK();

	dev_id = gl_dev_cb.dev_id++;

	HSB_DEVICE_CB_UNLOCK();
	
	return dev_id;
}


int register_dev_drv(HSB_DEV_DRV_T *drv)
{
	if (!drv)
		return HSB_E_BAD_PARAM;

	int len, id;
	GQueue *queue = &gl_dev_cb.driverq;
	HSB_DEV_DRV_T *pdrv = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdrv = (HSB_DEV_DRV_T *)g_queue_peek_nth(queue, id);
		if (!pdrv) {
			hsb_critical("drv null\n");
			continue;
		}

		if (pdrv->id == drv->id) {
			hsb_critical("%s driver load fail, drv_id alreasy exists.\n", drv->id);
			return HSB_E_ENTRY_EXISTS;
		}
	}

	g_queue_push_tail(queue, drv);

	hsb_debug("%s driver loaded\n", drv->name);

	return HSB_E_OK;
}

HSB_DEV_T *find_dev_by_ip(struct in_addr *ip)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (ip->s_addr == pdev->prty.ip.s_addr)
			return pdev;
	}

	return NULL;
}

HSB_DEV_T *alloc_dev(uint32_t devid)
{
	HSB_DEV_T *pdev = g_slice_new0(HSB_DEV_T);
	if (!pdev)
		return NULL;

	/* set default value */
	pdev->id = devid;
	pdev->work_mode = HSB_WORK_MODE_ALL;

	return pdev;
}

HSB_DEV_T *create_dev(void)
{
	HSB_DEV_T *pdev = g_slice_new0(HSB_DEV_T);
	if (!pdev)
		return NULL;

	/* set default value */
	pdev->id = alloc_dev_id();
	pdev->work_mode = HSB_WORK_MODE_ALL;

	return pdev;
}

int destroy_dev(HSB_DEV_T *dev)
{
	g_slice_free(HSB_DEV_T, dev);

	return 0;
}

int register_dev(HSB_DEV_T *dev)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;

	HSB_DEVICE_CB_LOCK();

	g_queue_push_tail(queue, dev);

	HSB_DEVICE_CB_UNLOCK();

	dev_updated(dev->id, HSB_DEV_UPDATED_TYPE_NEW_ADD, dev->info.dev_type);

	return 0;
}

int remove_dev(HSB_DEV_T *dev)
{
	GQueue *queue = &gl_dev_cb.queue;

	HSB_DEVICE_CB_LOCK();
	g_queue_remove(queue, dev);
	HSB_DEVICE_CB_UNLOCK();

	dev_updated(dev->id, HSB_DEV_UPDATED_TYPE_OFFLINE, dev->info.dev_type);

	return 0;
}

static int recover_dev(void)
{
	int ret;
	guint id;
	GQueue *queue = &gl_dev_cb.offq;
	HSB_DEV_T	*pdev = NULL;
	HSB_DEV_DRV_T	*drv = NULL;

	id = 0;
	while (id < g_queue_get_length(queue))
	{
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		drv = _find_drv(pdev->drvid);
		if (!drv) {
			hsb_critical("drv not found: %d\n", pdev->drvid);
			continue;
		}

		ret = HSB_E_OTHERS;
		if (drv->op && drv->op->recover_dev)
			ret = drv->op->recover_dev(pdev->id, pdev->info.dev_type);

		if (HSB_E_OK == ret)
			continue;

		id++;
	}

	return HSB_E_OK;
}

#include "hsb_const.h"

static void get_default_config(uint32_t dev_type, HSB_DEV_CONFIG_T *config)
{
	if (dev_type < sizeof(default_config))
		memcpy(config, &default_config[dev_type], sizeof(*config));
	else
		memset(config, 0, sizeof(*config));
}

int dev_online(uint32_t drvid,
		HSB_DEV_INFO_T *info,
		HSB_DEV_STATUS_T *status,
		HSB_DEV_OP_T *op,
		HSB_DEV_CONFIG_T *cfg,
		bool support_channel,
		void *priv,
		uint32_t *devid)
{
	int ret = HSB_E_OK;
	GQueue *offq = &gl_dev_cb.offq;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T *pdev = NULL;
	guint len, id;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(offq);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(offq, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (drvid == pdev->drvid &&
		    0 == memcmp(pdev->info.mac, info->mac, 8)) {
			break;
		}
	}

	HSB_DEVICE_CB_UNLOCK();

	if (id == len) { /* not found in offq */
		pdev = create_dev();
		if (support_channel)
			pdev->pchan_db = alloc_channel_db();
		pdev->drvid = drvid;
		pdev->driver = _find_drv(drvid);
		pdev->op = op;
		pdev->priv_data = priv;
		pdev->state = HSB_DEV_STATE_ONLINE;

		memcpy(&pdev->status, status, sizeof(*status));
		memcpy(&pdev->info, info, sizeof(*info));

		if (!cfg) {
			HSB_DEV_CONFIG_T config;
			get_default_config(info->dev_type, &config);

			memcpy(&pdev->config, &config, sizeof(config));
		} else {
			memcpy(&pdev->config, cfg, sizeof(*cfg));
		}

		HSB_DEVICE_CB_LOCK();
		g_queue_push_tail(queue, pdev);
		HSB_DEVICE_CB_UNLOCK();

		link_device(pdev);
		update_link(pdev);

		dev_updated(pdev->id, HSB_DEV_UPDATED_TYPE_NEW_ADD, pdev->info.dev_type);

		hsb_debug("device newadd %d\n", pdev->id);
		/* TODO */
		save_config();
	} else {
		g_queue_pop_nth(offq, id);

		if (support_channel && !pdev->pchan_db)
			pdev->pchan_db = alloc_channel_db();
		pdev->drvid = drvid;
		pdev->driver = _find_drv(drvid);
		pdev->op = op;
		pdev->priv_data = priv;
		pdev->state = HSB_DEV_STATE_ONLINE;

		memcpy(&pdev->info, info, sizeof(*info));
		memcpy(&pdev->status, status, sizeof(*status));

		HSB_DEVICE_CB_LOCK();
		g_queue_push_tail(queue, pdev);
		HSB_DEVICE_CB_UNLOCK();

		link_device(pdev);
		update_link(pdev);

		dev_updated(pdev->id, HSB_DEV_UPDATED_TYPE_ONLINE, pdev->info.dev_type);

		hsb_debug("device online %d\n", pdev->id);
	}

	*devid = pdev->id;

	return ret;
}

int dev_offline(uint32_t devid)
{
	int ret = HSB_E_OK;
	GQueue *offq = &gl_dev_cb.offq;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T *pdev = NULL;
	guint len, id;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->id == devid)
			break;
	}

	if (id == len) {
		hsb_critical("dev %d not found in queue\n", devid);
		HSB_DEVICE_CB_UNLOCK();
		return HSB_E_OTHERS;
	}

	pdev->state = HSB_DEV_STATE_OFFLINE;

	g_queue_pop_nth(queue, id);

	g_queue_push_tail(offq, pdev);

	HSB_DEVICE_CB_UNLOCK();

	update_link(pdev);

	dev_updated(devid, HSB_DEV_UPDATED_TYPE_OFFLINE, pdev->info.dev_type);

	return ret;
}

int dev_removed(uint32_t devid)
{
	int ret = HSB_E_OK;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T *pdev = NULL;
	guint len, id;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->id == devid)
			break;
	}

	if (id == len) {
		hsb_critical("dev %d not found in queue\n", devid);
		HSB_DEVICE_CB_UNLOCK();
		return HSB_E_OTHERS;
	}

	pdev->state = HSB_DEV_STATE_OFFLINE;

	g_queue_pop_nth(queue, id);

	HSB_DEVICE_CB_UNLOCK();

	update_link(pdev);

	dev_updated(devid, HSB_DEV_UPDATED_TYPE_OFFLINE, pdev->info.dev_type);

	destroy_dev(pdev);

	return ret;
}

int dev_recovered(uint32_t devid,
		uint32_t drvid,
		HSB_DEV_INFO_T *info,
		HSB_DEV_STATUS_T *status,
		HSB_DEV_OP_T *op,
		bool support_channel,
		void *priv)
{
	int ret = HSB_E_OK;
	GQueue *offq = &gl_dev_cb.offq;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T *pdev = NULL;
	guint len, id;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(offq);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(offq, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (devid == pdev->id) {
			break;
		}
	}

	HSB_DEVICE_CB_UNLOCK();

	if (id == len) { /* not found in offq */
		hsb_critical("not found in offq!\n");
		return HSB_E_OTHERS;
	}

	g_queue_pop_nth(offq, id);

	if (support_channel && !pdev->pchan_db)
		pdev->pchan_db = alloc_channel_db();
	pdev->drvid = drvid;
	pdev->driver = _find_drv(drvid);
	pdev->op = op;
	pdev->priv_data = priv;
	pdev->state = HSB_DEV_STATE_ONLINE;

	memcpy(&pdev->info, info, sizeof(*info));
	memcpy(&pdev->status, status, sizeof(*status));

	HSB_DEVICE_CB_LOCK();
	g_queue_push_tail(queue, pdev);
	HSB_DEVICE_CB_UNLOCK();

	link_device(pdev);
	update_link(pdev);

	dev_updated(pdev->id, HSB_DEV_UPDATED_TYPE_ONLINE, pdev->info.dev_type);

	hsb_debug("device recoverd %d\n", pdev->id);

	return ret;


}

static int check_linkage(uint32_t devid, HSB_EVT_T *evt)
{
	HSB_DEV_T *pdev = find_dev(devid);
	if (!pdev)
		return HSB_E_OTHERS;

	HSB_WORK_MODE_T work_mode = gl_dev_cb.work_mode;
	HSB_LINKAGE_T *link = pdev->link;
	HSB_LINKAGE_STATUS_T *status = pdev->link_status;

	if (!CHECK_BIT(pdev->work_mode, work_mode))
		return HSB_E_OTHERS;

	int id;
	uint8_t flag;

	for (id = 0; id < HSB_DEV_MAX_LINKAGE_NUM; id++, link++, status++) {
		if (!status->active)
			continue;

		if (!CHECK_BIT(link->work_mode, work_mode))
			continue;

		if (evt->id != link->evt_id ||
		    evt->param1 != link->evt_param1 ||
		    evt->param2 != link->evt_param2)
			continue;

		flag = link->flag;
		if (CHECK_BIT(flag, 0)) {
			HSB_ACTION_T action;
			action.devid = link->act_devid;
			action.id = link->act_id;
			action.param1 = link->act_param1;
			action.param2 = link->act_param2;
			set_dev_action_async(&action, NULL);
		} else  {
			HSB_STATUS_T stat;
			stat.devid = link->act_devid;
			stat.num = 1;
			stat.id[0] = link->act_id;
			stat.val[0] = link->act_param1;
			set_dev_status_async(&stat, NULL);
		}
	}

	return HSB_E_OK;
}


static _dev_event(uint32_t devid, HSB_EVT_TYPE_T type, uint16_t param1, uint32_t param2)
{
	HSB_RESP_T resp = { 0 };

	resp.type = HSB_RESP_TYPE_EVENT;
	resp.reply = NULL;
	resp.u.event.devid = devid;
	resp.u.event.id = type;
	resp.u.event.param1 = param1;
	resp.u.event.param2 = param2;

	check_linkage(devid, &resp.u.event);

	hsb_debug("get event: %d, %d, %d, %x\n", devid, type, param1, param2);

	return notify_resp(&resp, NULL);
}

int dev_status_updated(uint32_t devid, HSB_STATUS_T *status)
{
	HSB_DEV_T *pdev = find_dev(devid);
	if (pdev)
		sync_dev_status(pdev, (const HSB_STATUS_T *)status);

	HSB_RESP_T resp = { 0 };
	resp.type = HSB_RESP_TYPE_STATUS_UPDATE;
	resp.reply = NULL;
	memcpy(&resp.u.status, status, sizeof(*status));

	// TODO: check linkage

	return notify_resp(&resp, NULL);
}

int dev_updated(uint32_t devid, HSB_DEV_UPDATED_TYPE_T type, HSB_DEV_TYPE_T dev_type)
{
	return _dev_event(devid, HSB_EVT_TYPE_DEV_UPDATED, type, dev_type);
}

int dev_sensor_triggered(uint32_t devid, HSB_SENSOR_TYPE_T type)
{
	return _dev_event(devid, HSB_EVT_TYPE_SENSOR_TRIGGERED, type, 0);
}

int dev_sensor_recovered(uint32_t devid, HSB_SENSOR_TYPE_T type)
{
	return _dev_event(devid, HSB_EVT_TYPE_SENSOR_RECOVERED, type, 0);
}

int dev_ir_key(uint32_t devid, uint16_t param1, uint32_t param2)
{
	return _dev_event(devid, HSB_EVT_TYPE_IR_KEY, param1, param2);
}

int dev_mode_changed(HSB_WORK_MODE_T mode)
{
	return _dev_event(0, HSB_EVT_TYPE_MODE_CHANGED, mode, 0);
}

int set_box_work_mode(HSB_WORK_MODE_T mode)
{
	int ret = HSB_E_OK;
	if (!HSB_WORK_MODE_VALID(mode))
		return HSB_E_BAD_PARAM;

	gl_dev_cb.work_mode = mode;

	dev_mode_changed(mode);

	return ret;
}

HSB_WORK_MODE_T get_box_work_mode(void)
{
	return gl_dev_cb.work_mode;
}

static void probe_all_devices(void)
{
	GQueue *queue = &gl_dev_cb.driverq;
	HSB_DEV_DRV_T *drv;
	int id, len = g_queue_get_length(queue);

	for (id = 0; id < len; id++) {
		drv = g_queue_peek_nth(queue, id);
		if (drv->op && drv->op->probe)
			drv->op->probe();
	}
}

int probe_dev_async(const HSB_PROBE_T *probe, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_PROBE;
	act->reply = reply;
	act->u.probe.drvid = probe->drvid;

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

int set_dev_status_async(const HSB_STATUS_T *status, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_SET_STATUS;
	act->reply = reply;
	memcpy(&act->u.status, status, sizeof(*status));

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

int get_dev_status_async(uint32_t devid, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_GET_STATUS;
	act->reply = reply;
	act->u.status.devid = devid;

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

int set_dev_action_async(const HSB_ACTION_T *action, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_DO_ACTION;
	act->reply = reply;
	memcpy(&act->u.action, action, sizeof(*action));

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

void _process_dev_act(HSB_ACT_T *act)
{
	int ret;
	HSB_ACT_TYPE_T type = act->type;
	HSB_RESP_T resp = { 0 };
	void *reply = act->reply;
	resp.reply = reply;

	switch (type) {
		case HSB_ACT_TYPE_PROBE:
		{
			ret = probe_dev(act->u.probe.drvid);
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.devid = 0;
			resp.u.result.cmd = HSB_CMD_PROBE_DEV;
			resp.u.result.ret_val = ret;

			break;
		}
		case HSB_ACT_TYPE_SET_STATUS:
		{
			HSB_STATUS_T *pstat = &act->u.status;
			ret = set_dev_status(pstat);
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.devid = pstat->devid;
			resp.u.result.cmd = HSB_CMD_SET_STATUS;
			resp.u.result.ret_val = ret;

			break;
		}
		case HSB_ACT_TYPE_GET_STATUS:
		{
			HSB_STATUS_T *pstat = &act->u.status;
			ret = get_dev_status(pstat);

			if (HSB_E_OK != ret) {
				resp.type = HSB_RESP_TYPE_RESULT;
				resp.u.result.devid = pstat->devid;
				resp.u.result.cmd = HSB_CMD_GET_STATUS;
				resp.u.result.ret_val = ret;
			} else {
				resp.type = HSB_RESP_TYPE_STATUS;
				memcpy(&resp.u.status, &act->u.status, sizeof(HSB_STATUS_T));
			}
			
			break;
		}
		case HSB_ACT_TYPE_DO_ACTION:
		{
			HSB_ACTION_T *pact = &act->u.action;
			ret = set_dev_action(pact);
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.devid = pact->devid;
			resp.u.result.cmd = HSB_CMD_DO_ACTION;
			resp.u.result.ret_val = ret;

			break;
		}
		default:
			hsb_debug("unkown act type\n");
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.ret_val = HSB_E_NOT_SUPPORTED;
			break;
	}

	notify_resp(&resp, NULL);

	return;
}

static void *async_process_thread(thread_data_control *thread_data)
{
	HSB_ACT_T *data = NULL;

	pthread_mutex_lock(&thread_data->mutex);
	
	while (thread_data->active)
	{
		while (0 == g_queue_get_length(thread_data->data_queue) &&
			thread_data->active)
		{
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec++;
			int error_no = pthread_cond_timedwait(&thread_data->cond, &thread_data->mutex, &ts);
			if (ETIMEDOUT == error_no)
				continue;
		}

		if (thread_data->active == FALSE)
			break;

		data = (HSB_ACT_T *)g_queue_pop_head(thread_data->data_queue);
		pthread_mutex_unlock(&thread_data->mutex);
		if (data == NULL)
		{
			pthread_mutex_lock(&thread_data->mutex);
			continue;
		}

		/* process data */
		_process_dev_act(data);

		/* free data */
		g_slice_free(HSB_ACT_T, data);

		pthread_mutex_lock(&thread_data->mutex);
	}

	pthread_mutex_unlock(&thread_data->mutex);

	return NULL;
}

static int init_private_thread(void)
{
	thread_data_control *tptr;

	tptr = &gl_dev_cb.async_thread_ctl;

 	thread_control_init(tptr);
	thread_control_activate(tptr);

	pthread_t thread_id;
	if (pthread_create(&tptr->thread_id, NULL, (thread_entry_func)async_process_thread, tptr))
	{
		hsb_critical("create async thread failed\n");
		return -1;
	}

	return 0;
}

int init_dev_module(void)
{
	g_queue_init(&gl_dev_cb.queue);
	g_mutex_init(&gl_dev_cb.mutex);
	g_queue_init(&gl_dev_cb.driverq);
	g_queue_init(&gl_dev_cb.offq);

	init_scene();

	/* reserve hsb id=0 */
	gl_dev_cb.dev_id = 1;

	init_private_thread();

	load_config();

	init_virtual_switch_drv();
	init_cz_drv();
	init_ir_drv();

	recover_dev();

	probe_all_devices();

	return 0;
}

int get_dev_timer(uint32_t dev_id, uint16_t timer_id, HSB_TIMER_T *timer)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (timer_id >= (sizeof(dev->timer) / sizeof(dev->timer[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_TIMER_T *tm = &dev->timer[timer_id];
	HSB_TIMER_STATUS_T *status = &dev->timer_status[timer_id];

	memcpy(timer, tm, sizeof(*tm));

_out:
	return ret;
}

int set_dev_timer(uint32_t dev_id, const HSB_TIMER_T *timer)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;
	uint16_t timer_id = timer->id;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (timer_id >= (sizeof(dev->timer) / sizeof(dev->timer[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_TIMER_T *tm = &dev->timer[timer_id];
	HSB_TIMER_STATUS_T *status = &dev->timer_status[timer_id];

	memcpy(tm, timer, sizeof(*tm));
	memset(status, 0, sizeof(*status));
	status->active = true;
	status->expired = false;

	save_config();

_out:
	return ret;
}

int del_dev_timer(uint32_t dev_id, uint16_t timer_id)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (timer_id >= (sizeof(dev->timer) / sizeof(dev->timer[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_TIMER_T *tm = &dev->timer[timer_id];
	HSB_TIMER_STATUS_T *status = &dev->timer_status[timer_id];

	memset(tm, 0, sizeof(*tm));
	memset(status, 0, sizeof(*status));

	save_config();

_out:
	return ret;
}

int get_dev_delay(uint32_t dev_id, uint16_t delay_id, HSB_DELAY_T *delay)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (delay_id >= (sizeof(dev->delay) / sizeof(dev->delay[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_DELAY_T *dl = &dev->delay[delay_id];
	HSB_DELAY_STATUS_T *status = &dev->delay_status[delay_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memcpy(delay, dl, sizeof(*dl));

_out:
	return ret;
}

int set_dev_delay(uint32_t dev_id, const HSB_DELAY_T *delay)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;
	uint16_t delay_id = delay->id;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (delay_id >= (sizeof(dev->delay) / sizeof(dev->delay[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_DELAY_T *dl = &dev->delay[delay_id];
	HSB_DELAY_STATUS_T *status = &dev->delay_status[delay_id];

	memcpy(dl, delay, sizeof(*dl));
	status->active = true;

_out:
	return ret;
}

int del_dev_delay(uint32_t dev_id, uint16_t delay_id)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (delay_id >= (sizeof(dev->delay) / sizeof(dev->delay[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_DELAY_T *dl = &dev->delay[delay_id];
	HSB_DELAY_STATUS_T *status = &dev->delay_status[delay_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memset(dl, 0, sizeof(*dl));
	status->active = false;

_out:
	return ret;
}

int get_dev_linkage(uint32_t dev_id, uint16_t link_id, HSB_LINKAGE_T *link)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (link_id >= (sizeof(dev->link) / sizeof(dev->link[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_LINKAGE_T *lk = &dev->link[link_id];
	HSB_LINKAGE_STATUS_T *status = &dev->link_status[link_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memcpy(link, lk, sizeof(*lk));

_out:
	return ret;

}

int set_dev_linkage(uint32_t dev_id, const HSB_LINKAGE_T *link)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;
	uint16_t link_id = link->id;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (link_id >= (sizeof(dev->link) / sizeof(dev->link[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_LINKAGE_T *lk = &dev->link[link_id];
	HSB_LINKAGE_STATUS_T *status = &dev->link_status[link_id];

	memcpy(lk, link, sizeof(*lk));
	status->active = true;

_out:
	return ret;
}


int del_dev_linkage(uint32_t dev_id, uint16_t link_id)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (link_id >= (sizeof(dev->link) / sizeof(dev->link[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_LINKAGE_T *lk = &dev->link[link_id];
	HSB_LINKAGE_STATUS_T *status = &dev->link_status[link_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memset(lk, 0, sizeof(*lk));
	status->active = false;

_out:
	return ret;
}

static int compare_date(HSB_TIMER_T *ptimer, struct tm *tm_now)
{
	if (ptimer->year == 0 &&
		ptimer->mon == 0 &&
		ptimer->mday == 0)
	{
		return -1;
	}

	
	if (tm_now->tm_year == ptimer->year &&
		tm_now->tm_mon == ptimer->mon &&
		tm_now->tm_mday == ptimer->mday)
	{
		return 0;
	}

	return 1;
}


static void _check_dev_timer_and_delay(void *data, void *user_data)
{
	HSB_DEV_T *pdev = (HSB_DEV_T *)data;

	if (!pdev) {
		hsb_critical("null device\n");
		return;
	}

	HSB_WORK_MODE_T work_mode = gl_dev_cb.work_mode;

	time_t now = time(NULL);
	struct tm tm_now;
	localtime_r(&now, &tm_now);
	uint32_t sec_today = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;
	uint32_t sec_timer;
	bool bnextday = false;
	int cnt, ret;
	uint8_t flag, weekday;
	HSB_TIMER_T *ptimer = pdev->timer;
	HSB_TIMER_STATUS_T *tstatus = pdev->timer_status;
	HSB_DELAY_T *pdelay = pdev->delay;
	HSB_DELAY_STATUS_T *dstatus = pdev->delay_status;

	if (sec_today < gl_dev_cb.sec_today && sec_today < 10) {
		bnextday = true;
		
		for (cnt = 0; cnt < HSB_DEV_MAX_TIMER_NUM; cnt++, ptimer++, tstatus++) {
			flag = ptimer->flag;
			//if (!tstatus->active)
			if (!CHECK_BIT(flag, 1))
				continue;

			ret = compare_date(ptimer, &tm_now);
			if (0 == ret) {
				tstatus->expired = false;
			} else if (1 == ret) {
				tstatus->expired = true;
			} else if (-1 == ret) {
				weekday = ptimer->wday;
				if (!CHECK_BIT(weekday, tm_now.tm_wday))
					tstatus->expired = true;
				else
					tstatus->expired = false;
			}
		}
	}

	gl_dev_cb.sec_today = sec_today;

	if (!CHECK_BIT(pdev->work_mode, work_mode))
		return;

	ptimer = pdev->timer;
	tstatus = pdev->timer_status;

	for (cnt = 0; cnt < HSB_DEV_MAX_TIMER_NUM; cnt++, ptimer++, tstatus++) {
		flag = ptimer->flag;
		/* 1.chekc active & expired */
		//if (!tstatus->active || tstatus->expired)
		if (!CHECK_BIT(flag, 1) || tstatus->expired)
			continue;

		/* 2.check work mode */
		if (!CHECK_BIT(ptimer->work_mode, work_mode))
			continue;

		/* 3.check flag & weekday */
		weekday = ptimer->wday;
		ret = compare_date(ptimer, &tm_now);

		if (-1 == ret) {
			if (!CHECK_BIT(weekday, tm_now.tm_wday)) {
				continue;
			}
		} else if (1 == ret) {
			tstatus->expired = true;
			continue;
		}

		/* 4.check time */
		sec_timer = ptimer->hour * 3600 + ptimer->min * 60 + ptimer->sec;

		if (sec_timer > sec_today || sec_today - sec_timer > 5)
			continue;

		/* 5.do action */
		if (CHECK_BIT(flag, 0)) {
			HSB_ACTION_T action;
			action.devid = pdev->id;
			action.id = ptimer->act_id;
			action.param1 = ptimer->act_param1;
			action.param2 = ptimer->act_param2;
			set_dev_action_async(&action, NULL);
		} else  {
			HSB_STATUS_T stat;
			stat.devid = pdev->id;
			stat.num = 1;
			stat.id[0] = ptimer->act_id;
			stat.val[0] = ptimer->act_param1;
			set_dev_status_async(&stat, NULL);
		}

		hsb_debug("timer expired\n");

		if (CHECK_BIT(weekday, 7)) { /* One shot */
			//memset(ptimer, 0, sizeof(*ptimer));
			tstatus->active = false;
			tstatus->expired = true;
			continue;
		}

		tstatus->expired = true;
	}

	pdelay = pdev->delay;
	dstatus = pdev->delay_status;

	for (cnt = 0; cnt < HSB_DEV_MAX_DELAY_NUM; cnt++, pdelay++, dstatus++) {
		/* check active & started */
		if (!dstatus->active || !dstatus->started)
			continue;

		/* check work mode */
		if (!CHECK_BIT(pdelay->work_mode, work_mode))
			continue;

		/* check time */
		if (now - dstatus->start_tm < pdelay->delay_sec)
			continue;

		/* do action */
		flag = pdelay->flag;
		if (CHECK_BIT(flag, 0)) {
			HSB_ACTION_T action;
			action.devid = pdev->id;
			action.id = pdelay->act_id;
			action.param1 = pdelay->act_param1;
			action.param2 = pdelay->act_param2;
			set_dev_action_async(&action, NULL);
		} else  {
			HSB_STATUS_T stat;
			stat.devid = pdev->id;
			stat.num = 1;
			stat.id[0] = pdelay->act_id;
			stat.val[0] = pdelay->act_param1;
			set_dev_status_async(&stat, NULL);
		}

		dstatus->started = false;
	}

	return;
}

int check_timer_and_delay(void)
{
	GQueue *queue = &gl_dev_cb.queue;
	g_queue_foreach(queue, _check_dev_timer_and_delay, NULL);

	return HSB_E_OK;
}


