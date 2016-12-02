

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "network_utils.h"
#include "net_protocol.h"
#include "hsb_error.h"
#include "scene.h"

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
		if (buf[offset] != 0xFF)
			return HSB_E_INVALID_MSG;

		paction->delay = buf[offset + 1];
		paction->has_cond = buf[offset + 2] > 0 ? true : false;
		paction->act_num = buf[offset + 3];
		offset += 4;

		if (paction->has_cond) {
			pcond = &paction->condition;
			if (buf[offset] != 0xFE)
				return HSB_E_INVALID_MSG;

			pcond->expr = buf[offset + 1];
			pcond->devid = GET_CMD_FIELD(buf, offset + 2, uint16_t);
			pcond->id = GET_CMD_FIELD(buf, offset + 4, uint16_t);
			pcond->val = GET_CMD_FIELD(buf, offset + 6, uint16_t);
			offset += 8;
		}

		pact = &paction->acts[0];
		for (id = 0; id < paction->act_num; id++)
		{
			if (buf[offset] != 0xFD)
				return HSB_E_INVALID_MSG;

			pact->flag = buf[offset + 1];
			pact->devid = GET_CMD_FIELD(buf, offset + 2, uint16_t);
			pact->id = GET_CMD_FIELD(buf, offset + 4, uint16_t);
			pact->param1 = GET_CMD_FIELD(buf, offset + 6, uint16_t);
			pact->param2 = GET_CMD_FIELD(buf, offset + 8, uint32_t);
			offset += 12;
			pact++;
		}

		paction++;
		action_cnt++;
		if (action_cnt == 8)
			break;
	}

	if (offset != len)
		printf("paser_scene error, offset=%d len=%d\n", offset, len);

	scene->act_num = action_cnt;

	return HSB_E_OK;
}

static int deal_tcp_pkt(int fd, void *buf, size_t count, int *used)
{
	int cnt;
	uint32_t dev_id;

	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(buf, 2, uint16_t);

	printf("get a cmd: %x\n", cmd);
	
	*used = len;

	switch (cmd) {
		case HSB_CMD_GET_DEVS_RESP:
		{
			printf("get devices response:\n");
			for (cnt = 0; cnt < (len - 4) / 4; cnt++)
			{
				dev_id = GET_CMD_FIELD(buf, 4 + cnt*4, uint32_t);
				printf("dev: %d\n", dev_id);
			}
			break;
		}
		case HSB_CMD_GET_INFO_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint32_t drvid = GET_CMD_FIELD(buf, 8, uint32_t);
			uint16_t dev_class = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t interface = GET_CMD_FIELD(buf, 14, uint16_t);

			uint8_t mac[6];
			memcpy(mac, buf + 16, 6);

			printf("dev info: id=%d, drv=%d, class=%d, interface=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
				dev_id, drvid, dev_class, interface, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			break;
		}
		case HSB_CMD_GET_CONFIG_RESP:
		{
			char name[16], location[16];
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			strncpy(name, buf + 8, sizeof(name));
			strncpy(location, buf + 24, sizeof(location));

			printf("dev config: name=%s location=%s\n", name, location);
			break;
		}
		case HSB_CMD_GET_CHANNEL_RESP:
		{
			char name[16];
			uint32_t cid;

			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			strncpy(name, buf + 8, sizeof(name));
			cid = GET_CMD_FIELD(buf, 24, uint32_t);

			printf("channel[%s, %d]=%d\n", name, strlen(name), cid);
			break;
		}
		case HSB_CMD_GET_STATUS_RESP:
		case HSB_CMD_STATUS_UPDATE:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t id, val;

			for (cnt = 0; cnt < (len - 8) / 4; cnt++) {
				id = GET_CMD_FIELD(buf, 8 + cnt * 4, uint16_t);
				val = GET_CMD_FIELD(buf, 10 + cnt * 4, uint16_t);
				printf("device %d status[%d]=%d\n", dev_id, id, val);
			}

			break;
		}
		case HSB_CMD_GET_TIMER_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint8_t work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			uint8_t flag = GET_CMD_FIELD(buf, 11, uint8_t);
			uint8_t hour = GET_CMD_FIELD(buf, 12, uint8_t);
			uint8_t min = GET_CMD_FIELD(buf, 13, uint8_t);
			uint8_t sec = GET_CMD_FIELD(buf, 14, uint8_t);
			uint8_t wday = GET_CMD_FIELD(buf, 15, uint8_t);
			uint16_t year = GET_CMD_FIELD(buf, 16, uint16_t);
			uint8_t mon = GET_CMD_FIELD(buf, 18, uint8_t);
			uint8_t mday = GET_CMD_FIELD(buf, 19, uint8_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 20, uint16_t);
			uint16_t act_param1 = GET_CMD_FIELD(buf, 22, uint16_t);
			uint32_t act_param2 = GET_CMD_FIELD(buf, 24, uint32_t);

			printf("get dev %d timer info:\n", dev_id);
			printf("id=%d, work_mode=%d, flag=%02x, hour=%d, min=%d, sec=%d, wday=%02x, year=%d, mon=%d, mday=%d\n",
				timer_id, work_mode, flag, hour, min, sec, wday, year, mon, mday);
			printf("act: %d, param1=%d, param2=%x\n", act_id, act_param1, act_param2);
			break;
		}
		case HSB_CMD_GET_DELAY_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint8_t work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			uint8_t flag = GET_CMD_FIELD(buf, 11, uint8_t);
			uint16_t evt_id = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t evt_param1 = GET_CMD_FIELD(buf, 14, uint16_t);
			uint32_t evt_param2 = GET_CMD_FIELD(buf, 16, uint32_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 20, uint16_t);
			uint16_t act_param1 = GET_CMD_FIELD(buf, 22, uint16_t);
			uint32_t act_param2 = GET_CMD_FIELD(buf, 24, uint32_t);
			uint32_t delay_sec = GET_CMD_FIELD(buf, 28, uint32_t);

			printf("get dev %d delay info:\n", dev_id);
			printf("id=%d, work_mode=%d, flag=%d, delay=%d\n", delay_id, work_mode, flag, delay_sec);
			printf("evt id=%d, param=%d, param2=%d\n", evt_id, evt_param1, evt_param2);
			printf("act id=%d, param1=%d, param2=%x\n", act_id, act_param1, act_param2);
			break;
		}
		case HSB_CMD_GET_LINKAGE_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint8_t work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			uint8_t flag = GET_CMD_FIELD(buf, 11, uint8_t);
			uint16_t evt_id = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t evt_param1 = GET_CMD_FIELD(buf, 14, uint16_t);
			uint32_t evt_param2 = GET_CMD_FIELD(buf, 16, uint32_t);
			uint32_t act_devid = GET_CMD_FIELD(buf, 20, uint32_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 24, uint16_t);
			uint16_t act_param1 = GET_CMD_FIELD(buf, 26, uint16_t);
			uint32_t act_param2 = GET_CMD_FIELD(buf, 28, uint32_t);
	

			printf("get dev %d linkage info:\n", dev_id);
			printf("id=%d, work_mode=%d, flag=%d, link devid=%d\n", link_id, work_mode, flag, act_devid);
			printf("evt id=%d, param1=%d, param2=%d\n", evt_id, evt_param1, evt_param2);
			printf("act id=%d, param1=%d, param2=%x\n", act_id, act_param1, act_param2);

			break;
		}
		case HSB_CMD_EVENT:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t evt_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint16_t evt_param1 = GET_CMD_FIELD(buf, 10, uint16_t);
			uint32_t evt_param2 = GET_CMD_FIELD(buf, 12, uint32_t);

			printf("dev %d event: id=%d, param1=%d, param2=%x\n", dev_id, evt_id, evt_param1, evt_param2);
			switch (evt_id) {
				case HSB_EVT_TYPE_STATUS_UPDATED:
					printf("status[%d]=%d\n", evt_param1, evt_param2);
					break;
				case HSB_EVT_TYPE_DEV_UPDATED:
					if (evt_param1 > 0)
						printf("device %d online\n", dev_id);
					else
						printf("device %d offline\n", dev_id);
					break;
				case HSB_EVT_TYPE_SENSOR_TRIGGERED:
					break;
				case HSB_EVT_TYPE_SENSOR_RECOVERED:
					break;
				case HSB_EVT_TYPE_MODE_CHANGED:
					break;
				case HSB_EVT_TYPE_IR_KEY:
					break;
				default:
					break;
			}
			break;
		}
		case HSB_CMD_RESULT:
		{
			uint16_t result = GET_CMD_FIELD(buf, 10, uint16_t);
			printf("result=%d\n", result);
			break;
		}
		case HSB_CMD_DEV_ONLINE:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint32_t drvid = GET_CMD_FIELD(buf, 8, uint32_t);
			uint16_t dev_class = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t interface = GET_CMD_FIELD(buf, 14, uint16_t);
			uint32_t dev_type = GET_CMD_FIELD(buf, 16, uint32_t);

			uint8_t mac[8];
			memcpy(mac, buf + 20, 8);

			printf("dev online: id=%d, drv=%d, class=%d, interface=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
				dev_id, drvid, dev_class, interface, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7]);

			char name[16], location[16];
			strncpy(name, buf + 28, sizeof(name));
			strncpy(location, buf + 44, sizeof(location));

			printf("name=%s location=%s\n", name, location);

			uint16_t id, val;

			for (cnt = 0; cnt < (len - 60) / 4; cnt++) {
				id = GET_CMD_FIELD(buf, 60 + cnt * 4, uint16_t);
				val = GET_CMD_FIELD(buf, 62 + cnt * 4, uint16_t);
				printf("status[%d]=%d\n", id, val);
			}


			break;
		}
		case HSB_CMD_SCENE_UPDATE:
		{
			HSB_SCENE_T scene = { 0 };
			int ret = parse_scene(&scene, buf, len);

			if (HSB_E_OK != ret)
				printf("parse scene faild %d\n", ret);
			else {
				printf("scene [%s]\n", scene.name);
			}

			break;
		}
		default:
		{
			printf("unknown cmd: %x\n", cmd);
			break;
		}
	}

	return 0;
}

static int deal_input_cmd(int fd, void *buf, size_t count)
{
	uint8_t rbuf[128];
	int len;
	uint32_t dev_id;
	uint16_t status;
	uint16_t val;
	uint16_t drv_id;
	uint16_t timer_id;
	uint8_t evt_id;
	uint8_t evt_param1;
	uint16_t evt_param2;
	uint16_t act_id;
	uint16_t act_param;
	int val1, val2, val3;
	uint32_t val4;
	char name[16];
	char location[16];

	memset(rbuf, 0, sizeof(rbuf));
	memset(name, 0, sizeof(name));
	memset(location, 0, sizeof(location));

	if (!strncmp(buf, "get devices", 11)) {
		len = 4;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_DEVS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
	} else if (1 == sscanf(buf, "get info %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_INFO);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (1 == sscanf(buf, "get status %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_STATUS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (3 == sscanf(buf, "set status %d %d %d", &val1, &val2, &val3)) {
		len = 12;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_STATUS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
		SET_CMD_FIELD(rbuf, 10, uint16_t, val3);
	} else if (2 == sscanf(buf, "get timer %d %d", &val1, &val2)) {
		len = 12;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_TIMER);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
	} else if (2 == sscanf(buf, "set timer %d %d", &val1, &val2)) {
		len = 24;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_TIMER);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
		SET_CMD_FIELD(rbuf, 10, uint8_t, 0xff);
		SET_CMD_FIELD(rbuf, 11, uint8_t, 0);
		SET_CMD_FIELD(rbuf, 12, uint8_t, 21); /* hour */
		SET_CMD_FIELD(rbuf, 13, uint8_t, 49); /* min */
		SET_CMD_FIELD(rbuf, 14, uint8_t, 0);  /* sec */
		SET_CMD_FIELD(rbuf, 15, uint8_t, 0x7F);
		SET_CMD_FIELD(rbuf, 16, uint16_t, 0); /* on_off */
		SET_CMD_FIELD(rbuf, 18, uint16_t, 1);
	} else if (2 == sscanf(buf, "del timer %d %d", &val1, &val2)) {
		len = 12;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DEL_TIMER);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
	} else if (1 == sscanf(buf, "probe %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_PROBE_DEV);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint16_t, val1);
	} else if (1 == sscanf(buf, "adddev %d", &val1)) {
		len = 40;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_ADD_DEV);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint16_t, 3);
		SET_CMD_FIELD(rbuf, 6, uint16_t, val1);
		strcpy(rbuf + 8, "unknown");
		strcpy(rbuf + 24, "unknown");
	} else if (1 == sscanf(buf, "deldev %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DEL_DEV);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (3 == sscanf(buf, "set config %d %s %s", &val1, name, location)) {
		len = 40;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_CONFIG);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		strncpy(rbuf + 8, name, sizeof(name));
		strncpy(rbuf + 24, location, sizeof(location));
	} else if (1 == sscanf(buf, "get config %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_CONFIG);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (3 == sscanf(buf, "set channel %d %s %d", &val1, name, &val2)) {
		len = 12 + sizeof(name);
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_CHANNEL);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		strncpy(rbuf + 8, name, sizeof(name));
		SET_CMD_FIELD(rbuf, 8 + sizeof(name), uint32_t, val2); 
	} else if (2 == sscanf(buf, "del channel %d %s", &val1, name)) {
		len = 8 + sizeof(name);
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DEL_CHANNEL);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		strncpy(rbuf + 8, name, sizeof(name));
	} else if (2 == sscanf(buf, "switch channel %d %s", &val1, name)) {
		len = 8 + sizeof(name);
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SWITCH_CHANNEL);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		strncpy(rbuf + 8, name, sizeof(name));
	} else if (1 == sscanf(buf, "get channel %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_CHANNEL);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (4 == sscanf(buf, "action %d %d %d %x", &val1, &val2, &val3, &val4)) {
		len = 16;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DO_ACTION);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);	/* devid */
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2); /* action type */
		SET_CMD_FIELD(rbuf, 10, uint16_t, val3); /* action param */
		SET_CMD_FIELD(rbuf, 12, uint32_t, val4); /* action param2 */

		printf("action devid=%d\n", val1);
	} else if (1 == sscanf(buf, "del scene %s", name)) {
		len = 20;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DEL_SCENE);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		strncpy(rbuf + 4, name, sizeof(name));
		printf("del scene %s\n", name);
	} else if (1 == sscanf(buf, "enter scene %s", name)) {
		len = 20;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_ENTER_SCENE);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		strncpy(rbuf + 4, name, sizeof(name));
		printf("enter scene %s\n", name);
	} else if (0 == strncmp(buf, "get scene", 9)) {
		len = 4;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_SCENE);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
	} else if (0 == strncmp(buf, "set scene", 9)) {
		len = 36;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_SCENE);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		char *name = "test-scene";
		strcpy(rbuf + 4, name);

		rbuf[20] = 0xFF;
		rbuf[21] = 0;
		rbuf[22] = 0;
		rbuf[23] = 1;

		rbuf[24] = 0xFD;
		rbuf[25] = 0;
		SET_CMD_FIELD(rbuf, 26, uint16_t, 1);  // devid = 1
		SET_CMD_FIELD(rbuf, 28, uint16_t, 0);
		SET_CMD_FIELD(rbuf, 30, uint16_t, 12);
		SET_CMD_FIELD(rbuf, 32, uint32_t, 0);
	} else {
		printf("invalid cmd\n");
		return -1;
	}

	write(fd, rbuf, len);

	return 0;
}

static int control_box(struct in_addr *addr)
{
	char ip[32];
	inet_ntop(AF_INET, addr, ip, 16);
	struct timeval tv = { 1, 0 };
	int sockfd = connect_nonb(ip, 18002, &tv);
	if (sockfd < 0) {
		printf("connect %s:18002 fail\n", ip);
		return -1;
	}

	/******************************************************/

	int ret, nread, inputfd = 0;
	fd_set readset;
	uint8_t buf[1024];
	while (1) {
		FD_ZERO(&readset);
		FD_SET(inputfd, &readset);
		FD_SET(sockfd, &readset);
		ret = select(sockfd+1, &readset, NULL, NULL, NULL);
		if (ret <= 0) {
			printf("connfd select error %m\n");
			continue;
		}

		if (FD_ISSET(sockfd, &readset)) {
			nread = read(sockfd, buf, sizeof(buf));

			if (nread <= 0) {
				printf("read err %d\n", nread);
				break;
			}

			int tmp, nwrite = 0;
			while (nwrite < nread) {
				deal_tcp_pkt(sockfd, buf + nwrite, nread - nwrite, &tmp);

				nwrite += tmp;
			}

		}

		if (FD_ISSET(inputfd, &readset)) {
			nread = read(inputfd, buf, sizeof(buf));
			buf[nread] = 0;

			deal_input_cmd(sockfd, buf, nread);
		}
	}
	
	close(sockfd);

	return 0;
}

static int probe_box(struct in_addr *addr)
{
	int sockfd, bid;
	struct sockaddr_in servaddr, baddr;

	sockfd = open_udp_clientfd();

	if (get_broadcast_address(sockfd, &servaddr.sin_addr) < 0)
	{
		close(sockfd);
		return -1;
	}
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(18000);

	set_broadcast(sockfd, true);

	int n, ret;
	uint8_t sendline[16], recvline[16];
	socklen_t blen = sizeof(baddr);
	socklen_t servlen = sizeof(servaddr);

	memset(sendline, 0, sizeof(sendline));

	int send_len = 8;
	SET_CMD_FIELD(sendline, 0, uint16_t, HSB_CMD_BOX_DISCOVER);
	SET_CMD_FIELD(sendline, 2, uint16_t, send_len);
	SET_CMD_FIELD(sendline, 4, uint8_t, 0);
	SET_CMD_FIELD(sendline, 5, uint8_t, 1);
	SET_CMD_FIELD(sendline, 6, uint16_t, 1);

	ret = sendto(sockfd, sendline, send_len, 0, (struct sockaddr *)&servaddr, servlen);

	struct timeval tv = { 2, 0 };
	n = recvfrom_timeout(sockfd, recvline, sizeof(recvline), (struct sockaddr *)&baddr, &blen, &tv);
	if (n <= 0) {
		close(sockfd);
		return -2;
	}

	printf("recv %d bytes\n", n);

	uint16_t cmd = GET_CMD_FIELD(recvline, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(recvline, 2, uint16_t);
	bid = GET_CMD_FIELD(recvline, 6, uint16_t);

	printf("cmd: %x, len: %d, bid: %d\n", cmd, len, bid);

	char addstr[32];
	inet_ntop(AF_INET, &baddr.sin_addr, addstr, 16);

	printf("box address: %s\n", addstr);

	close(sockfd);

	memcpy(addr, &baddr.sin_addr, sizeof(*addr));

	return 0;
}

int main(int argc, char *argv[])
{
	struct in_addr addr;

	while (1) {
		if (probe_box(&addr))
			continue;

		control_box(&addr);
	}
	
	return 0;
}


