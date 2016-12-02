
#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "net_protocol.h"
#include "hsb_device.h"
#include "hsb_error.h"
#include "channel.h"

typedef struct {
	uint32_t	devid;
	uint16_t	id;
	uint16_t	param1;
	uint32_t	param2;
} HSB_ACTION_T;

typedef struct {
	uint32_t	devid;
	uint16_t	id;
	uint16_t	param1;
	uint32_t	param2;
} HSB_EVT_T;

typedef struct {
	uint32_t	devid;
	uint32_t	num;
	uint16_t	id[8];
	uint16_t	val[8];
} HSB_STATUS_T;

typedef struct {
	uint32_t	drvid;
} HSB_PROBE_T;

typedef struct {
	HSB_ERROR_NO_T		ret_val;
	uint32_t		devid;
	uint16_t		cmd;
} HSB_RESULT_T;

typedef enum {
	HSB_ACT_TYPE_PROBE = 0,
	HSB_ACT_TYPE_SET_STATUS,
	HSB_ACT_TYPE_GET_STATUS,
	HSB_ACT_TYPE_DO_ACTION,
} HSB_ACT_TYPE_T;

typedef struct {
	HSB_ACT_TYPE_T		type;
	void			*reply;
	union {
		HSB_PROBE_T	probe;
		HSB_STATUS_T	status;
		HSB_ACTION_T	action;
	} u;
} HSB_ACT_T;

typedef enum {
	HSB_RESP_TYPE_RESULT = 0,
	HSB_RESP_TYPE_EVENT,
	HSB_RESP_TYPE_STATUS,
	HSB_RESP_TYPE_STATUS_UPDATE,
} HSB_RESP_TYPE_T;

typedef struct {
	HSB_RESP_TYPE_T		type;
	void			*reply;
	union {
		HSB_EVT_T	event;
		HSB_STATUS_T	status;
		HSB_RESULT_T	result;
	} u;
} HSB_RESP_T;

#define HSB_DEV_MAX_TIMER_NUM		(32)
#define HSB_DEV_MAX_DELAY_NUM		(8)
#define HSB_DEV_MAX_LINKAGE_NUM		(8)
#define HSB_DEV_MAX_NAME_LEN		(16)
#define HSB_DEV_MAX_LOCATION_LEN	(16)

typedef struct {
	uint16_t	num;
	uint16_t	val[8];
} HSB_DEV_STATUS_T;

typedef struct {
	HSB_DEV_CLASS_T		cls;
	uint32_t		interface;
	uint32_t		dev_type;
	uint8_t			mac[8];
} HSB_DEV_INFO_T;

typedef struct {
	char			name[HSB_DEV_MAX_NAME_LEN];
	char			location[HSB_DEV_MAX_LOCATION_LEN];
} HSB_DEV_CONFIG_T;

typedef enum {
	HSB_DEV_STATE_UNINIT = 0,
	HSB_DEV_STATE_ONLINE,
	HSB_DEV_STATE_OFFLINE,
} HSB_DEV_STATE_T;

struct _HSB_DEV_T;
struct _HSB_DEV_DRV_T;

typedef struct {
	int (*get_status)(HSB_STATUS_T *status);
	int (*set_status)(const HSB_STATUS_T *status);
	int (*set_action)(const HSB_ACTION_T *act);
	int (*init)(void **priv);
	int (*release)(void *priv);
} HSB_DEV_OP_T;

typedef struct {
	int (*probe)(void);
	int (*add_dev)(HSB_DEV_TYPE_T dev_type, HSB_DEV_CONFIG_T *cfg);
	int (*del_dev)(uint32_t devid);
	int (*recover_dev)(uint32_t devid, uint32_t type);
} HSB_DEV_DRV_OP_T;

typedef struct _HSB_DEV_DRV_T {
	char			*name;
	uint32_t		id;

	HSB_DEV_DRV_OP_T	*op;
} HSB_DEV_DRV_T;

typedef struct _HSB_TIMER_T {
	uint16_t	id;
	uint8_t		work_mode;
	uint8_t		flag;
	uint16_t	year;
	uint8_t		mon;
	uint8_t		mday;
	uint8_t		hour;
	uint8_t		min;
	uint8_t		sec;
	uint8_t		wday;
	uint16_t	act_id;
	uint16_t	act_param1;
	uint32_t	act_param2;
} HSB_TIMER_T;

typedef struct {
	bool		active;
	bool		expired;
} HSB_TIMER_STATUS_T;

typedef struct _HSB_DELAY_T {
	uint16_t	id;
	uint8_t		work_mode;
	uint8_t		flag;
	uint16_t	evt_id;
	uint16_t	evt_param1;
	uint32_t	evt_param2;
	uint16_t	act_id;
	uint16_t	act_param1;
	uint32_t	act_param2;
	uint32_t	delay_sec;
} HSB_DELAY_T;

typedef struct {
	bool		active;
	bool		started;
	time_t		start_tm;
} HSB_DELAY_STATUS_T;

typedef struct _HSB_LINKAGE_T {
	uint16_t	id;
	uint8_t		work_mode;
	uint8_t		flag;
	uint16_t	evt_id;
	uint16_t	evt_param1;
	uint32_t	evt_param2;
	uint32_t	act_devid;
	uint16_t	act_id;
	uint16_t	act_param1;
	uint32_t	act_param2;
} HSB_LINKAGE_T;

typedef struct {
	bool		active;
} HSB_LINKAGE_STATUS_T;

typedef struct _HSB_DEV_T {
	uint32_t		id;

	uint32_t		state;

	HSB_DEV_STATUS_T	status;

	HSB_DEV_INFO_T		info;

	HSB_DEV_CONFIG_T	config;

	uint32_t		work_mode;
	HSB_CHANNEL_DB_T	*pchan_db;

	HSB_TIMER_T		timer[HSB_DEV_MAX_TIMER_NUM];
	HSB_TIMER_STATUS_T	timer_status[HSB_DEV_MAX_TIMER_NUM];

	HSB_DELAY_T		delay[HSB_DEV_MAX_DELAY_NUM];
	HSB_DELAY_STATUS_T	delay_status[HSB_DEV_MAX_DELAY_NUM];

	HSB_LINKAGE_T		link[HSB_DEV_MAX_LINKAGE_NUM];
	HSB_LINKAGE_STATUS_T	link_status[HSB_DEV_MAX_LINKAGE_NUM];

	union {
		struct in_addr	ip;
	} prty;

	uint32_t		idle_time;

	uint32_t		drvid;
	HSB_DEV_DRV_T		*driver;
	HSB_DEV_OP_T		*op;

	struct _HSB_DEV_T	*ir_dev;
	void			*priv_data;

	uint64_t		op_msec;
} HSB_DEV_T;

int init_dev_module(void);
int get_dev_id_list(uint32_t *dev_id, int *dev_num);
int get_dev_info(uint32_t dev_id, HSB_DEV_T *pdev);
HSB_DEV_T *find_dev(uint32_t dev_id);
int get_dev_cfg(uint32_t dev_id, HSB_DEV_CONFIG_T *cfg);
int set_dev_cfg(uint32_t dev_id, const HSB_DEV_CONFIG_T *cfg);


int get_status(HSB_DEV_T *pdev, HSB_STATUS_T *status);
int set_status(HSB_DEV_T *pdev, const HSB_STATUS_T *status);
int set_action(HSB_DEV_T *pdev, const HSB_ACTION_T *act);

int get_dev_status_async(uint32_t devid, void *reply);
int set_dev_status_async(const HSB_STATUS_T *status, void *reply);
int probe_dev_async(const HSB_PROBE_T *probe, void *reply);
int add_dev(uint32_t drv_id, HSB_DEV_TYPE_T dev_type, HSB_DEV_CONFIG_T *cfg);
int del_dev(uint32_t devid);
int set_dev_action_async(const HSB_ACTION_T *act, void *reply);

int get_dev_timer(uint32_t dev_id, uint16_t timer_id, HSB_TIMER_T *timer);
int set_dev_timer(uint32_t dev_id, const HSB_TIMER_T *timer);
int del_dev_timer(uint32_t dev_id, uint16_t timer_id);
int get_dev_delay(uint32_t dev_id, uint16_t delay_id, HSB_DELAY_T *delay);
int set_dev_delay(uint32_t dev_id, const HSB_DELAY_T *delay);
int del_dev_delay(uint32_t dev_id, uint16_t delay_id);
int get_dev_linkage(uint32_t dev_id, uint16_t link_id, HSB_LINKAGE_T *link);
int set_dev_linkage(uint32_t dev_id, const HSB_LINKAGE_T *link);
int del_dev_linkage(uint32_t dev_id, uint16_t link_id);

HSB_DEV_T *alloc_dev(uint32_t devid);
HSB_DEV_T *create_dev(void);
int destroy_dev(HSB_DEV_T *dev);
int register_dev(HSB_DEV_T *dev);
int remove_dev(HSB_DEV_T *dev);
int dev_online(uint32_t drvid,
		HSB_DEV_INFO_T *info,
		HSB_DEV_STATUS_T *status,
		HSB_DEV_OP_T *op,
		HSB_DEV_CONFIG_T *cfg,
		bool support_channel,
		void *priv,
		uint32_t *devid);
int dev_offline(uint32_t devid);
int dev_removed(uint32_t devid);
int dev_recovered(uint32_t devid,
		uint32_t drvid,
		HSB_DEV_INFO_T *info,
		HSB_DEV_STATUS_T *status,
		HSB_DEV_OP_T *op,
		bool support_channel,
		void *priv);

int dev_status_updated(uint32_t devid, HSB_STATUS_T *status);
int dev_updated(uint32_t devid, HSB_DEV_UPDATED_TYPE_T type, HSB_DEV_TYPE_T dev_type);
int dev_sensor_triggered(uint32_t devid, HSB_SENSOR_TYPE_T type);
int dev_sensor_recovered(uint32_t devid, HSB_SENSOR_TYPE_T type);
int dev_ir_key(uint32_t devid, uint16_t param1, uint32_t param2);
int dev_mode_changed(HSB_WORK_MODE_T mode);

int set_box_work_mode(HSB_WORK_MODE_T mode);
HSB_WORK_MODE_T get_box_work_mode(void);

int register_dev_drv(HSB_DEV_DRV_T *drv);

int init_virtual_switch_drv(void);

int check_timer_and_delay(void);

int set_dev_channel(uint32_t devid, char *name, uint32_t cid);
int del_dev_channel(uint32_t devid, char *name);
int get_dev_channel(uint32_t devid, char *name, uint32_t *cid);
int get_dev_channel_num(uint32_t devid, int *num);
int get_dev_channel_by_id(uint32_t devid, int id, char *name, uint32_t *cid);

int report_all_device(void *data);

int sync_dev_status(HSB_DEV_T *pdev, const HSB_STATUS_T *status);
int load_dev_status(HSB_DEV_T *pdev, HSB_STATUS_T *status);

int save_config(void);

#endif

