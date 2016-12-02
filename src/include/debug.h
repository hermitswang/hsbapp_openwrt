/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: debug.h 144 2005-08-21 09:45:02Z falls $
 * 
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_ 

/* default link type is extern  */
int debug_verbose ;

#define HSB_DEBUG_LOGFILE	"/tmp/hsb.debug"
#define MAX_DEBUG_FILE_SIZE	512  // kbytes

int hsb_message(const char *fmt, ...) ;

#if 1
#define hsb_debug(args...)	\
    do {                        \
 	    if (3 <= debug_verbose )  \
            hsb_message(args);     \
    } while(0)

#define hsb_warning(args...)	\
    do {                        \
        if (2 <= debug_verbose )  \
            hsb_message(args);     \
    } while(0)	
	
#define hsb_critical(args...)	\
    do {                        \
        if (1 <= debug_verbose )  \
            hsb_message(args);     \
    } while(0)	

#else
#define hsb_debug(n, args...)	do { } while (0)
#define hsb_warning(n, args...)	do { } while (0)
#define hsb_critical(n, args...)	do { } while (0)
#endif

#include <stdio.h>
#define print_buf(buf, len)	do { \
	int idx; \
	uint8_t *ptr = (uint8_t *)(buf); \
	for (idx = 0; idx < len; idx++) { \
		printf("%02X ", ptr[idx]); \
		if ((idx + 1) % 16 == 0) \
			printf("\n"); \
	} \
	printf("\n"); \
} while (0)

#endif
