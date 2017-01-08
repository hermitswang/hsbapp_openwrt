#!/usr/bin/env python3

import threading
from hsb_debug import log
from hsb_device import hsb_dev_action

class hsb_scene_condition:
    def __init__(self, condition):
        self.set_ob(condition)

    def set_ob(self, cond):
        self.valid = False
        if not ('devid' in cond and 'epid' in cond and 'expr' in cond and 'val' in cond):
            return

        self.devid = cond['devid']
        self.epid = cond['epid']
        self.expr = cond['expr']
        self.val = cond['val']

        self.valid = True

    def get_ob(self):
        ob = { 'devid': self.devid, 'epid': self.epid, 'expr': self.expr, 'val': self.val }
        return ob

    def check(self, val):
        expr = self.expr
        if expr == 'gt' and val > self.val:
            return True
        elif expr == 'lt' and val < self.val:
            return True
        elif expr == 'eq' and val == self.val:
            return True
        elif expr == 'ge' and val >= self.val:
            return True
        elif expr == 'le' and val <= self.val:
            return True

        return False

class hsb_scene_action:
    def __init__(self, act):
        self.set_ob(act)

    def set_ob(self, act):
        self.valid = False
        self.condition = None
        self.delay = act.get('delay', 0)

        if not 'actions' in act:
            return

        acts = act['actions']

        dev_acts = [ hsb_dev_action(action) for action in acts ]

        self.actions = dev_acts

        if 'condition'in act:
            condition = hsb_scene_condition(act['condition'])
            if condition.valid:
                self.condition = condition
            else:
                log('invalid condition')

        self.valid = True

    def get_ob(self):
        ob = {}

        if self.delay != 0:
            ob['delay'] = self.delay

        if self.condition:
            ob['condition'] = self.condition.get_ob()

        ob['actions'] = [ act.get_ob() for act in self.actions ]

        return ob

class hsb_scene:
    def __init__(self, manager, ob):
        self.manager = manager
        self.name = ''
        self.actions = []

        self.valid = self.load(ob)

    def load(self, ob):
        if not ('name' in ob and 'actions' in ob):
            return False

        name = ob['name']
        acts = ob['actions']

        actions = [ hsb_scene_action(act) for act in acts ]

        self.name = name
        self.actions = actions
        return True

    def get_ob(self):
        ob = { 'name': self.name }

        actions = [ act.get_ob() for act in self.actions ]

        ob['actions'] = actions

        return ob

    def enter(self):
        manager = self.manager
        for act in self.actions:  # hsb_scene_action
            cond = act.condition
            if cond:
                device = manager.find_device(cond.devid)
                if not device:
                    continue

                ep = device.get_ep(cond.epid)
                if not ep:
                    continue

                result = cond.check(ep.val)
                if not result:
                    continue

            delay = act.delay
            actions = act.actions
    
            def _callback(_acts):
                manager.do_actions(_acts)

            if delay > 0:
                t = threading.Timer(delay, _callback, (actions,))
                t.start()
            else:
                _callback(actions)



