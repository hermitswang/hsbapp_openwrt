/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * zhengjun <zhengjun@qubit-star.com>
 *
 * 
 */
 
#include "hsb_param.h"
#include "debug.h"
#include <glib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>  
#include <glib/gstdio.h>

gboolean cfg_read_from_mc(void);

gboolean total_cfg_read(void)
{
	bzero(&GL_total_cfg_info, sizeof(GL_total_cfg_info) );
	if ( !cfg_read_from_mc() )
	{
		hsb_critical("cfg_read_from_mc error !\n");
		return FALSE;
	}

	return TRUE;
}

gboolean cfg_read_from_mc(void)
{
#if 0
	gboolean read_flag = FALSE ;
	gchar **all_group;
	// Create a new and empty GKeyFile
    GKeyFile *my_key_file;

    my_key_file = g_key_file_new();
	g_key_file_set_list_separator(my_key_file,INI_FILE_SPLIT);  //设置分隔符号
    // Read all information from the ini file to GKeyFile struct
	read_flag = g_key_file_load_from_file(my_key_file, MC_INI,G_KEY_FILE_NONE,NULL);
	//Begin to get all info and store them to struct GL_total_cfg_info
	if (!read_flag)
	{
		g_key_file_free(my_key_file);
		return FALSE;
	}
	GL_total_cfg_info.server_ip = g_key_file_get_string(my_key_file,"Home","ServerIP",NULL);
	GL_total_cfg_info.server_port = g_key_file_get_integer(my_key_file,"Home","ServerPort",NULL);

	GL_total_cfg_info.hsbid = g_key_file_get_string(my_key_file,"Home","MCID",NULL);
	GL_total_cfg_info.ctrl_type = g_key_file_get_integer(my_key_file,"Home","CtrlType", 1);
	GL_total_cfg_info.serialno = g_key_file_get_integer(my_key_file,"Home","SerialNo", 0);

	GL_total_cfg_info.gateway = g_key_file_get_string(my_key_file,"Home", "GATEWAY", NULL);
	//GL_total_cfg_info.use_ssl = g_key_file_get_integer(my_key_file,"Home", "SSL", 1);
	GL_total_cfg_info.use_ssl = 0;

	if (!GL_total_cfg_info.server_ip || !GL_total_cfg_info.hsbid)
	{
		hsb_debug("read serverip or hsbid failed\n");
		return FALSE;
	}

	g_key_file_free(my_key_file);
#endif
	return TRUE;
}

//自己定义的配置读取函数
static gchar *cfg_get_value(gchar *filename, gchar *group_name, gchar *key_name)
{
	GKeyFile *my_key_file;
	gboolean read_flag;
	
	if ( filename == NULL || NULL == group_name || NULL == key_name )
	{
		hsb_warning("%s : value is null \n" , __func__ );
		return NULL;
	}
	my_key_file = g_key_file_new();
	g_key_file_set_list_separator(my_key_file,INI_FILE_SPLIT );  //设置分隔符号
    // Read all information from the ini file to GKeyFile struct
	read_flag = g_key_file_load_from_file(my_key_file,filename,G_KEY_FILE_NONE,NULL);	
	if ( !read_flag )
	{
		hsb_warning("Load ini from %s error!\n",filename);
		g_key_file_free(my_key_file);
		return NULL;
	}
	gchar *ptr = g_key_file_get_value (my_key_file ,group_name, key_name,NULL);
	if ( NULL == ptr )
	{
		hsb_warning("param error: %s %s %s\n",filename,group_name,key_name);
	}
	g_key_file_free(my_key_file);
	return ptr;	
}

gboolean cfg_get_string(gchar *filename, gchar *group_name, gchar *key_name ,\
						gchar **str)
{
	gchar *ptr = cfg_get_value(filename, group_name, key_name);
	if ( ptr )
	{
		*str = ptr;
		//snprintf(*value, slen , "%s" , ptr);
		//g_free(ptr);
		return TRUE;
	} else
		return FALSE;	
}

gboolean cfg_get_bool(gchar *filename, gchar *group_name, gchar *key_name ,gboolean *value)
{
	gchar *ptr = cfg_get_value(filename, group_name, key_name);
	if ( ptr )
	{
		*value = (gboolean)atoi(ptr);
		g_free(ptr);
		return TRUE;
	} else
		return FALSE;
}

gboolean cfg_get_gint(gchar *filename, gchar *group_name, gchar *key_name ,gint *value)
{
	gchar *ptr = cfg_get_value(filename, group_name, key_name);
	if ( ptr )
	{
		*value = (gint)atoi(ptr);
		g_free(ptr);
		return TRUE;
	} else
		return FALSE;
}

//返回的字符数组必须使用g_strfreev释放
gboolean cfg_get_string_list(gchar *filename, gchar *group_name, gchar *key_name,\
								gchar ***str, gint *length)
{
	gint i = 0;
	gchar *ptr = cfg_get_value(filename, group_name, key_name);
	if ( ptr )
	{
		*str = g_strsplit_set(ptr,STRING_SPILIT,-1);
		g_free(ptr);
		(*length) = (gint)g_strv_length(*str);
		return TRUE;
	} else
		return FALSE;
}

//value需要使用g_free命令释放
gboolean cfg_get_gint_list(gchar *filename, gchar *group_name, gchar *key_name,\
								gint **value, gint *length)
{
	gint leng_num, i = 0;
	gchar **str;
	gint *value_list;
	gchar *ptr = cfg_get_value(filename, group_name, key_name);
	if ( ptr )
	{
		str = g_strsplit_set(ptr,STRING_SPILIT,-1);
		g_free(ptr);
		leng_num = (gint)g_strv_length(str);
		value_list = g_malloc0(sizeof(gint)*leng_num);
		for ( i = 0 ; i < leng_num ; i++ )
		{
		//	printf("str=%s\n",str[i]);
			value_list[i] = atoi(str[i]) ; 
		//	printf("value_list=%d\n",value_list[i]);
		}
		g_strfreev(str);
		(*value) = value_list;
		(*length) = leng_num ; 
		return TRUE;
	} else
		return FALSE;
}

gboolean cfg_get_bool_list(gchar *filename, gchar *group_name, gchar *key_name,\
								gboolean **value, gint *length)
{
	gint leng_num, i = 0;
	gchar **str;
	gint *value_list;
	gchar *ptr = cfg_get_value(filename, group_name, key_name);
	if ( ptr )
	{
		str = g_strsplit_set(ptr,STRING_SPILIT,-1);
		g_free(ptr);
		leng_num = (gint)g_strv_length(str);
		value_list = g_malloc0(sizeof(gint)*leng_num);
		for ( i = 0 ; i < leng_num ; i++ )
		{
		//	printf("str=%s\n",str[i]);
			value_list[i] = atoi(str[i]) ; 
		//	printf("value_list=%d\n",value_list[i]);
		}
		g_strfreev(str);
		(*value) = value_list;
		(*length) = leng_num ; 
		return TRUE;
	} else
		return FALSE;
}

//自己定义的配置写入函数
gboolean cfg_set_string(gchar *filename, gchar *group_name, gchar *key_name ,\
						gchar *str)
{
	GKeyFile *my_key_file;
	gboolean read_flag;
	gsize file_size;
		
	if ( filename == NULL || NULL == group_name || NULL == key_name )
	{
		hsb_warning("%s : value is null \n" , __func__ );
		return FALSE;
	}
	my_key_file = g_key_file_new();
	g_key_file_set_list_separator(my_key_file,INI_FILE_SPLIT);  //设置分隔符号
    // Read all information from the ini file to GKeyFile struct
	read_flag = g_key_file_load_from_file(my_key_file,filename,G_KEY_FILE_KEEP_COMMENTS,NULL);	
	if ( !read_flag )
	{
		hsb_warning("Load ini from %s error!\n",filename);
		g_key_file_free(my_key_file);
		return FALSE;
	}
	gboolean has_key = g_key_file_has_key(my_key_file,group_name, key_name,NULL);
	if ( !has_key )
	{
		hsb_warning("param error: %s %s %s\n",filename,group_name,key_name);
		g_key_file_free(my_key_file);
		return FALSE;
	}
	g_key_file_set_value(my_key_file, group_name, key_name, str);
	//save back to info file
	gchar *contents = g_key_file_to_data(my_key_file,&file_size,NULL);
	FILE *fp ;
	fp = g_fopen(filename, "w");
	if ( fp == NULL )
	{
		g_free(contents);
		g_key_file_free(my_key_file);
		hsb_warning("save to file %s error\n",filename);
		return FALSE;
	}
	fwrite(contents, file_size, 1, fp);
	fclose(fp);
	g_free(contents);	
		
	g_key_file_free(my_key_file);
	return TRUE;	
}

gboolean cfg_set_gint(gchar *filename, gchar *group_name, gchar *key_name ,gint value)
{
	gchar *ptr = NULL ;
	ptr = g_strdup_printf("%d", value);
	if ( !cfg_set_string(filename, group_name, key_name, ptr) )
	{
		g_free(ptr);
		return FALSE;
	}
	g_free(ptr);
	return TRUE;
}

gboolean cfg_set_bool(gchar *filename, gchar *group_name, gchar *key_name ,gboolean value)
{	
	gchar *ptr = NULL ;
	ptr = g_strdup_printf("%d", value);
	if ( !cfg_set_string(filename, group_name, key_name, ptr) )
	{
		g_free(ptr);
		return FALSE;
	}
	g_free(ptr);
	return TRUE;
}


// free the struct of config
gboolean g_config_struct_free(void)
{
	/// TODO
	
	// free the SysParam Struct 
	bzero(&GL_total_cfg_info, sizeof(GL_total_cfg_info));	
	
	return TRUE;
}
