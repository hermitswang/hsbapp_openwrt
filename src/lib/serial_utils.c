
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/time.h>
#include <time.h>
#include <glib.h>

/***@brief  设置串口通信速率
*@param  fd     类型 int  打开串口的文件句柄
*@param  speed  类型 int  串口速度
*@return  void*/

int speed_arr[] = { B115200 , B38400, B19200, B9600, B4800, B2400, B1200, B300,
	    B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int name_arr[] = { 115200 , 38400,  19200,  9600,  4800,  2400,  1200,  300,
	    38400,  19200,  9600, 4800, 2400, 1200,  300, };
static void set_speed(int fd, int speed)
{
	int   i;
	int   status;
  struct termios   Opt;
  tcgetattr(fd, &Opt);
  for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)
   {
   	if  (speed == name_arr[i])
   	{
		printf("speed = %d\n" ,speed);
   	    tcflush(fd, TCIOFLUSH);
    	cfsetispeed(&Opt, speed_arr[i]);
    	cfsetospeed(&Opt, speed_arr[i]);
    	status = tcsetattr(fd, TCSANOW, &Opt);
    	if  (status != 0)
            perror("tcsetattr fd1");
     	return;
     	}
   tcflush(fd,TCIOFLUSH);
   }
}
/**
*@brief   设置串口数据位，停止位和效验位
*@param  fd     类型  int  打开的串口文件句柄*
*@param  databits 类型  int 数据位   取值 为 7 或者8*
*@param  stopbits 类型  int 停止位   取值为 1 或者2*
*@param  parity  类型  int  效验类型 取值为N,E,O,,S
*/
static int set_Parity(int fd,int databits,int stopbits,int parity)
{
	struct termios options;
 if  ( tcgetattr( fd,&options)  !=  0)
  {
  	perror("SetupSerial 1");
  	return 0;
  }
  options.c_cflag &= ~CSIZE;
  switch (databits) /*设置数据位数*/
  {
  	case 7:
  		options.c_cflag |= CS7;
  		break;
  	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr,"Unsupported data size\n");
		return 0;
	}
  switch (parity)
  	{
  	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;   /* Clear parity enable */
		options.c_iflag &= ~INPCK;     /* Enable parity checking */
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);  /* 设置为奇效验*/ 
		options.c_iflag |= INPCK;             /* Disnable parity checking */
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;     /* Enable parity */
		options.c_cflag &= ~PARODD;   /* 转换为偶效验*/  
		options.c_iflag |= INPCK;       /* Disnable parity checking */
		break;
	case 'S':
	case 's':  /*as no parity*/
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
	default:
		fprintf(stderr,"Unsupported parity\n");
		return 0;
		}
  /* 设置停止位*/   
  switch (stopbits)
  	{
  	case 1:
  		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr,"Unsupported stop bits\n");
		return 0;
	}
  /* Set input parity option */
  if (parity != 'n')
  		options.c_iflag |= INPCK;
    options.c_cc[VTIME] = 150; // 15 seconds
    options.c_cc[VMIN] = 0;

#if 1
  options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG );  /*Input*/
  options.c_iflag  &= ~(ISTRIP|IUCLC|IGNCR|ICRNL|INLCR|IXON|PARMRK );  /*Input*/
  options.c_oflag  &= ~(OPOST | IXON | IXOFF );   /*Output*/
#endif

  tcflush(fd,TCIFLUSH); /* Update the options and do it NOW */
  if (tcsetattr(fd,TCSANOW,&options) != 0)
  	{
  		perror("SetupSerial 3");
		return 0;
	}
  return 1;
 }
/**
*@breif 打开串口
*/
static int OpenDev(char *Dev, int flags)
{
	int	fd = open( Dev, flags );         //| O_NOCTTY | O_NDELAY
	if (-1 == fd) { /*设置数据位数*/
			return -1;
	} else
		return fd;

}

/*
 * arguments
 *		dev: /dev/ttyM0 - /dev/ttyM7
 *		baud_rate: 4800, 9600, 115200, ...
 *		flags: O_RDONLY, O_WRONLY, O_RDWR, ...
 * return
 * 		-1: error
 * 		>0: fd
 */
int open_serial_fd(char *dev, unsigned long baud_rate)
{
	int fd;
	if (!dev) {
		printf("no device\n");
		return -1;
	}

	if (!baud_rate)
		baud_rate = 115200;
	
	if ( (fd = OpenDev(dev, O_RDWR)) < 0 ) {
		printf("open serial port error\n");
		return -1;
	}

	set_speed(fd, baud_rate);
	if (!set_Parity(fd, 8, 1, 'n')) {
		close(fd);
		return -1;
	}

	printf("open %s success\n", dev);
	return fd;
}

