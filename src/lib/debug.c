/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: debug.c 1111 2006-05-27 19:14:37Z wangxiaogang $
 * 
 */

#include "hsb_config.h"
#include "debug.h"
#include "utils.h"
#include <libgen.h>
#include <unistd.h>
#include <stdarg.h>    
#include <stdio.h>
#include <time.h>    
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <syslog.h>

int hsb_message(const char *fmt, ...) 
{
	FILE *fp;
	va_list ap;
	char buf[MAXLINE] , fd_path[64] , name[128];
	time_t t;
	struct tm tm;
	struct stat stats;
	pid_t my_pid = getpid();
	
	sprintf(fd_path,"/proc/%d/exe",my_pid);
	bzero(name,sizeof(name));
	
	if (readlink(fd_path, name , sizeof(name)) < 0)
		return -1;
	

	memmove( name , basename(name) , strlen(basename(name))+1 );
	
	sprintf ( fd_path , "/proc/%d/fd/0" , my_pid);
	if ( readlink(fd_path,buf,sizeof(buf)) > 0 )
	{		
		if ( NULL == strstr(buf ,"/dev/null") )
		{            
			printf("%s(%lu): ", name , time(NULL)); 
			va_start(ap,fmt);
			vprintf(fmt,ap);
			va_end(ap);	
			return 0;
		}
	}

#if 0	
	if ( (stat(HSB_DEBUG_LOGFILE, &stats) == 0) && (stats.st_size > MAX_DEBUG_FILE_SIZE*1024) )
	{		
		if ( -1 == truncate(HSB_DEBUG_LOGFILE,0) )
		{		
			return -1 ;
		}
	}

	time(&t);
	localtime_r(&t,&tm);
	strftime(buf,sizeof(buf),"%F %T",&tm);
	if ( NULL == (fp = fopen(HSB_DEBUG_LOGFILE,"a")) )
	{		
		return -1;
	} 	
	fprintf(fp,"%s(%d) %s : ",buf , t , name );
	va_start(ap,fmt);
	vfprintf(fp,fmt,ap);
	va_end(ap);	
	fflush(fp);	
	fclose(fp);	
#else		
	char *ptr = buf ;
	ptr += snprintf(ptr , sizeof(buf) , "%s : " , name );
	va_start(ap,fmt);
	vsnprintf(ptr , sizeof(buf) - strlen(ptr) , fmt , ap );
	va_end(ap);
	syslog( LOG_DEBUG, "%s", buf );
#endif
	return 0;
}
