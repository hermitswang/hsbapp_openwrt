

#include <string.h>
#include <glib.h>
#include <pthread.h>
#include <errno.h>
#include "device.h"
#include "network_utils.h"
#include "debug.h"
#include "driver_cg_zigbee.h"
#include "thread_utils.h"
#include "device.h"
#include "hsb_error.h"
#include "hsb_config.h"
#include "utils.h"

//#define CZ_TEST

#define COND_LOCK()	do { \
	pthread_mutex_lock(&gl_ctx.cond_mutex); \
} while (0)

#define COND_UNLOCK()	do { \
	pthread_mutex_unlock(&gl_ctx.cond_mutex); \
} while (0)

#define CZ_TIMEOUT		(5)
#define CZ_HEADER_MAGIC		(0x55AA)
#define CZ_HEADER_LEN		(12)
#define CZ_DEFAULT_EP		(10)

typedef struct {
	GQueue	queue;
	GMutex	mutex;

	uint8_t			listen_path[64];
	int	fd;

	pthread_cond_t		condition;
	gboolean		is_waiting;
	uint8_t			send_buf[256];
	uint8_t			recv_buf[256];
	int			send_len;
	int			recv_len;
	uint16_t		trans_id;
	pthread_mutex_t		cond_mutex;
	
} HSB_DEVICE_DRIVER_CONTEXT_T;

typedef struct {
	uint32_t	dev_type;
	uint8_t		mac[8];
	uint16_t	status_num;
	uint16_t	status[8];
} CZ_INFO_T;

typedef enum {
	CZ_EVT_TYPE_SENSOR_TRIGGERED = 0,
	CZ_EVT_TYPE_SENSOR_RECOVERED,
	CZ_EVT_TYPE_KEY_PRESSED,
	CZ_EVT_TYPE_IR_KEY,
} CZ_EVT_TYPE_T;

typedef enum {
	CZ_STATUS_TYPE_ON_OFF = 0,
} CZ_STATUS_TYPE_T;

typedef struct {
	uint32_t	id;
	uint16_t	short_addr;
	uint16_t	end_point;
	uint32_t	idle_time;
	CZ_INFO_T	info;
} CZ_DEV_T;

static HSB_DEVICE_DRIVER_CONTEXT_T gl_ctx = { 0 };

static HSB_DEV_DRV_T cz_drv;
static HSB_DEV_OP_T sample_op;
static HSB_DEV_DRV_OP_T cz_drv_op;

static CZ_DEV_T *_find_dev_by_mac(uint8_t *mac)
{
	guint len, id;
	GQueue *queue = &gl_ctx.queue;
	CZ_DEV_T *pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (CZ_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (0 == memcmp(pdev->info.mac, mac, 8))
			return pdev;
	}

	return NULL;
}


static CZ_DEV_T *_find_dev_by_id(uint32_t devid)
{
	guint len, id;
	GQueue *queue = &gl_ctx.queue;
	CZ_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (CZ_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (devid == pdev->id)
			return pdev;
	}

	return NULL;
}

static CZ_DEV_T *_find_dev_by_short_addr(uint16_t short_addr)
{
	guint len, id;
	GQueue *queue = &gl_ctx.queue;
	CZ_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (CZ_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (short_addr == pdev->short_addr)
			return pdev;
	}

	return NULL;
}


static int _register_device(uint16_t short_addr, uint16_t end_point, CZ_INFO_T *info)
{
	GQueue *queue = &gl_ctx.queue;
	CZ_DEV_T *pdev = _find_dev_by_mac(info->mac);
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

	HSB_DEV_INFO_T dev_info;
	dev_info.cls = 0; // TODO
	dev_info.interface = 0; // TODO
	dev_info.dev_type = info->dev_type;
	memcpy(dev_info.mac, info->mac, 8);
	
	int ret = dev_online(cz_drv.id, &dev_info, &status, &sample_op, NULL, false, NULL, &devid);
	if (HSB_E_OK != ret)
		return ret;

	pdev = g_slice_new0(CZ_DEV_T);
	if (!pdev) {
		hsb_critical("create cz dev fail\n");
		return HSB_E_NO_MEMORY;
	}

	pdev->short_addr = short_addr;
	pdev->end_point = end_point;
	memcpy(&pdev->info, info, sizeof(*info));
	pdev->id = devid;

	g_queue_push_tail(queue, pdev);

	return 0;
}

#define NOTIFY_PROCESS_MSG	"notify"

static int notify_process(void)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;
	int fd = unix_socket_new();
	int ret = unix_socket_send_to(
				fd,
				pctx->listen_path,
				NOTIFY_PROCESS_MSG,
				strlen(NOTIFY_PROCESS_MSG));

	unix_socket_free(fd);

	return HSB_E_OK;
}

static int cz_probe(void)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;
	int len;
	uint8_t wbuf[64];
	uint8_t *ptr = wbuf + CZ_HEADER_LEN;

	COND_LOCK();

	pctx->is_waiting = FALSE;

	len = 8;
	SET_CMD_FIELD(wbuf, 0, uint16_t, CZ_HEADER_MAGIC);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len + CZ_HEADER_LEN);
	SET_CMD_FIELD(wbuf, 4, uint16_t, CZ_PKT_TYPE_BC);
	SET_CMD_FIELD(wbuf, 6, uint16_t, 0xFFFF);
	SET_CMD_FIELD(wbuf, 8, uint16_t, CZ_DEFAULT_EP);
	SET_CMD_FIELD(wbuf, 10, uint16_t, 0);

	SET_CMD_FIELD(ptr, 0, uint16_t, CZ_CMD_DEVICE_DISCOVER);
	SET_CMD_FIELD(ptr, 2, uint16_t, len);

	memcpy(pctx->send_buf, wbuf, len + CZ_HEADER_LEN);
	pctx->send_len = len + CZ_HEADER_LEN;

	notify_process();

	COND_UNLOCK();

	return HSB_E_OK;
}

static int sample_set_status(const HSB_STATUS_T *status)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;
	int ret, len;
	uint8_t wbuf[64];
	uint8_t *ptr = wbuf + CZ_HEADER_LEN;
	struct timespec ts;
	int error_no;
	HSB_STATUS_T *pstat = (HSB_STATUS_T *)status;

	CZ_DEV_T *pdev = _find_dev_by_id(pstat->devid);
	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += CZ_TIMEOUT;

	COND_LOCK();
	pctx->is_waiting = TRUE;

	len = 4 + 4 * pstat->num;
	SET_CMD_FIELD(wbuf, 0, uint16_t, CZ_HEADER_MAGIC);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len + CZ_HEADER_LEN);
	SET_CMD_FIELD(wbuf, 4, uint16_t, CZ_PKT_TYPE_UC);
	SET_CMD_FIELD(wbuf, 6, uint16_t, pdev->short_addr);
	SET_CMD_FIELD(wbuf, 8, uint16_t, pdev->end_point);
	SET_CMD_FIELD(wbuf, 10, uint16_t, pctx->trans_id);

	SET_CMD_FIELD(ptr, 0, uint16_t, CZ_CMD_SET_STATUS);
	SET_CMD_FIELD(ptr, 2, uint16_t, len);

	int id;
	for (id = 0; id < pstat->num; id++) {
		SET_CMD_FIELD(ptr, 4 + id * 4, uint16_t, pstat->id[id]);
		SET_CMD_FIELD(ptr, 6 + id * 4, uint16_t, pstat->val[id]);
	}

	memcpy(pctx->send_buf, wbuf, len + CZ_HEADER_LEN);
	pctx->send_len = len + CZ_HEADER_LEN;

	notify_process();

	error_no = pthread_cond_timedwait(&pctx->condition, &pctx->cond_mutex, &ts);
	if (ETIMEDOUT == error_no) {
		ret = HSB_E_OTHERS;
		goto fail;
	}

	ptr = pctx->recv_buf + CZ_HEADER_LEN;
	len = pctx->recv_len - CZ_HEADER_LEN;

	uint16_t cmd = GET_CMD_FIELD(ptr, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(ptr, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(ptr, 4, uint16_t);

	if (cmd != CZ_CMD_RESULT || rlen != len) {
		hsb_critical("set status: get err pkt, len=%d\n", len);
		ret = HSB_E_INVALID_MSG;
		goto fail;
	}

	if (result) {
		hsb_debug("set status result=%d\n", result);
		ret = HSB_E_ACT_FAILED;
		goto fail;
	}

	pctx->is_waiting = FALSE;
	pctx->trans_id++;
	COND_UNLOCK();

	dev_status_updated(pstat->devid, pstat);

	return HSB_E_OK;
fail:
	pctx->is_waiting = FALSE;
	pctx->trans_id++;
	COND_UNLOCK();

	return ret;
}

static int sample_get_status(HSB_STATUS_T *status)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;
	int ret, len;
	uint8_t wbuf[64];
	uint8_t *ptr = wbuf + CZ_HEADER_LEN;
	struct timespec ts;
	int error_no;
	HSB_STATUS_T *pstat = (HSB_STATUS_T *)status;

	CZ_DEV_T *pdev = _find_dev_by_id(pstat->devid);
	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += CZ_TIMEOUT;

	COND_LOCK();
	pctx->is_waiting = TRUE;

	len = 8;
	SET_CMD_FIELD(wbuf, 0, uint16_t, CZ_HEADER_MAGIC);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len + CZ_HEADER_LEN);
	SET_CMD_FIELD(wbuf, 4, uint16_t, CZ_PKT_TYPE_UC);
	SET_CMD_FIELD(wbuf, 6, uint16_t, pdev->short_addr);
	SET_CMD_FIELD(wbuf, 8, uint16_t, pdev->end_point);
	SET_CMD_FIELD(wbuf, 10, uint16_t, pctx->trans_id);

	SET_CMD_FIELD(ptr, 0, uint16_t, CZ_CMD_GET_STATUS);
	SET_CMD_FIELD(ptr, 2, uint16_t, len);
	SET_CMD_FIELD(ptr, 4, uint32_t, 0xffffffff);

	memcpy(pctx->send_buf, wbuf, len + CZ_HEADER_LEN);
	pctx->send_len = len + CZ_HEADER_LEN;

	notify_process();

	error_no = pthread_cond_timedwait(&pctx->condition, &pctx->cond_mutex, &ts);
	if (ETIMEDOUT == error_no) {
		ret = HSB_E_OTHERS;
		goto fail;
	}

	ptr = pctx->recv_buf + CZ_HEADER_LEN;
	len = pctx->recv_len - CZ_HEADER_LEN;

	uint16_t cmd = GET_CMD_FIELD(ptr, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(ptr, 2, uint16_t);

	if (cmd != CZ_CMD_GET_STATUS_RESP || rlen != len) {
		hsb_critical("get status: get err pkt, cmd=%x, len=%d\n", cmd, len);
		ret = HSB_E_INVALID_MSG;
		goto fail;
	}

	int status_num = 0;
	uint16_t id, val;
	len = 4;

	while (len + 4 <= rlen) {
		status->id[status_num] = GET_CMD_FIELD(ptr, len, uint16_t);
		status->val[status_num] = GET_CMD_FIELD(ptr, len + 2, uint16_t);

		status_num ++;
		len += 4;

		if (status_num >= status->num)
			break;
	}

	status->num = status_num;

	pctx->is_waiting = FALSE;
	pctx->trans_id++;
	COND_UNLOCK();

	return HSB_E_OK;
fail:
	pctx->is_waiting = FALSE;
	pctx->trans_id++;
	COND_UNLOCK();

	return ret;
}

static int sample_set_action(const HSB_ACTION_T *act)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;
	int ret, len;
	uint8_t wbuf[64];
	uint8_t *ptr = wbuf + CZ_HEADER_LEN;
	struct timespec ts;
	int error_no;

	CZ_DEV_T *pdev = _find_dev_by_id(act->devid);
	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += CZ_TIMEOUT;

	COND_LOCK();
	pctx->is_waiting = TRUE;

	len = 12;
	SET_CMD_FIELD(wbuf, 0, uint16_t, CZ_HEADER_MAGIC);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len + CZ_HEADER_LEN);
	SET_CMD_FIELD(wbuf, 4, uint16_t, CZ_PKT_TYPE_UC);
	SET_CMD_FIELD(wbuf, 6, uint16_t, pdev->short_addr);
	SET_CMD_FIELD(wbuf, 8, uint16_t, pdev->end_point);
	SET_CMD_FIELD(wbuf, 10, uint16_t, pctx->trans_id);

	SET_CMD_FIELD(ptr, 0, uint16_t, CZ_CMD_DO_ACTION);
	SET_CMD_FIELD(ptr, 2, uint16_t, len);
	SET_CMD_FIELD(ptr, 4, uint16_t, act->id);
	SET_CMD_FIELD(ptr, 6, uint16_t, act->param1);
	SET_CMD_FIELD(ptr, 8, uint32_t, act->param2);

	memcpy(pctx->send_buf, wbuf, len + CZ_HEADER_LEN);
	pctx->send_len = len + CZ_HEADER_LEN;

	notify_process();

	error_no = pthread_cond_timedwait(&pctx->condition, &pctx->cond_mutex, &ts);
	if (ETIMEDOUT == error_no) {
		ret = HSB_E_OTHERS;
		goto fail;
	}

	ptr = pctx->recv_buf + CZ_HEADER_LEN;
	len = pctx->recv_len - CZ_HEADER_LEN;

	uint16_t cmd = GET_CMD_FIELD(ptr, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(ptr, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(ptr, 4, uint16_t);

	if (cmd != CZ_CMD_RESULT || rlen != len) {
		hsb_critical("set action: get err pkt, len=%d\n", len);
		ret = HSB_E_INVALID_MSG;
		goto fail;
	}

	if (result) {
		hsb_debug("set action result=%d\n", result);
		ret = HSB_E_ACT_FAILED;
		goto fail;
	}

	pctx->is_waiting = FALSE;
	pctx->trans_id++;
	COND_UNLOCK();

	return HSB_E_OK;
fail:
	pctx->is_waiting = FALSE;
	pctx->trans_id++;
	COND_UNLOCK();

	return ret;
}

static HSB_DEV_OP_T sample_op = {
	sample_get_status,
	sample_set_status,
	sample_set_action,
	NULL,
	NULL,
};

static HSB_DEV_DRV_OP_T cz_drv_op = {
	cz_probe,
	NULL,
	NULL,
	NULL,
};

static HSB_DEV_DRV_T cz_drv = {
	"cg zigbee",
	HSB_DRV_ID_CJ_ZIGBEE,
	&cz_drv_op,
};

static int _status_updated(HSB_STATUS_T *status)
{
	return dev_status_updated(status->devid, status);
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
		case CZ_EVT_TYPE_SENSOR_TRIGGERED:
			ret = dev_sensor_triggered(devid, param);
			break;
		case CZ_EVT_TYPE_SENSOR_RECOVERED:
			ret = dev_sensor_recovered(devid, param);
			break;
		case CZ_EVT_TYPE_KEY_PRESSED:
			ret = _key(param);
			break;
		case CZ_EVT_TYPE_IR_KEY:
			ret = _ir_key(devid, param, param2);
			break;
		default:
			hsb_debug("unknown event %x\n", id);
			ret = HSB_E_OTHERS;
			break;
	}

	return ret;
}

static int _remove_timeout_dev(void)
{
	GQueue *queue = &gl_ctx.queue;
	int len = g_queue_get_length(queue);
	int id;
	CZ_DEV_T *pdev;

	for (id = 0; id < len; ) {
		pdev = g_queue_peek_nth(queue, id);
		if (!pdev)
			break;

		if (pdev->idle_time > 10) {
			dev_offline(pdev->id);

			g_queue_pop_nth(queue, id);
			hsb_debug("device offline 0x%x\n", pdev->short_addr);
			g_slice_free(CZ_DEV_T, pdev);

			continue;
		}

		pdev->idle_time++;

		id++;
	}

	return 0;
}

static int _refresh_device(CZ_DEV_T *pdev)
{
	pdev->idle_time = 0;

	return 0;
}

int deal_recv_buf(uint8_t *buf, int len)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;

	uint16_t magic = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t tlen = GET_CMD_FIELD(buf, 2, uint16_t);
	uint16_t type = GET_CMD_FIELD(buf, 4, uint16_t);
	uint16_t short_addr = GET_CMD_FIELD(buf, 6, uint16_t);
	uint16_t end_point = GET_CMD_FIELD(buf, 8, uint16_t);
	uint16_t trans_id = GET_CMD_FIELD(buf, 10, uint16_t);

	uint8_t *ptr = buf + CZ_HEADER_LEN;

	uint16_t cmd = GET_CMD_FIELD(ptr, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(ptr, 2, uint16_t);

	CZ_DEV_T *pdev = NULL;

	if (0 == trans_id) {
		switch (cmd) {
			case CZ_CMD_DEVICE_DISCOVER_RESP:
			{
				if (rlen < 22) {
					hsb_critical("probe: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
					return HSB_E_OTHERS;
				}

				_register_device(short_addr, end_point, (CZ_INFO_T *)(ptr + 8));
				break;
			}
			case CZ_CMD_EVENT:
			{
				uint16_t id = GET_CMD_FIELD(ptr, 4, uint16_t);
				uint16_t param = GET_CMD_FIELD(ptr, 6, uint16_t);
				uint32_t param2 = GET_CMD_FIELD(ptr, 8, uint32_t);

				pdev = _find_dev_by_short_addr(short_addr);
				if (!pdev) {
					hsb_critical("device not found\n");
					return HSB_E_OTHERS;
				}

				_event(pdev->id, id, param, param2);

				break;
			}
			case CZ_CMD_KEEP_ALIVE:
			{
				pdev = _find_dev_by_short_addr(short_addr);
				break;
			}
			case CZ_CMD_STATUS_CHANGED:
			{
				uint16_t id;
				uint16_t val;

				pdev = _find_dev_by_short_addr(short_addr);
				if (!pdev) {
					hsb_critical("device not found\n");
					return HSB_E_OTHERS;
				}

				HSB_STATUS_T status = { 0 };
				status.devid = pdev->id;
				status.num = (rlen - 4) / 4;

				int cnt;
				for (cnt = 0; cnt < status.num; cnt++) {
					id = GET_CMD_FIELD(ptr, 4 + cnt * 4, uint16_t);
					val = GET_CMD_FIELD(ptr, 6 + cnt * 4, uint16_t);
					status.id[cnt] = id;
					status.val[cnt] = val;
				}

				_status_updated(&status);

				break;
			}
			default:
				break;
		}

		if (pdev)
			_refresh_device(pdev);

		return HSB_E_OK;
	}

	if (trans_id != pctx->trans_id)
		return HSB_E_OTHERS;

	COND_LOCK();

	if (!pctx->is_waiting) {
		COND_UNLOCK();
		return HSB_E_OTHERS;
	}

	memcpy(pctx->recv_buf, buf, len);
	pctx->recv_len = len;

	COND_UNLOCK();

	pthread_cond_signal(&pctx->condition);

	return HSB_E_OK;
}

static void *_monitor_thread(void *arg)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;
	int fd = pctx->fd;
	int unfd = unix_socket_new_listen((const char *)pctx->listen_path);
	fd_set rset;
	int ret, maxfd;
	struct timeval tv, tval;
	uint8_t rbuf[256];
	int cmd_len, nread, nwrite;

	maxfd = (unfd > fd) ? unfd : fd;

	while (1) {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		FD_SET(unfd, &rset);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		ret = select(maxfd + 1, &rset, NULL, NULL, &tv);

		if (ret < 0)
			continue;

		if (0 == ret) {
			_remove_timeout_dev();
			continue;
		}

		if (FD_ISSET(unfd, &rset)) {
			nread = recvfrom(unfd, rbuf, sizeof(rbuf), 0, NULL, NULL);

			tval.tv_sec = 1;
			tval.tv_usec = 0;
			nwrite = write_timeout(fd, pctx->send_buf, pctx->send_len, &tval);
			if (nwrite < pctx->send_len)
				hsb_debug("uart send %d < %d\n", nwrite, pctx->send_len);

			hsb_debug("write uart %d\n", nwrite);
#ifdef CZ_TEST
			print_buf(pctx->send_buf, pctx->send_len);
#endif
			continue;
		}

		if (!FD_ISSET(fd, &rset))
			continue;

		tval.tv_sec = 1;
		tval.tv_usec = 0;
		nread = read_timeout(fd, rbuf, CZ_HEADER_LEN, &tval);
		if (nread < CZ_HEADER_LEN ||
		   GET_CMD_FIELD(rbuf, 0, uint16_t) != CZ_HEADER_MAGIC)
		{
//#ifdef CZ_TEST
#if 1
			hsb_critical("read uart error pkt, %d bytes\n", nread);
			print_buf(rbuf, nread);
#endif
			continue;
		}

//#ifdef CZ_TEST
#if 1
		hsb_debug("read uart %d\n", nread);
		print_buf(rbuf, nread);
#endif

		cmd_len = GET_CMD_FIELD(rbuf, 2, uint16_t);
		if (cmd_len <= CZ_HEADER_LEN)
			continue;

		tval.tv_sec = 1;
		tval.tv_usec = 0;
		ret = read_timeout(fd, rbuf + nread, cmd_len - CZ_HEADER_LEN, &tval);
		if (ret < cmd_len - CZ_HEADER_LEN)
			continue;

//#ifdef CZ_TEST
#if 1
		hsb_debug("read uart %d\n", ret);
		print_buf(rbuf + nread, ret);
#endif

		deal_recv_buf(rbuf, cmd_len);
	}

	unix_socket_free(unfd);

	return NULL;
}

int init_cz_drv(void)
{
	HSB_DEVICE_DRIVER_CONTEXT_T *pctx = &gl_ctx;

	g_queue_init(&pctx->queue);
	g_mutex_init(&pctx->mutex);

	pthread_cond_init(&pctx->condition, NULL);
	pthread_mutex_init(&pctx->cond_mutex, NULL);

	const char *uart = get_uart_interface();
	if (!uart) {
		hsb_critical("get uart failed\n");
		return HSB_E_OTHERS;
	}

	int fd = open_serial_fd(uart, 115200);
	if (fd < 0) {
		hsb_critical("open %s failed\n", uart);
		return HSB_E_OTHERS;
	}

	pctx->fd = fd;
	pctx->trans_id = 1;

	snprintf(pctx->listen_path,
		sizeof(pctx->listen_path),
		"%sdriver_cz.listen",
		get_work_dir());

	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)_monitor_thread, NULL))
		return -1;

	return register_dev_drv(&cz_drv);
}

