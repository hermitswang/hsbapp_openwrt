

#include <string.h>
#include <glib.h>
#include <pthread.h>
#include <errno.h>
#include "device.h"
#include "network_utils.h"
#include "debug.h"
#include "thread_utils.h"
#include "device.h"
#include "hsb_error.h"
#include "hsb_config.h"
#include "channel.h"

static int ir_dev_delay(HSB_DEV_T *pdev)
{
	uint64_t msec = get_msec();
	uint64_t next = pdev->op_msec + 1000;

	if (msec < next) {
		usleep(next - msec);
	}

	pdev->op_msec = get_msec();

	return HSB_E_OK;
}

static HSB_DEV_DRV_T ir_drv;
static HSB_DEV_OP_T cc9201_op;
static HSB_DEV_OP_T gree_op;
static HSB_DEV_DRV_OP_T ir_drv_op;

static uint8_t cc9201_key_map[HSB_TV_ACTION_LAST] = {
	0x4C,
	0x48,
	0x49,
	0x43,
	0x42,
	0x46,
	0x47,
	0x44,
	0x45,
	0x4D,
	0x30,
	0x31,
	0x32,
	0x33,
	0x34,
	0x35,
	0x36,
	0x37,
	0x38,
	0x39,
};

static int cc9201_key_press(HSB_DEV_T *ir_dev, HSB_TV_ACTION_T action)
{
	if (!ir_dev || action >= HSB_TV_ACTION_LAST)
		return HSB_E_BAD_PARAM;

	HSB_ACTION_T act = { 0 };

	act.devid = ir_dev->id;
	act.id = HSB_ACT_TYPE_REMOTE_CONTROL;
	act.param1 = HSB_IR_PROTOCOL_TYPE_CC9201;
	act.param2 = cc9201_key_map[action];

	return set_action(ir_dev, &act);
}


static int cc9201_init(void **priv)
{
	*priv = NULL;

	return HSB_E_OK;
}

static int cc9201_release(void *priv)
{
	return HSB_E_OK;
}

static int cc9201_set_status(const HSB_STATUS_T *status)
{
	HSB_DEV_T *pdev = find_dev(status->devid);

	if (!pdev)
		return HSB_E_OTHERS;

	HSB_DEV_T *irdev = pdev->ir_dev;
	if (!irdev)
		return HSB_E_OTHERS;

	
	switch (status->id[0]) {
		case HSB_TV_STATUS_CHANNEL:
		{
			uint32_t channel = status->val[0];
			int id;

			ir_dev_delay(pdev);

			id = (channel / 100) % 10;
			cc9201_key_press(irdev, HSB_TV_ACTION_KEY_0 + id);

			ir_dev_delay(pdev);

			id = (channel / 10) % 10;
			cc9201_key_press(irdev, HSB_TV_ACTION_KEY_0 + id);

			ir_dev_delay(pdev);

			id = channel % 10;
			cc9201_key_press(irdev, HSB_TV_ACTION_KEY_0 + id);

			pdev->status.val[HSB_TV_STATUS_CHANNEL] = channel;

			dev_status_updated(pdev->id, (HSB_STATUS_T *)status);

			break;
		}
		default:
			break;
	}

	return HSB_E_OK;
}

static int cc9201_get_status(HSB_STATUS_T *status)
{
	HSB_DEV_T *pdev = find_dev(status->devid);

	if (!pdev)
		return HSB_E_OTHERS;

	uint32_t id = status->id[0];
	if (id >= HSB_TV_STATUS_LAST)
		return HSB_E_OTHERS;

	status->val[0] = pdev->status.val[id];
	status->num = 1;

	return HSB_E_OK;
}

static int cc9201_set_action(const HSB_ACTION_T *act)
{
	HSB_DEV_T *pdev = find_dev(act->devid);

	if (!pdev)
		return HSB_E_OTHERS;

	HSB_DEV_T *irdev = pdev->ir_dev;
	if (!irdev)
		return HSB_E_OTHERS;

	ir_dev_delay(pdev);

	return cc9201_key_press(irdev, act->param1);
}

static int cc9201_get_channel_db(HSB_DEV_T *pdev, HSB_CHANNEL_DB_T **pdb)
{
	*pdb = (HSB_CHANNEL_DB_T *)pdev->priv_data;

	return HSB_E_OK;
}

static HSB_DEV_OP_T cc9201_op = {
	cc9201_get_status,
	cc9201_set_status,
	cc9201_set_action,
	cc9201_init,
	cc9201_release,
};

typedef enum {
	GREE_STATUS_ID_WORK_MODE = 0,
	GREE_STATUS_ID_POWER,
	GREE_STATUS_ID_WIND_SPEED,
	GREE_STATUS_ID_TEMPERATURE,
	GREE_STATUS_ID_LIGHT,
} GREE_STATUS_ID;

static int gree_set_status(const HSB_STATUS_T *status)
{
	HSB_DEV_T *pdev = find_dev(status->devid);

	if (!pdev)
		return HSB_E_OTHERS;

	HSB_DEV_T *irdev = pdev->ir_dev;
	if (!irdev)
		return HSB_E_OTHERS;

	HSB_STATUS_T _status = { 0 };
	_status.devid = pdev->id;

	int id;
	uint16_t val;
	bool auto_mode = false;
	for (id = 0; id < status->num; id++)
	{
		val = status->val[id];
		switch (status->id[id]) {
			case GREE_STATUS_ID_WORK_MODE:
				if (val > 4)
					return HSB_E_BAD_PARAM;
				if (val == 0)
					auto_mode = true;
				break;
			case GREE_STATUS_ID_POWER:
				if (val > 0)
					val = 1;
				break;
			case GREE_STATUS_ID_WIND_SPEED:
				if (val > 3)
					return HSB_E_BAD_PARAM;
				break;
			case GREE_STATUS_ID_TEMPERATURE:
				if (val < 16 || val > 30)
					return HSB_E_BAD_PARAM;
				if (auto_mode)
					val = 25;
				break;
			case GREE_STATUS_ID_LIGHT:
				if (val > 0)
					val = 1;
				break;
			default:
				return HSB_E_BAD_PARAM;
				break;
		}

		_status.id[_status.num] = status->id[id];
		_status.val[_status.num] = val;
		_status.num++;
	}

	sync_dev_status(pdev, &_status);

	HSB_DEV_STATUS_T *pstat = &pdev->status;
	uint8_t data[4];

	data[0] = pstat->val[GREE_STATUS_ID_WORK_MODE] & 0x07;
	if (pstat->val[GREE_STATUS_ID_POWER] > 0)
		data[0] |= (1 << 3);
	data[0] |= ((pstat->val[GREE_STATUS_ID_WIND_SPEED] & 0x3) << 4);

	data[1] = pstat->val[GREE_STATUS_ID_TEMPERATURE] & 0x0F;

	data[2] = pstat->val[GREE_STATUS_ID_LIGHT] ? 0x20 : 0;

	data[3] = 0;

	ir_dev_delay(pdev);

	HSB_ACTION_T act = { 0 };
	act.devid = irdev->id;
	act.id = HSB_ACT_TYPE_REMOTE_CONTROL;
	act.param1 = HSB_IR_PROTOCOL_TYPE_GREE;
	act.param2 = *(uint32_t *)data;

	int ret = set_action(irdev, &act);
	if (HSB_E_OK != ret) {
		hsb_debug("set action fail ret=%d\n", ret);
		return ret;
	}

	dev_status_updated(pdev->id, &_status);

	return ret;
}

static int gree_get_status(HSB_STATUS_T *status)
{
	HSB_DEV_T *pdev = find_dev(status->devid);

	if (!pdev)
		return HSB_E_OTHERS;

	load_dev_status(pdev, status);

	return HSB_E_OK;
}

static HSB_DEV_OP_T gree_op = {
	gree_get_status,
	gree_set_status,
	NULL,
	NULL,
	NULL,
};

static int ir_add_dev(HSB_DEV_TYPE_T ir_type, HSB_DEV_CONFIG_T *cfg)
{
	bool support_channel = false;
	uint32_t devid;
	HSB_DEV_INFO_T dev_info = { 0 };
	dev_info.interface = HSB_INTERFACE_IR;
	dev_info.dev_type = ir_type;

	HSB_DEV_STATUS_T status = { 0 };

	HSB_DEV_OP_T *op = NULL;
	// ir_type to dev op
	switch (ir_type) {
		case HSB_DEV_TYPE_CC9201:
			dev_info.cls = HSB_DEV_CLASS_STB;

			status.num = 1;
			status.val[0] = 0;

			op = &cc9201_op;

			support_channel = true;
			break;
		case HSB_DEV_TYPE_GREE_AC:
			dev_info.cls = HSB_DEV_CLASS_AIR_CONDITIONER;

			status.num = 5;
			status.val[GREE_STATUS_ID_WORK_MODE] = 0;
			status.val[GREE_STATUS_ID_POWER] = 0;
			status.val[GREE_STATUS_ID_WIND_SPEED] = 0;
			status.val[GREE_STATUS_ID_TEMPERATURE] = 25;
			status.val[GREE_STATUS_ID_LIGHT] = 1;

			op = &gree_op;
			break;
		default:
			break;
	}

	if (!op)
		return HSB_E_NOT_SUPPORTED;

	void *priv = NULL;
	int ret;
	if (op->init) {
		ret = op->init(&priv);
		if (ret != HSB_E_OK)
			return ret;
	}

	return dev_online(ir_drv.id, &dev_info, &status, op, cfg, support_channel, priv, &devid);
}

static int ir_del_dev(uint32_t devid)
{
	int ret = HSB_E_OK;
	HSB_DEV_T *pdev = find_dev(devid);

	if (!pdev)
		return HSB_E_OTHERS;

	HSB_DEV_OP_T *op = pdev->op;
	if (!op)
		return HSB_E_NOT_SUPPORTED;

	if (op->release) {
		ret = op->release(pdev->priv_data);
		if (ret != HSB_E_OK)
			return ret;
	}

	return dev_removed(devid);
}

static int ir_recover_dev(uint32_t devid, uint32_t type)
{
	bool support_channel = false;
	HSB_DEV_INFO_T dev_info = { 0 };
	dev_info.interface = HSB_INTERFACE_IR;
	dev_info.dev_type = type;

	HSB_DEV_STATUS_T status = { 0 };

	HSB_DEV_OP_T *op = NULL;
	// ir_type to dev op
	switch (type) {
		case HSB_DEV_TYPE_CC9201:
			dev_info.cls = HSB_DEV_CLASS_STB;

			status.num = 1;
			status.val[0] = 0;

			op = &cc9201_op;

			support_channel = true;
			break;
		case HSB_DEV_TYPE_GREE_AC:
			dev_info.cls = HSB_DEV_CLASS_AIR_CONDITIONER;

			status.num = 5;
			status.val[GREE_STATUS_ID_WORK_MODE] = 0;
			status.val[GREE_STATUS_ID_POWER] = 0;
			status.val[GREE_STATUS_ID_WIND_SPEED] = 0;
			status.val[GREE_STATUS_ID_TEMPERATURE] = 25;
			status.val[GREE_STATUS_ID_LIGHT] = 1;

			op = &gree_op;
			break;
		default:
			break;
	}

	if (!op)
		return HSB_E_NOT_SUPPORTED;

	void *priv = NULL;
	int ret;
	if (op->init) {
		ret = op->init(&priv);
		if (ret != HSB_E_OK)
			return ret;
	}

	return dev_recovered(devid, ir_drv.id, &dev_info, &status, op, support_channel, priv);
}

static HSB_DEV_DRV_OP_T ir_drv_op = {
	NULL,
	ir_add_dev,
	ir_del_dev,
	ir_recover_dev,
};

static HSB_DEV_DRV_T ir_drv = {
	"cg ir",
	HSB_DRV_ID_IR,
	&ir_drv_op,
};

int init_ir_drv(void)
{
	return register_dev_drv(&ir_drv);
}



