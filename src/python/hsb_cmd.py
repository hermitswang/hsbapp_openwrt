#!/usr/bin/env python3

import json

class hsb_cmd:
    def __init__(self, client, data):
        self.client = client
        self.data = data
        self.ob = json.loads(data)
        print('ob=%s' % str(self.ob))

    def get(self, key):
        if key in self.ob:
            return self.ob[key]

        return None

class hsb_reply:
    def __init(self, client, ob={}):
        self.client = client
        self.ob = ob
        self.data = json.dumps(ob)

    def set(self, key, val):
        self.ob[key] = val
