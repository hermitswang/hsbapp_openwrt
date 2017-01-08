#!/usr/bin/env python3

from hsb_debug import log
import json

class hsb_cmd:
    GET_DEVS = 'get_devices'
    SET_DEVS = 'set_devices'
    GET_SCENES = 'get_scenes'
    SET_SCENES = 'set_scenes'
    DEL_SCENES = 'del_scenes'
    ENTER_SCENE = 'enter_scene'
    ADD_DEVS = 'add_devices'
    DEL_DEVS = 'del_devices'

    def __init__(self, client, data):
        self.valid = False
        self.client = client
        self.data = data

        try:
            self.ob = json.loads(data)
        except Exception:
            log('load cmd fail: %s' % data)
            self.ob = None
        else:
            self.valid = True

    def get(self, key):
        if key in self.ob:
            return self.ob[key]

        return None

class hsb_reply:
    def __init__(self, cmd, ob):
        self.cmd = cmd
        self.client = cmd.client

        _cmd = cmd.get('cmd')
        ob['cmd'] = _cmd + '_reply'
        self.ob = ob

    def set(self, key, val):
        self.ob[key] = val

    def get(self):
        data = json.dumps(self.ob, ensure_ascii=False)
        return data

class hsb_event:
    DEVS_ONLINE = 'devices_online'
    DEVS_OFFLINE = 'devces_offline'
    DEVS_UPDATED = 'devices_updated'

    def __init__(self, cmd, ob):
        ob['cmd'] = cmd
        self.ob = ob

    def set(self, key, val):
        self.ob[key] = val

    def get(self):
        data = json.dumps(self.ob, ensure_ascii=False)
        return data



