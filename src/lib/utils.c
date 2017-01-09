/*       
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: util.c 1178 2007-04-03 05:18:24Z kf701 $
 * 
 */
#include "hsb_config.h"
#include "debug.h"
#include "utils.h"
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <syslog.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/wait.h>

int create_pid_file(char *pidfile)
{
	FILE *fp;
	char tbuf[100];
	
	fp = fopen(pidfile, "w+");	
	if ( fp == NULL )
		return -1;

	sprintf(tbuf, "%d", getpid());
	if ( 1 != fwrite(tbuf,strlen(tbuf),1,fp) )
	{
		fclose(fp);
		return -2;	
	}

	fclose(fp);
	return 0;
}

int get_pid_from_file(char *pid_file)
{	
	if ( pid_file == NULL )
		return -1 ;
	
	if ( 0 == access(pid_file,R_OK) )
	{
		FILE *fp;
		char tbuf[100];
		int pid;
		
		fp = fopen(pid_file,"r");
		if (!fp)	
			return -2;
		if ( !fgets(tbuf,100,fp) )
		{
			fclose(fp);
			return -3;
		} else {
			pid = atoi(tbuf);			
			fclose(fp);
			return pid ;
		}
	} 

	return -1 ;
}

// if success , return the number of bytes read
// if error, return -1;
ssize_t readline1(int fd, void *vptr, size_t maxlen) 
{
	ssize_t	n, rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
again:
		if ( (rc = read(fd, &c, 1)) == 1) {
			*ptr++ = c;
			if (c == '\n') 
			{
				break;	/* newline is stored, like fgets() */
			}
		} else if (rc == 0) {
			if (n == 1)
				return(0);	/* EOF, no data read */
			else
				break;	/* EOF, some data was read */
		} else {
			if (errno == EINTR)
				goto again;
			return(-1);		/* error, errno set by read() */
		}
	}

	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}

int check_cmd_prefix(const char *cmd , const char *prefix)
{
	if (NULL == cmd || NULL == prefix)
		return -1;

	if (0 == strncasecmp(cmd, prefix, strlen(prefix)))
		return 0;

	return -2;
}

void hsb_syslog(const char *fmt , ... )
{
	va_list ap;
	va_start(ap,fmt);
	vsyslog(LOG_INFO , fmt , ap);
	va_end(ap);
}

gint hsb_mkstemp (gchar *tmpl)
{
  int len;
  char *XXXXXX;
  int count, fd;
  static const char letters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static const int NLETTERS = sizeof (letters) - 1;
  glong value;
  GTimeVal tv;
  static int counter = 0;

  len = strlen (tmpl);
  if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX"))
    {
      errno = EINVAL;
      return -1;
    }

  /* This is where the Xs start.  */
  XXXXXX = &tmpl[len - 6];

  /* Get some more or less random data.  */
  g_get_current_time (&tv);
  value = (tv.tv_usec ^ tv.tv_sec) + counter++;

  for (count = 0; count < 100; value += 7777, ++count)
    {
      glong v = value;

      /* Fill in the random bits.  */
      XXXXXX[0] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[1] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[2] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[3] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[4] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[5] = letters[v % NLETTERS];

      /* tmpl is in UTF-8 on Windows, thus use g_open() */
      fd = g_open (tmpl, O_RDWR | O_CREAT | O_EXCL , 0600);

      if (fd >= 0)
	return fd;
      else if (errno != EEXIST)
	/* Any other error will apply also to other names we might
	 *  try, and there are 2^32 or so of them, so give up now.
	 */
	return -1;
    }

  /* We got out of the loop because we ran out of combinations to try.  */
  errno = EEXIST;
  return -1;
}

size_t encrypt_b64(guchar* dest,guchar* src,size_t size)
{
	int i, tmp = 0, b64_tmp = 0;
	
	if ( dest == NULL || src == NULL )
		return -1;
	
	while ((size - b64_tmp) >= 3) {
		// every 3 bytes of source change to 4 bytes of destination, 4*6 = 3*8
		dest[tmp] = 0x3F & (src[b64_tmp]>>2);
		dest[tmp+1] = ((src[b64_tmp]<<4) & 0x30) | ((src[b64_tmp+1]>>4) & 0x0F);
		dest[tmp+2] = ((src[b64_tmp+1]<<2) & 0x3C) | ((src[b64_tmp+2]>>6) & 0x03);
		dest[tmp+3] = 0x3F & src[b64_tmp+2];
		for (i=0; i<=3; i++) {
			if ( (dest[tmp+i] <= 25) )
				dest[tmp+i] += 'A';
			else	
				if (dest[tmp+i] <= 51)
					dest[tmp+i] += 'a' - 26;	       
				else
					if (dest[tmp+i] <= 61)
						dest[tmp+i] += '0' - 52;
			if (dest[tmp+i] == 62)
				dest[tmp+i] = '+';
			if (dest[tmp+i] == 63)
				dest[tmp+i] = '/';
		}
		
		tmp += 4;
		b64_tmp += 3;
	} //end while	
	if (b64_tmp == size) {
		dest[tmp] = '\0';
		return tmp;
	}
	if ((size - b64_tmp) == 1) {    //one
		dest[tmp] = 0x3F & (src[b64_tmp]>>2);
		dest[tmp+1] = (src[b64_tmp]<<4) & 0x30;
		dest[tmp+2] = '=';
	}
	else {    //two
		dest[tmp] = 0x3F & (src[b64_tmp]>>2);
		dest[tmp+1] = ((src[b64_tmp]<<4) & 0x30) | ((src[b64_tmp+1]>>4) & 0x0F);
		dest[tmp+2] = (src[b64_tmp+1]<<2) & 0x3F;	
	}

	for (i=0; i<=(size - b64_tmp); i++) {
		if  (dest[tmp+i] <= 25)
			dest[tmp+i] += 'A';
		else	
			if (dest[tmp+i] <= 51)
				dest[tmp+i] += 'a' - 26;	       
			else
				if (dest[tmp+i] <= 61)
					dest[tmp+i] += '0' - 52;	//end if
		if (dest[tmp+i] == 62)
			dest[tmp+i] = '+';
		if (dest[tmp+i] == 63)
			dest[tmp+i] = '/';
	}
	
	dest[tmp+3] = '=';
	dest[tmp+4] = '\0';

	return tmp+4;
}

static const short base64_reverse_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

size_t decode_b64( guchar *desc ,const guchar *str, int length)
{
	const unsigned char *current = str;
	int ch, i = 0, j = 0, k;
	/* this sucks for threaded environments */
	unsigned char *result;
	int ret_length;
	
	result = desc ;
	if (result == NULL) {
		return -1;
	}

	/* run through the whole string, converting as we go */
	while ((ch = *current++) != '\0' && length-- > 0) {
		if (ch == '=') break;

	    /* When Base64 gets POSTed, all pluses are interpreted as spaces.
		   This line changes them back.  It's not exactly the Base64 spec,
		   but it is completely compatible with it (the spec says that
		   spaces are invalid).  This will also save many people considerable
		   headache.  - Turadg Aleahmad <turadg@wise.berkeley.edu>
	    */

		if (ch == ' ') ch = '+'; 

		ch = base64_reverse_table[ch];
		if (ch < 0) continue;

		switch(i % 4) {
		case 0:
			result[j] = ch << 2;
			break;
		case 1:
			result[j++] |= ch >> 4;
			result[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			result[j++] |= ch >>2;
			result[j] = (ch & 0x03) << 6;
			break;
		case 3:
			result[j++] |= ch;
			break;
		}
		i++;
	}

	k = j;
	/* mop things up if we ended on a boundary */
	if (ch == '=' ) {
		switch(i % 4) {
		case 1:
			return -1;
		case 2:
			k++;
		case 3:
			result[k++] = 0;
		}
	}
    ret_length = j;
	result[j] = '\0';
	return ret_length;
}

time_t get_uptime(void)
{
	return time(NULL);
}

uint64_t get_msec(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

const char *get_work_dir(void)
{
	return WORK_DIR;
}

const char *get_eth_interface(void)
{
	return ETH_INTERFACE;
}

const char *get_uart_interface(void)
{
	const char *intf1 = "/dev/ttyUSB0";
	const char *intf2 = "/dev/ttyS1";

	if (0 == access(intf1, F_OK))
		return intf1;
	else if (0 == access(intf2, F_OK))
		return intf2;

	return NULL;
}

void get_config_file(char *path)
{
	sprintf(path, CONFIG_DIR"%s", HSB_CONFIG_FILE);
}

int str_to_mac(char *buf, uint8_t *pmac)
{
	if (!buf || !pmac)
		return -1;

	uint32_t mac[8];
	sscanf(buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		&mac[0], &mac[1], &mac[2], &mac[3],
		&mac[4], &mac[5], &mac[6], &mac[7]);

	int id;
	for (id = 0; id < 8; id++) {
		pmac[id] = (uint8_t)(mac[id] & 0xFF);
	}

	return 0;
}

int mac_to_str(uint8_t *mac, char *buf)
{
	if (!buf || !mac)
		return -1;

	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3],
		mac[4], mac[5], mac[6], mac[7]);

	return 0;
}

