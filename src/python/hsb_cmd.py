#!/usr/bin/env python3

import json

class hsb_cmd:
    def __init__(self, client, data):
        self.client = client
        self.data = data
        self.ob = json.loads(data)
        # print('ob=%s' % str(self.ob))

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
        data = json.dumps(self.ob)
        return data

class hsb_event:
    def __init__(self, cmd, ob):
        ob['cmd'] = cmd
        self.ob = ob

    def set(self, key, val):
        self.ob[key] = val

    def get(self):
        data = json.dumps(self.ob)
        return data



