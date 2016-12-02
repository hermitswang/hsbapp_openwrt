
#ifndef _SCENE_H_
#define _SCENE_H_

#include "hsb_config.h"

typedef enum {
	HSB_SCENE_EXPR_EQUAL = 0,
	HSB_SCENE_EXPR_GT = 1,
	HSB_SCENE_EXPR_GE = 2,
	HSB_SCENE_EXPR_LT = 3,
	HSB_SCENE_EXPR_LE = 4,
} HSB_SCENE_EXPR_T;

typedef struct {
	uint32_t	devid;
	uint32_t	id;
	uint32_t	val;
	uint32_t	expr;
} HSB_SCENE_CONDITION_T;

typedef struct {
	uint32_t	flag;
	uint32_t	devid;
	uint32_t	id;
	uint32_t	param1;
	uint32_t	param2;
} HSB_SCENE_ACT_T;

typedef struct {
	bool			has_cond;
	HSB_SCENE_CONDITION_T	condition;
	uint32_t		delay;

	int			act_num;
	HSB_SCENE_ACT_T		acts[8];
} HSB_SCENE_ACTION_T;

typedef struct {
	char			name[HSB_SCENE_MAX_NAME_LEN];

	int			act_num;
	HSB_SCENE_ACTION_T	actions[8];
} HSB_SCENE_T;

#endif

