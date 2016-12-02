
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include "debug.h"
#include "hsb_error.h"
#include "hsb_config.h"
#include "device.h"
#include "thread_utils.h"
#include "network.h"
#include "network_utils.h"
#include "net_protocol.h"
#include "scene.h"
#include "utils.h"

#define MAKE_CMD_HDR(_buf, _cmd, _len)	do { \
	SET_CMD_FIELD(_buf, 0, uint16_t, _cmd); \
	SET_CMD_FIELD(_buf, 2, uint16_t, _len); \
} while (0)

#define REPLY_OK	(1)
#define REPLY_FAIL	(0)

static int check_tcp_pkt_valid(uint8_t *buf, int len)
{
	if (!buf || len <= 0)
		return -1;

	uint16_t command = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t length = GET_CMD_FIELD(buf, 2, uint16_t);

	if (!HSB_CMD_VALID(command))
		return -2;

	if (len < length)
		return -3;

	return 0;
}

static int _reply_result(uint8_t *buf, int errcode, uint32_t devid, uint16_t cmd)
{
	int len = 12;

	MAKE_CMD_HDR(buf, HSB_CMD_RESULT, len);

	SET_CMD_FIELD(buf, 4, uint32_t, devid);
	SET_CMD_FIELD(buf, 8, uint16_t, cmd);
	SET_CMD_FIELD(buf, 10, uint16_t, errcode);

	return len;
}

static int _reply_dev_id_list(uint8_t *buf, uint32_t *dev_id, int dev_num)
{
	int len, id;

	len = dev_num * 4 + 4;
	MAKE_CMD_HDR(buf, HSB_CMD_GET_DEVS_RESP, len);

	for (id = 0; id < dev_num; id++) {
		SET_CMD_FIELD(buf, 4 + id * 4, uint32_t, dev_id[id]);
	}

	return len;
}

static int _reply_get_device_info(uint8_t *buf, HSB_DEV_T *dev)
{
	int len = 26;

	MAKE_CMD_HDR(buf, HSB_CMD_GET_INFO_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev->id); /* device id */
	SET_CMD_FIELD(buf, 8, uint32_t, dev->driver->id); /* driver id */
	SET_CMD_FIELD(buf, 12, uint16_t, dev->info.cls); /* device class */
	SET_CMD_FIELD(buf, 14, uint16_t, dev->info.interface); /* device interface */
	SET_CMD_FIELD(buf, 16, uint32_t, dev->info.dev_type);
	memcpy(buf + 18, dev->info.mac, 8); /* mac address */

	return len;
}

static int _reply_get_device_cfg(uint8_t *buf, uint32_t dev_id, HSB_DEV_CONFIG_T *cfg)
{
	int len = 8 + sizeof(cfg->name) + sizeof(cfg->location);

	MAKE_CMD_HDR(buf, HSB_CMD_GET_CONFIG_RESP, len);
	SET_CMD_FIELD(buf, 4, uint32_t, dev_id); /* device id */
	memcpy(buf + 8, cfg, sizeof(*cfg));

	return len;
}

static int _reply_get_device_channel(uint8_t *buf, uint32_t dev_id, char *name, uint32_t cid)
{
	int len = 12 + HSB_CHANNEL_MAX_NAME_LEN;
	MAKE_CMD_HDR(buf, HSB_CMD_GET_CHANNEL_RESP, len);
	SET_CMD_FIELD(buf, 4, uint32_t, dev_id); /* device id */
	memcpy(buf + 8, name, HSB_CHANNEL_MAX_NAME_LEN);
	SET_CMD_FIELD(buf, 8 + HSB_CHANNEL_MAX_NAME_LEN, uint32_t, cid);

	return len;
}

/*
static int _reply_get_device_status(uint8_t *buf, uint32_t dev_id, HSB_STATUS_T *status, int num)
{
	int len = 8 + num * 4;
	int id;

	MAKE_CMD_HDR(buf, HSB_CMD_GET_STATUS_RESP, len);
	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);

	for (id = 0; id < num; id++) {
		SET_CMD_FIELD(buf, 8 + id * 4, uint16_t, status[id].id);
		SET_CMD_FIELD(buf, 10 + id * 4, uint16_t, status[id].val);
	}

	return len;
}
*/

static int  _reply_get_timer(uint8_t *buf, uint32_t dev_id, HSB_TIMER_T *tm)
{
	int len = 28;
	
	MAKE_CMD_HDR(buf, HSB_CMD_GET_TIMER_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint16_t, tm->id);
	SET_CMD_FIELD(buf, 10, uint8_t, tm->work_mode);
	SET_CMD_FIELD(buf, 11, uint8_t, tm->flag);
	SET_CMD_FIELD(buf, 12, uint8_t, tm->hour);
	SET_CMD_FIELD(buf, 13, uint8_t, tm->min);
	SET_CMD_FIELD(buf, 14, uint8_t, tm->sec);
	SET_CMD_FIELD(buf, 15, uint8_t, tm->wday);
	int year = 0, mon = 0;
	if (tm->year > 0) {
		year = tm->year + 1900;
		mon = tm->mon + 1;
	}

	hsb_debug("get year %d mon %d\n", year, mon);
	hsb_debug("tm->year %d tm->mon %d\n", tm->year, tm->mon);

	SET_CMD_FIELD(buf, 16, uint16_t, year);
	SET_CMD_FIELD(buf, 18, uint8_t, mon);
	SET_CMD_FIELD(buf, 19, uint8_t, tm->mday);
	SET_CMD_FIELD(buf, 20, uint16_t, tm->act_id);
	SET_CMD_FIELD(buf, 22, uint16_t, tm->act_param1);
	SET_CMD_FIELD(buf, 24, uint32_t, tm->act_param2);

	return len;
}

static int  _reply_get_delay(uint8_t *buf, uint32_t dev_id, HSB_DELAY_T *delay)
{
	int len = 32;
	
	MAKE_CMD_HDR(buf, HSB_CMD_GET_DELAY_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint16_t, delay->id);
	SET_CMD_FIELD(buf, 10, uint8_t, delay->work_mode);
	SET_CMD_FIELD(buf, 11, uint8_t, delay->flag);
	SET_CMD_FIELD(buf, 12, uint16_t, delay->evt_id);
	SET_CMD_FIELD(buf, 14, uint16_t, delay->evt_param1);
	SET_CMD_FIELD(buf, 16, uint32_t, delay->evt_param2);
	SET_CMD_FIELD(buf, 20, uint16_t, delay->act_id);
	SET_CMD_FIELD(buf, 22, uint16_t, delay->act_param1);
	SET_CMD_FIELD(buf, 24, uint32_t, delay->act_param2);
	SET_CMD_FIELD(buf, 28, uint32_t, delay->delay_sec);

	return len;
}

static int  _reply_get_linkage(uint8_t *buf, uint32_t dev_id, HSB_LINKAGE_T *link)
{
	int len = 32;
	
	MAKE_CMD_HDR(buf, HSB_CMD_GET_LINKAGE_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint16_t,link->id);
	SET_CMD_FIELD(buf, 10, uint8_t,link->work_mode);
	SET_CMD_FIELD(buf, 11, uint8_t,link->flag);
	SET_CMD_FIELD(buf, 12, uint16_t, link->evt_id);
	SET_CMD_FIELD(buf, 14, uint16_t, link->evt_param1);
	SET_CMD_FIELD(buf, 16, uint32_t, link->evt_param2);
	SET_CMD_FIELD(buf, 20, uint32_t, link->act_devid);
	SET_CMD_FIELD(buf, 24, uint16_t, link->act_id);
	SET_CMD_FIELD(buf, 26, uint16_t, link->act_param1);
	SET_CMD_FIELD(buf, 28, uint32_t, link->act_param2);

	return len;
}

static int parse_scene(HSB_SCENE_T *scene, uint8_t *buf, int len)
{
	int action_cnt = 0;
	int id, offset = 4;
	strncpy(scene->name, buf + offset, HSB_SCENE_MAX_NAME_LEN);
	offset += HSB_SCENE_MAX_NAME_LEN;

	HSB_SCENE_ACTION_T *paction = &scene->actions[0];
	HSB_SCENE_CONDITION_T *pcond = NULL;
	HSB_SCENE_ACT_T *pact = NULL;

	while (offset < len) {
		if (buf[offset] != 0xFF) {
			hsb_debug("invalid scene, header %x\n", buf[offset]);
			return HSB_E_INVALID_MSG;
		}

		paction->delay = buf[offset + 1];
		paction->has_cond = buf[offset + 2] > 0 ? true : false;
		paction->act_num = buf[offset + 3];
		offset += 4;

		hsb_debug("found a action, %d-%d-%d\n", paction->delay, paction->has_cond, paction->act_num);

		if (paction->has_cond) {
			pcond = &paction->condition;
			if (buf[offset] != 0xFE) {
				hsb_debug("invalid scene, condition header %x\n", buf[offset]);
				return HSB_E_INVALID_MSG;
			}

			pcond->expr = buf[offset + 1];
			pcond->devid = GET_CMD_FIELD(buf, offset + 2, uint16_t);
			pcond->id = GET_CMD_FIELD(buf, offset + 4, uint16_t);
			pcond->val = GET_CMD_FIELD(buf, offset + 6, uint16_t);
			offset += 8;

			hsb_debug("found a condition, %d-%d-%d-%d\n", pcond->devid, pcond->expr, pcond->id, pcond->val);
		}

		pact = &paction->acts[0];
		for (id = 0; id < paction->act_num; id++)
		{
			if (buf[offset] != 0xFD) {
				hsb_debug("invalid scene, action header %x\n", buf[offset]);
				return HSB_E_INVALID_MSG;
			}

			pact->flag = buf[offset + 1];
			pact->devid = GET_CMD_FIELD(buf, offset + 2, uint16_t);
			pact->id = GET_CMD_FIELD(buf, offset + 4, uint16_t);
			pact->param1 = GET_CMD_FIELD(buf, offset + 6, uint16_t);
			pact->param2 = GET_CMD_FIELD(buf, offset + 8, uint32_t);
			offset += 12;
			hsb_debug("found a act, %d-%d-%d-%d\n", pact->devid, pact->id, pact->param1, pact->param2);
			pact++;
		}

		paction++;
		action_cnt++;
		if (action_cnt == 8)
			break;
	}

	if (offset != len)
		hsb_debug("paser_scene error, offset=%d len=%d\n", offset, len);

	scene->act_num = action_cnt;

	return HSB_E_OK;
}

static int _reply_get_scene(uint8_t *buf, HSB_SCENE_T *scene)
{
	int offset = 0;
	int id = 0, index = 0;

	SET_CMD_FIELD(buf, 0, uint16_t, HSB_CMD_SCENE_UPDATE);
	strncpy(buf + 4, scene->name, HSB_SCENE_MAX_NAME_LEN);
	offset = 4 + HSB_SCENE_MAX_NAME_LEN;

	HSB_SCENE_ACTION_T *paction = &scene->actions[0];
	HSB_SCENE_CONDITION_T *pcond = NULL;
	HSB_SCENE_ACT_T *pact = NULL;

	for (id = 0; id < scene->act_num; id++)
	{
		buf[offset] = 0xFF;
		buf[offset + 1] = paction->delay;
		buf[offset + 2] = paction->has_cond ? 1 : 0;
		buf[offset + 3] = paction->act_num;
		offset += 4;

		if (paction->has_cond) {
			pcond = &paction->condition;
			buf[offset] = 0xFE;
			buf[offset + 1] = pcond->expr;
			SET_CMD_FIELD(buf, offset + 2, uint16_t, pcond->devid);
			SET_CMD_FIELD(buf, offset + 4, uint16_t, pcond->id);
			SET_CMD_FIELD(buf, offset + 6, uint16_t, pcond->val);
			offset += 8;
		}

		for (index = 0; index < paction->act_num; index++)
		{
			pact = &paction->acts[index];

			buf[offset] = 0xFD;
			buf[offset + 1] = pact->flag;

			SET_CMD_FIELD(buf, offset + 2, uint16_t, pact->devid);
			SET_CMD_FIELD(buf, offset + 4, uint16_t, pact->id);
			SET_CMD_FIELD(buf, offset + 6, uint16_t, pact->param1);
			SET_CMD_FIELD(buf, offset + 8, uint32_t, pact->param2);
			offset += 12;
		}

		paction++;
	}

	/* set length */
	SET_CMD_FIELD(buf, 2, uint16_t, offset);

	return offset;
}

static int _get_dev_status(uint8_t *buf, int len, HSB_STATUS_T *status)
{
	int id, total;

	total = (len - 8) / 4;

	if (total > 8) {
		hsb_debug("too many status %d\n", total);
		total = 8;
	}

	for (id = 0; id < total; id++) {
		status->id[id] = GET_CMD_FIELD(buf, 8 + id * 4, uint16_t);
		status->val[id] = GET_CMD_FIELD(buf, 10 + id * 4, uint16_t);
		//hsb_debug("_get_dev_status: %d=%d\n", status->id[id], status->val[id]);
	}

	status->num = total;

	return 0;
}

static int _make_online_event(uint8_t *buf, uint32_t devid)
{
	int ret, id;
	HSB_DEV_T dev;
	ret = get_dev_info(devid, &dev);

	int len = 60 + 4 * dev.status.num;

	MAKE_CMD_HDR(buf, HSB_CMD_DEV_ONLINE, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev.id); /* device id */
	SET_CMD_FIELD(buf, 8, uint32_t, dev.driver->id); /* driver id */
	SET_CMD_FIELD(buf, 12, uint16_t, dev.info.cls); /* device class */
	SET_CMD_FIELD(buf, 14, uint16_t, dev.info.interface); /* device interface */
	SET_CMD_FIELD(buf, 16, uint32_t, dev.info.dev_type);
	memcpy(buf + 20, dev.info.mac, 8); /* mac address */

	memcpy(buf + 28, dev.config.name, 16);
	memcpy(buf + 44, dev.config.location, 16);

	for (id = 0; id < dev.status.num; id++)
	{
		SET_CMD_FIELD(buf, 60 + id * 4, uint16_t, id);
		SET_CMD_FIELD(buf, 62 + id * 4, uint16_t, dev.status.val[id]);
	}


	return len;
}

static int _make_notify_resp(uint8_t *buf, HSB_RESP_T *resp)
{
	int len = 0;

	switch (resp->type) {
		case HSB_RESP_TYPE_EVENT:

			if (resp->u.event.id == HSB_EVT_TYPE_DEV_UPDATED &&
				((resp->u.event.param1 == HSB_DEV_UPDATED_TYPE_ONLINE) ||
				(resp->u.event.param1 == HSB_DEV_UPDATED_TYPE_NEW_ADD)))
			{
				return _make_online_event(buf, resp->u.event.devid);
			}


			len = 16;
			MAKE_CMD_HDR(buf, HSB_CMD_EVENT, len);
			SET_CMD_FIELD(buf, 4, uint32_t, resp->u.event.devid);
			SET_CMD_FIELD(buf, 8, uint16_t, resp->u.event.id);
			SET_CMD_FIELD(buf, 10, uint16_t, resp->u.event.param1);
			SET_CMD_FIELD(buf, 12, uint32_t, resp->u.event.param2);
			break;
		case HSB_RESP_TYPE_RESULT:
			len = 12;
			MAKE_CMD_HDR(buf, HSB_CMD_RESULT, len);
			SET_CMD_FIELD(buf, 4, uint32_t, resp->u.result.devid);
			SET_CMD_FIELD(buf, 8, uint16_t, resp->u.result.cmd);
			SET_CMD_FIELD(buf, 10, uint16_t, resp->u.result.ret_val);
			break;
		case HSB_RESP_TYPE_STATUS:
		case HSB_RESP_TYPE_STATUS_UPDATE:
		{
			HSB_STATUS_T *pstat = &resp->u.status;
			len = 8 + pstat->num * 4;
			if (resp->type == HSB_RESP_TYPE_STATUS)
				MAKE_CMD_HDR(buf, HSB_CMD_GET_STATUS_RESP, len);
			else
				MAKE_CMD_HDR(buf, HSB_CMD_STATUS_UPDATE, len);
				
			SET_CMD_FIELD(buf, 4, uint32_t, pstat->devid);

			int id;
			for (id = 0; id < pstat->num; id++) {
				SET_CMD_FIELD(buf, 8 + 4 * id, uint16_t, pstat->id[id]);
				SET_CMD_FIELD(buf, 10 + 4 * id, uint16_t, pstat->val[id]);
			}
		}
			break;
		default:
			hsb_debug("invalid resp %d\n", resp->type);
			break;
	}

	return len;
}

int deal_tcp_packet(int fd, uint8_t *buf, int len, void *reply, int *used)
{
	int ret = 0;
	int rlen = 0;
	uint8_t reply_buf[1024];
	uint16_t cmd, cmdlen;

	//hsb_debug("get tcp packet, len=%d\n", len);

	if (check_tcp_pkt_valid(buf, len)) {
		hsb_debug("tcp pkt invalid, len=%d\n", len);
		*used = len;
		return -1;
	}

	cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	cmdlen = GET_CMD_FIELD(buf, 2, uint16_t);
	memset(reply_buf, 0, sizeof(reply_buf));

	*used = cmdlen;
	//hsb_debug("cmd: %x, len: %d\n", cmd, cmdlen);

	switch (cmd) {
		case HSB_CMD_GET_DEVS:
		{
			uint32_t dev_id[128];
			int dev_num;
			ret = get_dev_id_list(dev_id, &dev_num);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret, 0, cmd);
			else
				rlen = _reply_dev_id_list(reply_buf, dev_id, dev_num);

			break;
		}
		case HSB_CMD_GET_INFO:
		{
			HSB_DEV_T dev;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			ret = get_dev_info(dev_id, &dev);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			else
				rlen = _reply_get_device_info(reply_buf, &dev);

			break;
		}
		case HSB_CMD_GET_CONFIG:
		{
			HSB_DEV_CONFIG_T cfg;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			ret = get_dev_cfg(dev_id, &cfg);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			else
				rlen = _reply_get_device_cfg(reply_buf, dev_id, &cfg);

			break;
		}
		case HSB_CMD_SET_CONFIG:
		{
			HSB_DEV_CONFIG_T cfg;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			memcpy(&cfg, buf + 8, sizeof(cfg));

			ret = set_dev_cfg(dev_id, &cfg);

			rlen = _reply_result(reply_buf, ret, dev_id, cmd);

			break;
		}
		case HSB_CMD_GET_STATUS:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			ret = get_dev_status_async(dev_id, reply);

			rlen = 0;

			break;
		}
		case HSB_CMD_SET_STATUS:
		{
			HSB_STATUS_T status = { 0 };
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			status.devid = dev_id;

			_get_dev_status(buf, len, &status);

			ret = set_dev_status_async(&status, reply);

			rlen = 0;

			break;
		}
		case HSB_CMD_SET_CHANNEL:
		{
			char name[HSB_CHANNEL_MAX_NAME_LEN];
			uint32_t cid, devid;

			devid = GET_CMD_FIELD(buf, 4, uint32_t);
			strncpy(name, buf + 8, sizeof(name));
			cid = GET_CMD_FIELD(buf, 8 + sizeof(name), uint32_t);

			ret = set_dev_channel(devid, name, cid);

			rlen = _reply_result(reply_buf, ret, devid, cmd);

			break;
		}
		case HSB_CMD_DEL_CHANNEL:
		{
			char name[HSB_CHANNEL_MAX_NAME_LEN];
			uint32_t devid;

			devid = GET_CMD_FIELD(buf, 4, uint32_t);
			strncpy(name, buf + 8, sizeof(name));

			ret = del_dev_channel(devid, name);

			rlen = _reply_result(reply_buf, ret, devid, cmd);

			break;
		}
		case HSB_CMD_SWITCH_CHANNEL:
		{
			char name[HSB_CHANNEL_MAX_NAME_LEN];
			uint32_t devid, cid;
			HSB_STATUS_T status = { 0 };

			devid = GET_CMD_FIELD(buf, 4, uint32_t);
			strncpy(name, buf + 8, sizeof(name));

			ret = get_dev_channel(devid, name, &cid);
			
			if (HSB_E_OK != ret) {
				rlen = _reply_result(reply_buf, ret, devid, cmd);
				break;
			}

			status.devid = devid;
			status.num = 1;
			status.id[0] = HSB_TV_STATUS_CHANNEL;
			status.val[0] = cid;

			ret = set_dev_status_async(&status, reply);

			rlen = 0;

			break;
		}
		case HSB_CMD_GET_CHANNEL:
		{
			uint32_t devid = GET_CMD_FIELD(buf, 4, uint32_t);

			uint32_t id, num = 0;
			ret = get_dev_channel_num(devid, &num);
			if (HSB_E_OK != ret) {
				rlen = _reply_result(reply_buf, ret, devid, cmd);
				break;
			}

			char name[HSB_CHANNEL_MAX_NAME_LEN];
			uint32_t cid;

			for (id = 0; id < num; id++) {
				memset(name, 0, sizeof(name));
				ret = get_dev_channel_by_id(devid, id, name, &cid);
				if (HSB_E_OK != ret)
					continue;

				rlen = _reply_get_device_channel(reply_buf, devid, name, cid);
				if (rlen > 0) {
					struct timeval tv = { 1, 0 };
					ret = write_timeout(fd, reply_buf, rlen, &tv);
				}
			}

			ret = HSB_E_OK;
			rlen = _reply_result(reply_buf, ret, devid, cmd);

			break;
		}
		case HSB_CMD_GET_TIMER:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_TIMER_T tm = { 0 };

			ret = get_dev_timer(dev_id, timer_id, &tm);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			else
				rlen = _reply_get_timer(reply_buf, dev_id, &tm);

			break;
		}
		case HSB_CMD_SET_TIMER:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_TIMER_T tm = { 0 };

			tm.id = timer_id;
			tm.work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			tm.flag = GET_CMD_FIELD(buf, 11, uint8_t);
			tm.hour = GET_CMD_FIELD(buf, 12, uint8_t);
			tm.min = GET_CMD_FIELD(buf, 13, uint8_t);
			tm.sec = GET_CMD_FIELD(buf, 14, uint8_t);
			tm.wday = GET_CMD_FIELD(buf, 15, uint8_t);
			int year, mon;
			year = GET_CMD_FIELD(buf, 16, uint16_t);
			mon = GET_CMD_FIELD(buf, 18, uint8_t);

			hsb_debug("set year %d mon %d\n", year, mon);

			if (year > 1900) {
				year -= 1900;
				mon -= 1;
			}

			tm.year = year;
			tm.mon = mon;


			hsb_debug("tm->year %d tm->mon %d\n", tm.year, tm.mon);

			tm.mday = GET_CMD_FIELD(buf, 19, uint8_t);
			tm.act_id = GET_CMD_FIELD(buf, 20, uint16_t);
			tm.act_param1 = GET_CMD_FIELD(buf, 22, uint16_t);
			tm.act_param2 = GET_CMD_FIELD(buf, 24, uint32_t);

			hsb_debug("set timer %d, %d/%d/%d %d:%d:%d\n", timer_id,
				tm.year, tm.mon, tm.mday, tm.hour, tm.min, tm.sec);

			hsb_debug("devid %d, actid %d, param1 %d\n", dev_id, tm.act_id, tm.act_param1);

			ret = set_dev_timer(dev_id, &tm);
			if (ret)
				hsb_debug("set_dev_timer ret=%d\n", ret);

			rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			break;
		}
		case HSB_CMD_DEL_TIMER:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);

			ret = del_dev_timer(dev_id, timer_id);
			rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			break;
		}
		case HSB_CMD_GET_DELAY:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_DELAY_T delay = { 0 };

			ret = get_dev_delay(dev_id, delay_id, &delay);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			else
				rlen = _reply_get_delay(reply_buf, dev_id, &delay);

			break;
		}
		case HSB_CMD_SET_DELAY:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_DELAY_T delay = { 0 };

			delay.id = delay_id;
			delay.work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			delay.flag = GET_CMD_FIELD(buf, 11, uint8_t);
			delay.evt_id = GET_CMD_FIELD(buf, 12, uint16_t);
			delay.evt_param1 = GET_CMD_FIELD(buf, 14, uint16_t);
			delay.evt_param2 = GET_CMD_FIELD(buf, 16, uint32_t);
			delay.act_id = GET_CMD_FIELD(buf, 20, uint16_t);
			delay.act_param1 = GET_CMD_FIELD(buf, 22, uint16_t);
			delay.act_param2 = GET_CMD_FIELD(buf, 24, uint32_t);
			delay.delay_sec = GET_CMD_FIELD(buf, 28, uint32_t);

			ret = set_dev_delay(dev_id, &delay);
			rlen = _reply_result(reply_buf, ret, dev_id, cmd);

			break;
		}
		case HSB_CMD_DEL_DELAY:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);

			ret = del_dev_delay(dev_id, delay_id);
			rlen = _reply_result(reply_buf, ret, dev_id, cmd);

			break;
		}
		case HSB_CMD_GET_LINKAGE:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_LINKAGE_T link = { 0 };

			ret = get_dev_linkage(dev_id, link_id, &link);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			else
				rlen = _reply_get_linkage(reply_buf, dev_id, &link);

			break;
		}
		case HSB_CMD_SET_LINKAGE:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_LINKAGE_T link = { 0 };

			link.id = link_id;
			link.work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			link.flag = GET_CMD_FIELD(buf, 11, uint8_t);
			link.evt_id = GET_CMD_FIELD(buf, 12, uint16_t);
			link.evt_param1 = GET_CMD_FIELD(buf, 14, uint16_t);
			link.evt_param2 = GET_CMD_FIELD(buf, 16, uint32_t);
			link.act_devid = GET_CMD_FIELD(buf, 20, uint32_t);
			link.act_id = GET_CMD_FIELD(buf, 24, uint16_t);
			link.act_param1 = GET_CMD_FIELD(buf, 26, uint16_t);
			link.act_param2 = GET_CMD_FIELD(buf, 28, uint32_t);

			ret = set_dev_linkage(dev_id, &link);
			rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			break;
		}
		case HSB_CMD_DEL_LINKAGE:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);

			ret = del_dev_linkage(dev_id, link_id);
			rlen = _reply_result(reply_buf, ret, dev_id, cmd);
			break;
		}
		case HSB_CMD_DO_ACTION:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint16_t param1 = GET_CMD_FIELD(buf, 10, uint16_t);
			uint32_t param2 = GET_CMD_FIELD(buf, 12, uint32_t);
			HSB_ACTION_T act = { 0 };

			/* TODO */
			act.devid = dev_id;
			act.id = act_id;
			act.param1 = param1;
			act.param2 = param2;

			ret = set_dev_action_async(&act, reply);

			rlen = 0;
 	
			break;
		}
		case HSB_CMD_PROBE_DEV:
		{
			uint16_t drvid = GET_CMD_FIELD(buf, 4, uint16_t);
			HSB_PROBE_T probe;

			probe.drvid = drvid;

			hsb_debug("probe\n");

			ret = probe_dev_async(&probe, reply);

			rlen = 0;

			break;
		}
		case HSB_CMD_ADD_DEV:
		{
			uint16_t drvid = GET_CMD_FIELD(buf, 4, uint16_t);
			uint16_t dev_type = GET_CMD_FIELD(buf, 6, uint16_t);
			HSB_DEV_CONFIG_T cfg;

			memcpy(&cfg, buf + 8, sizeof(cfg));

			hsb_debug("add_dev: %d,%d\n", drvid, dev_type);

			ret = add_dev(drvid, dev_type, &cfg);

			rlen = _reply_result(reply_buf, ret, 0, cmd);

			break;
		}
		case HSB_CMD_DEL_DEV:
		{
			uint32_t devid = GET_CMD_FIELD(buf, 4, uint32_t);

			ret = del_dev(devid);

			rlen = _reply_result(reply_buf, ret, 0, cmd);
			
			break;
		}
		case HSB_CMD_SET_SCENE:
		{
			/* alloc_scene */
			HSB_SCENE_T *scene = alloc_scene();
			if (!scene) {
				ret = HSB_E_NO_MEMORY;
				rlen = _reply_result(reply_buf, ret, 0, cmd);
				break;
			}

			/* parse_scene */
			ret = parse_scene(scene, buf, cmdlen);
			if (HSB_E_OK != ret) {
				rlen = _reply_result(reply_buf, ret, 0, cmd);
				break;
			}

			hsb_debug("add scene [%s]\n", scene->name);

			/* add_scene */
			ret = add_scene(scene);
			hsb_debug("add scene [%s] success\n", scene->name);
			rlen = _reply_result(reply_buf, ret, 0, cmd);

			break;
		}
		case HSB_CMD_DEL_SCENE:
		{
			char name[HSB_SCENE_MAX_NAME_LEN];

			strncpy(name, buf + 4, sizeof(name));
			hsb_debug("del scene [%s]\n", name);

			ret = del_scene(name);

			rlen = _reply_result(reply_buf, ret, 0, cmd);

			break;
		}
		case HSB_CMD_ENTER_SCENE:
		{
			char name[HSB_SCENE_MAX_NAME_LEN];

			strncpy(name, buf + 4, sizeof(name));
			hsb_debug("enter scene [%s]\n", name);

			ret = enter_scene(name);

			rlen = _reply_result(reply_buf, ret, 0, cmd);

			break;
		}
		case HSB_CMD_GET_SCENE:
		{
			uint32_t id, num = 0;

			hsb_debug("get scene\n");
			ret = get_scene_num(&num);
			if (HSB_E_OK != ret) {
				hsb_debug("get scene num fail\n");
				rlen = _reply_result(reply_buf, ret, 0, cmd);
				break;
			}

			HSB_SCENE_T *scene = NULL;

			for (id = 0; id < num; id++) {
				ret = get_scene(id, &scene);
				if (HSB_E_OK != ret)
					continue;

				rlen = _reply_get_scene(reply_buf, scene);
				if (rlen > 0) {
					struct timeval tv = { 1, 0 };
					ret = write_timeout(fd, reply_buf, rlen, &tv);
				}
			}

			ret = HSB_E_OK;
			rlen = _reply_result(reply_buf, ret, 0, cmd);

			break;
		}
		default:
			rlen = _reply_result(reply_buf, REPLY_FAIL, 0, cmd);
			break;
	}

	if (rlen < 0) {
		hsb_debug("rlen %d<0\n", rlen);
		return -2;
	}

	if (rlen > 0) {
		struct timeval tv = { 1, 0 };
		ret = write_timeout(fd, reply_buf, rlen, &tv);
	}

	return 0;
}

#define UDP_CMD_VALID(x)	(x == HSB_CMD_BOX_DISCOVER || x == HSB_CMD_BOX_DISCOVER_RESP)

static int check_udp_pkt_valid(uint8_t *buf, int len)
{
	if (!buf || len <= 0)
		return -1;

	uint16_t command = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t length = GET_CMD_FIELD(buf, 2, uint16_t);

	if (!UDP_CMD_VALID(command))
		return -2;

	if (len != length)
		return -3;

	return 0;
}

static int _reply_box_discover(uint8_t *buf, uint16_t bid)
{
	int len = 8;

	MAKE_CMD_HDR(buf, HSB_CMD_BOX_DISCOVER_RESP, len);

	SET_CMD_FIELD(buf, 4, uint8_t, 0);	/* Minor Version */
	SET_CMD_FIELD(buf, 5, uint8_t, 1);	/* Major Version */

	SET_CMD_FIELD(buf, 6, uint16_t, bid);

	return len;
}

static int deal_udp_packet(int fd, uint8_t *buf, int len, struct sockaddr *cliaddr, socklen_t slen)
{
	int rlen = 0;
	uint8_t reply_buf[32];
	uint32_t bid = 1;

	if (check_udp_pkt_valid(buf, len)) {
		hsb_critical("get invalid udp pkt\n");
		return -1;
	}

	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	memset(reply_buf, 0, sizeof(reply_buf));

	switch (cmd) {
		case HSB_CMD_BOX_DISCOVER:
		{
			uint16_t uid = GET_CMD_FIELD(buf, 6, uint16_t);
			rlen = _reply_box_discover(reply_buf, bid);
		}
		default:
			break;
	}

	if (rlen > 0) {
		sendto(fd, reply_buf, rlen, 0, (const struct sockaddr *)cliaddr, slen);
	}

	return 0;
}

static void *udp_listen_thread(void *arg)
{
	struct sockaddr_in cliaddr;
	int sockfd = open_udp_listenfd(CORE_UDP_LISTEN_PORT);
	if (sockfd < 0)
		return NULL;

	int nread;
	socklen_t len;
	uint8_t buf[1024];

	while (1) {
		len = sizeof(cliaddr);
		nread = recvfrom(sockfd,
				buf,
				sizeof(buf),
				0,
				(struct sockaddr *)&cliaddr,
				&len);

		deal_udp_packet(sockfd, buf, nread, (struct sockaddr *)&cliaddr, len);
	}

	return NULL;
}

/* tcp client pool */

typedef struct {
	int tcp_sockfd;
	int un_sockfd;
	char listen_path[64];
	GMutex mutex;
	GQueue queue;
	int using;
} tcp_client_context;

typedef struct {
	tcp_client_context	context[MAX_TCP_CLIENT_NUM];
	GMutex			mutex;

	GThreadPool		*pool;
} tcp_client_pool;

static tcp_client_pool	client_pool;

static void tcp_client_handler(gpointer data, gpointer user_data);

static int init_client_pool(void)
{
	int cnt;
	tcp_client_context *pctx = NULL;
	const char *work_dir = get_work_dir();

	memset(&client_pool, 0, sizeof(client_pool));

	for (cnt = 0; cnt < MAX_TCP_CLIENT_NUM; cnt++) {
		pctx = &client_pool.context[cnt];

		g_mutex_init(&pctx->mutex);
		g_queue_init(&pctx->queue);

		snprintf(pctx->listen_path,
			sizeof(pctx->listen_path),
			"%score_client_%d.listen",
			work_dir,
			cnt);

		//pctx->un_sockfd = unix_socket_new_listen((const char *)pctx->listen_path);
	}

	g_mutex_init(&client_pool.mutex);
	
	GError *error = NULL;

	client_pool.pool = g_thread_pool_new(tcp_client_handler, NULL, 5, TRUE, &error);

	return 0;
}

static int release_client_pool(void)
{
	// TODO
	return 0;
}

static tcp_client_context *get_client_context(int sockfd)
{
	int cnt;
	tcp_client_context *pctx;

	g_mutex_lock(&client_pool.mutex);

	for (cnt = 0; cnt < MAX_TCP_CLIENT_NUM; cnt++) {
		pctx = &client_pool.context[cnt];
		if (!pctx->using)
			break;
	}

	if (cnt == MAX_TCP_CLIENT_NUM) {
		g_mutex_unlock(&client_pool.mutex);
		return NULL;
	}

	pctx->using = 1;
	pctx->tcp_sockfd = sockfd;
	pctx->un_sockfd = unix_socket_new_listen((const char *)pctx->listen_path);

	g_mutex_unlock(&client_pool.mutex);

	return pctx;
}

static int put_client_context(tcp_client_context *pctx)
{
	
	g_mutex_lock(&client_pool.mutex);

	pctx->using = 0;

	// TODO: clean queue
	unix_socket_free(pctx->un_sockfd);

	g_mutex_unlock(&client_pool.mutex);

	return 0;
}

static void *tcp_listen_thread(void *arg)
{
	fd_set readset;
        int ret, sockfd = 0;
        int listenfd = open_tcp_listenfd(CORE_TCP_LISTEN_PORT);
        if (listenfd <= 0) {
                hsb_critical("open listen fd error\n");
                return NULL;
        }

        signal(SIGPIPE, SIG_IGN);
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);

	tcp_client_context *pctx = NULL;

	while (1) {
		bzero(&addr, addrlen);
		sockfd = accept(listenfd, &addr, &addrlen);
		if (sockfd < 0) {
			hsb_critical("accept error\n");
			close(listenfd);
			return NULL;
		}

		hsb_debug("get a client\n");
		pctx = get_client_context(sockfd);
		if (!pctx) {
			hsb_critical("can't get client context\n");
			close(sockfd);
			continue;
		}

		g_thread_pool_push(client_pool.pool, (gpointer)pctx, NULL);

		usleep(1000000);
		report_all_device(pctx);
	}

	hsb_critical("tcp listen thread closed\n");
	close(listenfd);
	return NULL;
}

static int _process_notify(int fd, tcp_client_context *pctx)
{
	HSB_RESP_T *resp = NULL;
	uint8_t buf[128];
	int ret, len;

	while (!g_queue_is_empty(&pctx->queue)) {
		resp = g_queue_pop_head(&pctx->queue);

		memset(buf, 0, sizeof(buf));

		len = _make_notify_resp(buf, resp);
		g_slice_free(HSB_RESP_T, resp);

		ret = write(fd, buf, len);
		if (ret <= 0)
			return ret;
	}

	return 0;
}

static void tcp_client_handler(gpointer data, gpointer user_data)
{
	tcp_client_context *pctx = (tcp_client_context *)data;
	fd_set readset;
	int ret, nread;
	int sockfd = pctx->tcp_sockfd;
	int unfd = pctx->un_sockfd;
	int maxfd = (sockfd > unfd) ? sockfd : unfd;
	uint8_t msg[1024];

        signal(SIGPIPE, SIG_IGN);

	while (1) {
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		FD_SET(unfd, &readset);

		ret = select(maxfd+1, &readset, NULL, NULL, NULL);

		if (ret <= 0) {
			hsb_critical("select error\n");
			break;
		}

		if (FD_ISSET(sockfd, &readset)) {
			nread = read(sockfd, msg, sizeof(msg));	
			if (nread <= 0) {
				break;
			}

			int tmp, nwrite = 0;
			while (nwrite < nread) {
				deal_tcp_packet(sockfd, msg + nwrite, nread - nwrite, pctx, &tmp);

				nwrite += tmp;
			}
		}

		if (FD_ISSET(unfd, &readset)) {
			nread = recvfrom(unfd, msg, sizeof(msg), 0, NULL, NULL);
			//hsb_debug("get notify\n");

			_process_notify(sockfd, pctx);
		}
	}

	close(sockfd);

	put_client_context(pctx);
	hsb_debug("put a client\n");
}

#define NOTIFY_MESSAGE	"notify"

int notify_resp(HSB_RESP_T *msg, void *data)
{
	int cnt, fd;
	HSB_RESP_T *notify = NULL;
	tcp_client_context *pctx = NULL;

	if (msg->reply)
		pctx = (tcp_client_context *)msg->reply;
	else if (data)
		pctx = (tcp_client_context *)data;

	if (pctx) {
		if (!pctx->using)
			return HSB_E_OK;

		notify = g_slice_dup(HSB_RESP_T, msg);
		if (!notify) {
			hsb_debug("no memory\n");
			return HSB_E_NO_MEMORY;
		}

		g_mutex_lock(&pctx->mutex);

		g_queue_push_tail(&pctx->queue, notify);

		g_mutex_unlock(&pctx->mutex);

		fd = unix_socket_new();
		unix_socket_send_to(fd, pctx->listen_path, NOTIFY_MESSAGE, strlen(NOTIFY_MESSAGE));
		unix_socket_free(fd);

		return HSB_E_OK;
	}


	for (cnt = 0; cnt < MAX_TCP_CLIENT_NUM; cnt++) {
		pctx = &client_pool.context[cnt];
		if (!pctx->using)
			continue;

		if (msg->reply && msg->reply != (void *)pctx)
			continue;

		notify = g_slice_dup(HSB_RESP_T, msg);
		if (!notify) {
			hsb_debug("no memory\n");
			continue;
		}

		g_mutex_lock(&pctx->mutex);

		g_queue_push_tail(&pctx->queue, notify);

		g_mutex_unlock(&pctx->mutex);

		fd = unix_socket_new();
		unix_socket_send_to(fd, pctx->listen_path, NOTIFY_MESSAGE, strlen(NOTIFY_MESSAGE));
		unix_socket_free(fd);
	}

	return HSB_E_OK;
}

int init_network_module(void)
{
	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)udp_listen_thread, NULL))
	{
		hsb_critical("create udp listen thread failed\n");
		return -1;
	}

	init_client_pool();

	if (pthread_create(&thread_id, NULL, (thread_entry_func)tcp_listen_thread, NULL))
	{
		hsb_critical("create tcp listen thread failed\n");
		return -2;
	}

	return 0;
}



