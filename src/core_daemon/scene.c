
#include <glib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "scene.h"
#include "hsb_error.h"
#include "device.h"
#include "debug.h"

static GQueue scene_q;
static GThreadPool *gl_thread_pool = NULL;

static void scene_handler(gpointer data, gpointer user_data);


int init_scene(void)
{
	g_queue_init(&scene_q);

	GError *error = NULL;
	gl_thread_pool = g_thread_pool_new(
					scene_handler,
					NULL,
					5,
					TRUE,
					&error);

	return HSB_E_OK;
}

static HSB_SCENE_T *find_scene(char *name)
{
	int id;
	GQueue *queue = &scene_q;
	int len = g_queue_get_length(queue);
	HSB_SCENE_T *scene = NULL;

	for (id = 0; id < len; id++) {
		scene = (HSB_SCENE_T *)g_queue_peek_nth(queue, id);
		if (0 == strcmp(scene->name, name))
			return scene;
	}

	return NULL;
}

HSB_SCENE_T *alloc_scene(void)
{
	HSB_SCENE_T *pscene = g_slice_new0(HSB_SCENE_T);

	return pscene;
}

static int free_scene(HSB_SCENE_T *scene)
{
	g_slice_free(HSB_SCENE_T, scene);

	return HSB_E_OK;
}

int add_scene(HSB_SCENE_T *scene)
{
	GQueue *queue = &scene_q;
	int id, len = g_queue_get_length(queue);
	HSB_SCENE_T *pscene = NULL;

	for (id = 0; id < len; id++) {
		pscene = (HSB_SCENE_T *)g_queue_peek_nth(queue, id);
		if (0 == strcmp(scene->name, pscene->name)) {
			g_queue_pop_nth(queue, id);
			g_queue_push_nth(queue, scene, id);
			free_scene(pscene);
			return HSB_E_OK;
		}
	}

	g_queue_push_tail(queue, scene);

	save_config();

	return HSB_E_OK;
}

int del_scene(char *name)
{
	GQueue *queue = &scene_q;
	HSB_SCENE_T *scene = find_scene(name);
	if (!scene)
		return HSB_E_BAD_PARAM;

	g_queue_remove(queue, scene);

	free_scene(scene);

	save_config();

	return HSB_E_OK;
}

int enter_scene(char *name)
{
	HSB_SCENE_T *scene = find_scene(name);
	if (!scene) {
		hsb_debug("scene [%s] not found\n", name);
		return HSB_E_BAD_PARAM;
	}

	HSB_SCENE_T *tmp = alloc_scene();
	memcpy(tmp, scene, sizeof(*tmp));

	g_thread_pool_push(gl_thread_pool, (gpointer)tmp, NULL);

	return HSB_E_OK;
}

static bool check_condition(HSB_SCENE_CONDITION_T *pcond)
{
	HSB_DEV_T *pdev = find_dev(pcond->devid);
	if (!pdev) {
		hsb_debug("check condition: dev not found\n");
		return false;
	}

	HSB_STATUS_T status = { 0 };

	if (HSB_E_OK != load_dev_status(pdev, &status))
	{
		hsb_debug("check condition: load status fail\n");
		return false;
	}

	int id;
	bool match = false;
	uint16_t val = 0;
	for (id = 0; id < status.num; id++)
	{
		if (status.id[id] != pcond->id)
			continue;

		val = status.val[id];

		switch (pcond->expr) {
			case HSB_SCENE_EXPR_EQUAL:
				if (val == pcond->val)
					return true;
				break;
			case HSB_SCENE_EXPR_GT:
				if (val > pcond->val)
					return true;
				break;
			case HSB_SCENE_EXPR_GE:
				if (val >= pcond->val)
					return true;
				break;
			case HSB_SCENE_EXPR_LT:
				if (val < pcond->val)
					return true;
				break;
			case HSB_SCENE_EXPR_LE:
				if (val <= pcond->val)
					return true;
				break;
			default:
				break;
		}

		return false;
	}

	hsb_debug("check condition: status not found\n");
	return false;
}

#if 0
static void execute_action(HSB_SCENE_ACT_T *pact)
{
	if (CHECK_BIT(pact->flag, 0)) {
		HSB_ACTION_T action;
		action.devid = pact->devid;
		action.id = pact->id;
		action.param1 = pact->param1;
		action.param2 = pact->param2;
		set_dev_action_async(&action, NULL);
	} else  {
		HSB_STATUS_T stat;
		stat.devid = pact->devid;
		stat.num = 1;
		stat.id[0] = pact->id;
		stat.val[0] = pact->param1;
		set_dev_status_async(&stat, NULL);
	}

	return;
}
#endif

static int execute_action(HSB_SCENE_ACTION_T *paction)
{
	int id, cnt, devid, num = 0;
	HSB_SCENE_ACT_T *pact = NULL;
	HSB_STATUS_T status[8] = { 0 };
	HSB_STATUS_T *pstat = NULL;

	for (id = 0; id < paction->act_num; id++)
	{
		pact = &paction->acts[id];

		devid = pact->devid;
		for (cnt = 0; cnt < num; cnt++)
		{
			pstat = &status[cnt];
			if (pstat->devid == devid)
			{
				pstat->id[pstat->num] = pact->id;
				pstat->val[pstat->num] = pact->param1;
				pstat->num++;
				break;
			}
		}

		if (cnt < num)
			continue;

		pstat = &status[num];
		pstat->devid = devid;
		pstat->id[pstat->num] = pact->id;
		pstat->val[pstat->num] = pact->param1;
		pstat->num++;

		num++;
	}

	for (id = 0; id < num; id++)
	{
		pstat = &status[id];

		set_dev_status_async(pstat, NULL);
	}

	return HSB_E_OK;
}

static void scene_handler(gpointer data, gpointer user_data)
{
	HSB_SCENE_T *scene = (HSB_SCENE_T *)data;

	int id;
	int delay = 0;
	HSB_SCENE_ACTION_T *paction = NULL;
	HSB_SCENE_CONDITION_T *pcond = NULL;

	hsb_debug("start scene [%s]\n", scene->name);

	for (id = 0; id < scene->act_num; id++) {
		paction = &scene->actions[id];

		if (paction->has_cond) {
			pcond = &paction->condition;
			if (!check_condition(pcond)) {
				hsb_debug("condition not match\n");
				continue;
			}
		}

		if (delay < paction->delay) {
			hsb_debug("sleep (%d - %d=%d)\n", paction->delay, delay, paction->delay - delay);
			sleep(paction->delay - delay);
			delay = paction->delay;
		}

		execute_action(paction);
	}

	hsb_debug("start scene done\n");

	free_scene(scene);
	return;
}

int get_scene_num(uint32_t *num)
{
	GQueue *queue = &scene_q;

	*num = g_queue_get_length(queue);

	return HSB_E_OK;
}

int get_scene(int id, HSB_SCENE_T **scene)
{
	GQueue *queue = &scene_q;
	int len = g_queue_get_length(queue);

	if (id >= len)
		return HSB_E_BAD_PARAM;

	HSB_SCENE_T *pscene = g_queue_peek_nth(queue, id);

	*scene = pscene;

	return HSB_E_OK;
}


