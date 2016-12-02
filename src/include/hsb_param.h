/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: hsb_param.h 1178 2007-04-03 05:18:24Z kf701 $
 * 
 */

#ifndef _HSB_PARAM_H
#define _HSB_PARAM_H 

#include <glib.h>

#define CONF_DIR	"/sysconf/"
#define MC_INI			CONF_DIR "mc.ini"

//INI文件中分隔符
#define INI_FILE_SPLIT 		','	
#define STRING_SPILIT		","

typedef struct {

} SysParam;

//全局变量，保存全部的配置信息
SysParam GL_total_cfg_info;

#define GSP GL_total_cfg_info

//从全部的配置文件中读出全部的配置信息并存储到上面声明的全局变量中
gboolean total_cfg_read(void);

//定义自己使用的配置信息读取函数
static gchar *cfg_get_value(gchar *filename, gchar *group_name, gchar *key_name);
gboolean cfg_get_string(gchar *filename, gchar *group_name, gchar *key_name ,\
						gchar **str);
gboolean cfg_get_bool(gchar *filename, gchar *group_name, gchar *key_name ,gboolean *value);
gboolean cfg_get_gint(gchar *filename, gchar *group_name, gchar *key_name ,gint *value);

gboolean cfg_get_string_list(gchar *filename, gchar *group_name, gchar *key_name,\
								gchar ***str,	gint *length );
gboolean cfg_get_gint_list(gchar *filename, gchar *group_name, gchar *key_name,\
								gint **value, gint *length);
gboolean cfg_get_bool_list(gchar *filename, gchar *group_name, gchar *key_name,\
								gboolean **value, gint *length);

//配置信息写入函数
gboolean cfg_set_string(gchar *filename, gchar *group_name, gchar *key_name ,\
						gchar *str);					
gboolean cfg_set_gint(gchar *filename, gchar *group_name, gchar *key_name ,gint value);
gboolean cfg_set_bool(gchar *filename, gchar *group_name, gchar *key_name ,gboolean value);


//释放配置参数结构体及申请的内存空间
gboolean g_config_struct_free(void);

#endif
