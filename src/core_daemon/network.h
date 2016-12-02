
#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include "device.h"

#define CORE_UDP_LISTEN_PORT		(18000)
#define CORE_TCP_LISTEN_PORT		(18002)


int notify_resp(HSB_RESP_T *resp, void *data);

int init_network_module(void);

#endif

