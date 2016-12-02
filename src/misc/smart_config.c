
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdint.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include "thread_utils.h"
#include <stdbool.h>

#define INTERVAL_GUIDE_CODE_MILLISECOND		(10)
#define INTERVAL_DATA_CODE_MILLISECOND		(10)
#define TIMEOUT_GUIDE_CODE_MILLISECOND		(2000)
#define TIMEOUT_DATA_CODE_MILLISECOND		(4000)
#define TIMEOUT_TOTAL_CODE_MILLISECOND		(TIMEOUT_GUIDE_CODE_MILLISECOND + TIMEOUT_DATA_CODE_MILLISECOND)
#define PORT_LISTEN				(18266)
#define TARGET_PORT				(7001)
#define WAIT_UDP_RECV_MILLISECOND		(15000)
#define WAIT_UDP_SEND_MILLISECOND		(45000)

#define ONE_DATA_LEN				(3)

static uint8_t _code[1024];

static uint16_t guide_code[4] = { 515, 514, 513, 512 };
static int gc_len = 4;
static uint16_t data_code[1024];
static int dc_len = 0;

static int _datagramCount = 0;
#define CRC_POLYNOM	(0x8c)
static short crcTable[256];

uint64_t get_millisecond(void)
{
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);

	return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int _getNextDatagramCount(void)
{
	return 1 + (_datagramCount++) % 100;
}

static void getTargetHostName(char *buf)
{
	int count = _getNextDatagramCount();
	sprintf(buf, "234.%d.%d.%d", count, count, count);
}

void init_crcTable(void)
{
	int dividend, bit, remainder;

	for (dividend = 0; dividend < 256; dividend++) {
		remainder = dividend;
		for (bit = 0; bit < 8; bit++) {
			if ((remainder & 0x01) != 0)
				remainder = (remainder >> 1) ^ CRC_POLYNOM;
			else
				remainder = remainder >> 1;
		}

		crcTable[dividend] = (short)remainder;
	}
}

uint8_t crc8(uint8_t *buf, int offset, int len)
{
	short value = 0;
	int data, cnt;

	for (cnt = 0; cnt < len; cnt++) {
		data = buf[offset + cnt] ^ value;
		value = (short) (crcTable[data & 0xff] ^ (value << 8));
	}

	return (uint8_t)(value & 0xff);
}

static void make_data_code(uint16_t data, uint8_t index)
{
	uint8_t datal = data & 0xF;
	uint8_t datah = (data & 0xF0) >> 4;
	uint8_t crc, crcl, crch, seq;
	int offset = index * 3;

	uint8_t buf[2];

	buf[0] = data;
	buf[1] = index;

	crc = crc8(buf, 0, 2);
	crcl = crc & 0xF;
	crch = (crc  & 0xF0) >> 4;

	data_code[offset] = ((uint16_t)(crch << 4) | datah) + 40;
	data_code[offset+1] = ((uint16_t)(0x01 << 8) | index) + 40;
	data_code[offset+2] = ((uint16_t)(crcl << 4) | datal) + 40;
}

static void make_code(char *ssid, char *pwd, uint8_t *ip, uint8_t *bssid)
{
	uint16_t apPwdLen = strlen(pwd);
	uint16_t apSsidLen = strlen(ssid);
	uint16_t total_len, apSsidCrc, apBssidCrc;
	uint16_t total_xor = 0;
	int cnt;
	uint16_t tmp;

	memset(_code, '1', sizeof(_code));

	init_crcTable();

	total_len = 5 + 4 + apPwdLen + apSsidLen;
	make_data_code(total_len, 0);
	total_xor ^= total_len;

	make_data_code(apPwdLen, 1);
	total_xor ^= apPwdLen;

	apSsidCrc = crc8(ssid, 0, apSsidLen);
	make_data_code(apSsidCrc, 2);
	total_xor ^= apSsidCrc;

	apBssidCrc = crc8(bssid, 0, 6);
	make_data_code(apBssidCrc, 3);
	total_xor ^= apBssidCrc;

	for (cnt = 0; cnt < 4; cnt++) {
		tmp = (uint16_t)ip[cnt];
		make_data_code(tmp, 5 + cnt);
		total_xor ^= tmp;
	}

	for (cnt = 0; cnt < apPwdLen; cnt++) {
		tmp = (uint16_t)pwd[cnt];
		make_data_code(tmp, 9 + cnt);
		total_xor ^= tmp;
	}

	for (cnt = 0; cnt < apSsidLen; cnt++) {
		tmp = (uint16_t)ssid[cnt];
		make_data_code(tmp, 9 + apPwdLen + cnt);
		total_xor ^= tmp;
	}
	
	make_data_code(total_xor, 4);

	//dc_len = (9 + apPwdLen + apSsidLen) * 3;
	dc_len = (9 + apPwdLen) * 3;

	printf("dc_len=%d\n",dc_len);
}

static int _send_data(uint16_t *buf, int offset, int len, int timeout)
{
	char ip[32];
	int port = TARGET_PORT;
	struct hostent *hent;
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	int cnt, sockfd;

	getTargetHostName(ip);

	hent = gethostbyname(ip);
	if (!hent)
		return -1;

	sockfd = open_udp_clientfd();
	if (sockfd < 0)
		return -2;

	set_broadcast(sockfd, true);

#if 0
	struct in_addr if_req;
	inet_aton("192.168.1.120", &if_req);
	if (setsockopt(sockfd, SOL_IP, IP_MULTICAST_IF, &if_req, sizeof(if_req)) < 0) {
		printf("set mc if failed\n");
		return -4;
	}
#endif

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	//inet_aton( ip, &servaddr.sin_addr );	
	servaddr.sin_addr = *(struct in_addr *)hent->h_addr;


	for (cnt = offset; cnt < offset + len; cnt++) {
		sendto(sockfd, _code, buf[cnt], 0, (struct sockaddr *)&servaddr, slen);

		//printf("sendto %s, %d bytes\n", inet_ntoa(servaddr.sin_addr), buf[cnt]);

		usleep(timeout * 1000);
	}

	close(sockfd);

	return 0;
}

static void *listen_thread(void *arg)
{
	int fd = open_udp_listenfd(PORT_LISTEN);
	fd_set rset;
	int ret;
	struct timeval tv;
	struct sockaddr_in dev_addr;
	socklen_t dev_len = sizeof(dev_addr);
	uint8_t rbuf[1024];

	set_broadcast(fd, true);

	while (1) {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		tv.tv_sec = WAIT_UDP_RECV_MILLISECOND;
		tv.tv_usec = 0;

		ret = select(fd+1, &rset, NULL, NULL, &tv);

		if (ret < 0)
			continue;

		if (0 == ret) {	/* timeout */
			printf("listen timeout\n");
			break;
		}

		ret = recvfrom(fd, rbuf, sizeof(rbuf), 0, (struct sockaddr *)&dev_addr, &dev_len);

		if (11 == ret) {
			uint8_t mac[6];
			struct in_addr addr;
			memcpy(mac, rbuf + 1, 6);
			memcpy(&addr, rbuf + 7, sizeof(addr));

			printf("device: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			printf("        %s\n", inet_ntoa(addr));
		} else
			printf("ret=%d\n", ret);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int index = 0;
	int ret;
	struct in_addr addr;
	uint8_t *ip = (uint8_t *)&addr;
	uint32_t mac_i[6];
	uint8_t mac[6];

	if (argc < 4) {
		printf("Usage: %s ssid password ap-mac\n", argv[0]);
		return -1;
	}

	get_ip(&addr);

	printf("ip: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

	if (6 != sscanf(argv[3], "%02x:%02x:%02x:%02x:%02x:%02x", &mac_i[0], &mac_i[1], &mac_i[2], &mac_i[3], &mac_i[4], &mac_i[5]))
	{
		printf("ap-mac format: xx:xx:xx:xx:xx:xx\n");
		return -3;
	}

	mac[0] = (uint8_t)(mac_i[0] & 0xff);
	mac[1] = (uint8_t)(mac_i[1] & 0xff);
	mac[2] = (uint8_t)(mac_i[2] & 0xff);
	mac[3] = (uint8_t)(mac_i[3] & 0xff);
	mac[4] = (uint8_t)(mac_i[4] & 0xff);
	mac[5] = (uint8_t)(mac_i[5] & 0xff);

	printf("mac: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	make_code(argv[1], argv[2], (uint8_t *)&addr, mac);

	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)listen_thread, NULL))
	{
		printf("create listen thread failed\n");
		return -1;
	}

	uint64_t start_time = get_millisecond();
	uint64_t current_time = start_time;
	uint64_t last_time = current_time - TIMEOUT_TOTAL_CODE_MILLISECOND;
	while (1) {
		if (current_time - last_time >= TIMEOUT_TOTAL_CODE_MILLISECOND) {

			while (get_millisecond() - current_time < TIMEOUT_GUIDE_CODE_MILLISECOND) {

				_send_data(guide_code, 0, gc_len, INTERVAL_GUIDE_CODE_MILLISECOND);

				if (get_millisecond() - start_time > WAIT_UDP_SEND_MILLISECOND)
					break;
			}

			last_time = current_time;
		} else {

			_send_data(data_code, index, ONE_DATA_LEN, INTERVAL_DATA_CODE_MILLISECOND);

			index = (index + ONE_DATA_LEN) % dc_len;
		}

		current_time = get_millisecond();
		if (current_time - start_time > WAIT_UDP_SEND_MILLISECOND)
			break;
	}

	return 0;
}


