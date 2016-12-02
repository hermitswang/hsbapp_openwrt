

#include <string.h>
#include <glib.h>
#include "device.h"
#include "network_utils.h"
#include "debug.h"
#include "driver_virtual_switch.h"
#include "thread_utils.h"
#include "device.h"
#include "hsb_error.h"

typedef struct {
	GQueue	queue;
	GMutex	mutex;
} HSB_DEVICE_DRIVER_CONTEXT_T;

typedef struct {
	uint16_t	dev_class;
	uint16_t	interface;
	uint32_t	dev_type;
	uint8_t		mac[6];
	uint8_t		resv[2];
	uint16_t	status_num;
	uint16_t	status[8];
} VS_INFO_T;

typedef enum {
	VS_EVT_TYPE_SENSOR_TRIGGERED = 0,
	VS_EVT_TYPE_SENSOR_RECOVERED,
	VS_EVT_TYPE_KEY_PRESSED,
	VS_EVT_TYPE_IR_KEY,
} VS_EVT_TYPE_T;

typedef enum {
	VS_STATUS_TYPE_ON_OFF = 1,
	VS_STATUS_TYPE_RO_DOOR = 16,
} VS_STATUS_TYPE_T;

typedef struct {
	uint32_t	id;
	struct in_addr	ip;
	uint32_t	idle_time;
	VS_INFO_T	info;
} VS_DEV_T;

static HSB_DEVICE_DRIVER_CONTEXT_T gl_ctx = { 0 };

static HSB_DEV_DRV_T virtual_switch_drv;
static int _register_device(struct in_addr *addr, VS_INFO_T *info);

static VS_DEV_T *_find_dev_by_ip(struct in_addr *addr)
{
	GQueue *queue = &gl_ctx.queue;
	int len = g_queue_get_length(queue);
	int id;
	VS_DEV_T *pdev;

	for (id = 0; id < len; id++) {
		pdev = g_queue_peek_nth(queue, id);

		if (pdev->ip.s_addr == addr->s_addr)
			return pdev;
	}

	return NULL;
}

static VS_DEV_T *_find_dev_by_mac(uint8_t *mac)
{
	guint len, id;
	GQueue *queue = &gl_ctx.queue;
	VS_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (VS_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (0 == memcmp(pdev->info.mac, mac, 6))
			return pdev;
	}

	return NULL;
}

static VS_DEV_T *_find_dev_by_id(uint32_t devid)
{
	guint len, id;
	GQueue *queue = &gl_ctx.queue;
	VS_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (VS_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (devid == pdev->id)
			return pdev;
	}

	return NULL;
}

static int virtual_switch_probe(void)
{
	struct sockaddr_in servaddr, devaddr;
	socklen_t slen = sizeof(servaddr);
	socklen_t devlen = sizeof(devaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count = 0;

	//hsb_debug("probe virtual switch\n");

	/* 1.create udp socket */
	int sockfd = open_udp_clientfd();

	/* 2.send broadcast packet, 192.168.2.255:19001 */
	if (get_broadcast_address(sockfd, &servaddr.sin_addr)) {
		close(sockfd);
		return -1;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(VIRTUAL_SWITCH_LISTEN_PORT);

	set_broadcast(sockfd, true);

	memset(sbuf, 0, sizeof(sbuf));
	count = 8;
	SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_DEVICE_DISCOVER);
	SET_CMD_FIELD(sbuf, 2, uint16_t, count);

	sendto(sockfd, sbuf, count, 0, (struct sockaddr *)&servaddr, slen);

	/* 3.wait for response in 1 second */
	struct timeval tv = { 3, 0 };
	
	do {
		count = 28;
		ret = recvfrom_timeout(sockfd, rbuf, sizeof(rbuf), (struct sockaddr *)&devaddr, &devlen, &tv);
		if (ret > 0 && ret < count) {
			hsb_critical("probe: get err pkt, len=%d\n", ret);
			continue;
		} else if (0 == ret)
			break;

		int cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
		int len = GET_CMD_FIELD(rbuf, 2, uint16_t);

		if (cmd != VS_CMD_DEVICE_DISCOVER_RESP || len < count) {
			hsb_critical("probe: get err pkt, cmd=%x, len=%d\n", cmd, len);
			continue;
		}

		VS_INFO_T info = { 0 };
		memcpy(&info, rbuf + 8, len - 8);

		_register_device(&devaddr.sin_addr, &info);

	} while (tv.tv_sec > 0 && tv.tv_usec > 0);

	close(sockfd);
	//hsb_debug("probe virtual switch done\n");

	return 0;
}

static int _transfer(struct in_addr *addr, uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen)
{
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	int ret;

	int sockfd = open_udp_clientfd();

	make_sockaddr(&servaddr, addr, VIRTUAL_SWITCH_LISTEN_PORT);

	sendto(sockfd, wbuf, wlen, 0, (struct sockaddr *)&servaddr, slen);

	struct timeval tv = { 3, 0 };
	ret = recvfrom_timeout(sockfd, rbuf, rlen, NULL, NULL, &tv);

	if (ret <= 0) {
		hsb_critical("_transfer: get err pkt, len=%d\n", ret);
		close(sockfd);
		return -1;
	}

	close(sockfd);

	return ret;
}

static int virtual_switch_set_status(const HSB_STATUS_T *status)
{
	int ret, len;
	uint8_t wbuf[64];
	uint8_t rbuf[64];
	HSB_STATUS_T *pstat = (HSB_STATUS_T *)status;

	VS_DEV_T *pdev = _find_dev_by_id(pstat->devid);
	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	len = 4 + 4 * pstat->num;
	memset(wbuf, 0, sizeof(wbuf));
	SET_CMD_FIELD(wbuf, 0, uint16_t, VS_CMD_SET_STATUS);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len);

	int id;
	for (id = 0; id < pstat->num; id++) {
		SET_CMD_FIELD(wbuf, 4 + id * 4, uint16_t, pstat->id[id]);
		SET_CMD_FIELD(wbuf, 6 + id * 4, uint16_t, pstat->val[id]);
	}

	ret = _transfer(&pdev->ip, wbuf, len, rbuf, sizeof(rbuf));

	if (ret < MIN_VS_CMD_LEN) {
		hsb_critical("set status: get err pkt, len=%d\n", ret);
		return HSB_E_INVALID_MSG;
	}

	uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(rbuf, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(rbuf, 4, uint16_t);

	if (cmd != VS_CMD_RESULT || rlen != ret) {
		hsb_critical("set status: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
		return HSB_E_INVALID_MSG;
	}

	if (!result) {
		hsb_debug("set status result=%d\n", result);
		return HSB_E_ACT_FAILED;
	}

	return HSB_E_OK;
}

static int virtual_switch_get_status(HSB_STATUS_T *status)
{
	uint8_t wbuf[64], rbuf[64];
	int ret, len;

	VS_DEV_T *pdev = _find_dev_by_id(status->devid);
	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	memset(wbuf, 0, sizeof(wbuf));
	len = 8;
	SET_CMD_FIELD(wbuf, 0, uint16_t, VS_CMD_GET_STATUS);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len);
	SET_CMD_FIELD(wbuf, 4, uint32_t, 0xffffffff);

	ret = _transfer(&pdev->ip, wbuf, len, rbuf, sizeof(rbuf));

	if (ret < MIN_VS_CMD_LEN) {
		hsb_critical("get status: get err pkt, len=%d\n", ret);
		return HSB_E_INVALID_MSG;
	}

	uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(rbuf, 2, uint16_t);

	if (cmd != VS_CMD_GET_STATUS_RESP || ret != rlen) {
		hsb_critical("get status: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
		return HSB_E_INVALID_MSG;
	}

	int status_num = 0;
	uint16_t id, val;
	len = 4;

	while (len + 4 <= rlen) {
		status->id[status_num] = GET_CMD_FIELD(rbuf, len, uint16_t);
		status->val[status_num] = GET_CMD_FIELD(rbuf, len + 2, uint16_t);

		status_num ++;
		len += 4;

		if (status_num >= status->num)
			break;
	}

	status->num = status_num;

	return HSB_E_OK;
}

static int virtual_switch_set_action(const HSB_ACTION_T *act)
{
	int ret, len;
	uint8_t wbuf[64];
	uint8_t rbuf[64];

	VS_DEV_T *pdev = _find_dev_by_id(act->devid);
	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	len = 12;
	memset(wbuf, 0, sizeof(wbuf));
	SET_CMD_FIELD(wbuf, 0, uint16_t, VS_CMD_DO_ACTION);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len);
	SET_CMD_FIELD(wbuf, 4, uint16_t, act->id);
	SET_CMD_FIELD(wbuf, 6, uint16_t, act->param1);
	SET_CMD_FIELD(wbuf, 8, uint32_t, act->param2);

	ret = _transfer(&pdev->ip, wbuf, len, rbuf, sizeof(rbuf));

	if (ret < MIN_VS_CMD_LEN) {
		hsb_critical("set action: get err pkt, len=%d\n", ret);
		return HSB_E_INVALID_MSG;
	}

	uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(rbuf, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(rbuf, 4, uint16_t);

	if (cmd != VS_CMD_RESULT || rlen != ret) {
		hsb_critical("set action: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
		return HSB_E_INVALID_MSG;
	}

	if (!result) {
		hsb_debug("set action result=%d\n", result);
		return HSB_E_ACT_FAILED;
	}

	return HSB_E_OK;
}

static HSB_DEV_OP_T virtual_switch_op = {
	virtual_switch_get_status,
	virtual_switch_set_status,
	virtual_switch_set_action,
	NULL,
	NULL,
};

static HSB_DEV_DRV_OP_T virtual_switch_drv_op = {
	virtual_switch_probe,
	NULL,
	NULL,
	NULL,
};

static HSB_DEV_DRV_T virtual_switch_drv = {
	"virtual switch",
	HSB_DRV_ID_CJ_WIFI,
	&virtual_switch_drv_op,
};

static int _remove_timeout_dev(void)
{
	GQueue *queue = &gl_ctx.queue;
	int len = g_queue_get_length(queue);
	int id;
	VS_DEV_T *pdev;

	for (id = 0; id < len; ) {
		pdev = g_queue_peek_nth(queue, id);
		if (!pdev)
			break;

		if (pdev->idle_time > 10) {
			dev_offline(pdev->id);

			g_queue_pop_nth(queue, id);
			//hsb_debug("device offline %s\n", inet_ntoa(pdev->ip));
			g_slice_free(VS_DEV_T, pdev);

			continue;
		}

		pdev->idle_time++;

		id++;
	}

	return 0;
}


static int _register_device(struct in_addr *addr, VS_INFO_T *info)
{
	GQueue *queue = &gl_ctx.queue;
	VS_DEV_T *pdev = _find_dev_by_mac(info->mac);
	uint32_t devid;
	int id;

	if (pdev)
		return HSB_E_OK;

	HSB_DEV_STATUS_T status = { 0 };
	status.num = info->status_num;
	for (id = 0; id < info->status_num; id++)
	{
		status.val[id] = info->status[id];
	}

	HSB_DEV_INFO_T dev_info = { 0 };
	dev_info.cls = info->dev_class;
	dev_info.interface = info->interface;
	dev_info.dev_type = info->dev_type;
	memcpy(dev_info.mac, info->mac, 6);

	int ret = dev_online(virtual_switch_drv.id,
				&dev_info,
				&status,
				&virtual_switch_op,
				NULL,
				false,
				NULL,
				&devid);
	if (HSB_E_OK != ret)
		return ret;

	pdev = g_slice_new0(VS_DEV_T);
	if (!pdev) {
		hsb_critical("create vs dev fail\n");
		return HSB_E_NO_MEMORY;
	}

	memcpy(&pdev->ip, addr, sizeof(struct in_addr));
	memcpy(&pdev->info, info, sizeof(*info));
	pdev->id = devid;

	g_queue_push_tail(queue, pdev);

	return 0;
}

static int _refresh_device(VS_DEV_T *pdev)
{
	pdev->idle_time = 0;

	return 0;
}

static int _status_updated(uint32_t devid, uint16_t id, uint16_t val)
{
	HSB_STATUS_T status;
	status.devid = devid;
	status.num = 1;
	status.id[0] = id;
	status.val[0] = val;

	return dev_status_updated(devid, &status);
}

static int _key(uint16_t key)
{
	int ret = HSB_E_OK;

	switch (key) {
		case HSB_WORK_MODE_HOME:
		case HSB_WORK_MODE_OUTSIDE:
		case HSB_WORK_MODE_GUARD:
			ret = set_box_work_mode(key);
			break;
		default:
			hsb_debug("get undefined key %d\n", key);
			ret = HSB_E_OTHERS;
			break;
	}

	return ret;
}

static int _ir_key(uint32_t devid, uint16_t param, uint32_t param2)
{
	return dev_ir_key(devid, param, param2);
}

static int _event(uint32_t devid, uint16_t id, uint16_t param, uint32_t param2)
{
	int ret = HSB_E_OK;

	switch (id) {
		case VS_EVT_TYPE_SENSOR_TRIGGERED:
			ret = dev_sensor_triggered(devid, param);
			break;
		case VS_EVT_TYPE_SENSOR_RECOVERED:
			ret = dev_sensor_recovered(devid, param);
			break;
		case VS_EVT_TYPE_KEY_PRESSED:
			ret = _key(param);
			break;
		case VS_EVT_TYPE_IR_KEY:
			ret = _ir_key(devid, param, param2);
			break;
		default:
			hsb_debug("unknown event %x\n", id);
			ret = HSB_E_OTHERS;
			break;
	}

	return ret;
}

static void *_monitor_thread(void *arg)
{
	int fd = open_udp_listenfd(VIRTUAL_SWITCH_BOX_LISTEN_PORT);
	fd_set rset;
	int ret;
	struct timeval tv;
	struct sockaddr_in mc_addr, dev_addr;
	socklen_t mc_len = sizeof(mc_addr);
	socklen_t dev_len = sizeof(dev_addr);
	uint8_t sbuf[16], rbuf[16];
	int cmd_len;

	set_broadcast(fd, true);

	get_broadcast_address(fd, &mc_addr.sin_addr);
	mc_addr.sin_family = AF_INET;
	mc_addr.sin_port = htons(VIRTUAL_SWITCH_LISTEN_PORT);

	while (1) {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		ret = select(fd+1, &rset, NULL, NULL, &tv);

		if (ret < 0)
			continue;

		if (0 == ret) {	/* timeout, send keep alive */

			_remove_timeout_dev();

			memset(sbuf, 0, sizeof(sbuf));

			cmd_len = 4;
			SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_KEEP_ALIVE);
			SET_CMD_FIELD(sbuf, 2, uint16_t, cmd_len);
			
			sendto(fd, sbuf, cmd_len, 0, (struct sockaddr *)&mc_addr, mc_len);
			continue;
		}

		cmd_len = 16;
		/* get message */
		dev_len = sizeof(struct sockaddr_in);
		ret = recvfrom(fd, rbuf, cmd_len, 0, (struct sockaddr *)&dev_addr, &dev_len);

		if (ret < 4)
			continue;

		uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
		uint16_t len = GET_CMD_FIELD(rbuf, 2, uint16_t);

		if (len != ret) {
			hsb_debug("error cmd: %d, %d\n", len, ret);
			continue;
		}

		//hsb_debug("get a cmd: %x\n", cmd);

		VS_DEV_T *pdev = _find_dev_by_ip(&dev_addr.sin_addr);
		if (!pdev) {
			probe_dev(virtual_switch_drv.id);
			continue;
		}

		switch (cmd) {
			case VS_CMD_KEEP_ALIVE:
			{
				break;
			}
			case VS_CMD_STATUS_CHANGED:
			{
				uint16_t id = GET_CMD_FIELD(rbuf, 4, uint16_t);
				uint16_t val = GET_CMD_FIELD(rbuf, 6, uint16_t);

				_status_updated(pdev->id, id, val);
				break;
			}
			case VS_CMD_EVENT:
			{
				uint16_t id = GET_CMD_FIELD(rbuf, 4, uint16_t);
				uint16_t param = GET_CMD_FIELD(rbuf, 6, uint16_t);
				uint32_t param2 = GET_CMD_FIELD(rbuf, 8, uint32_t);

				_event(pdev->id, id, param, param2);
				break;
			}
			default:
				break;
		}
	
		_refresh_device(pdev);
	}

	return NULL;
}

int init_virtual_switch_drv(void)
{
	g_queue_init(&gl_ctx.queue);
	g_mutex_init(&gl_ctx.mutex);

	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)_monitor_thread, NULL))
		return -1;

	return register_dev_drv(&virtual_switch_drv);
}

