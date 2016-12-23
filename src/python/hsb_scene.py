#!/usr/bin/env python3


class hsb_scene_action:
    def __init__(self, devid, epid, val, delay, condition=None):
        self.devid = devid
        self.epid = epid
        self.val = val
        self.delay = delay
        self.condition = condition

    def get_ob(self):
        ob = { 'devid': self.devid, 'epid': self.epid, 'val': self.val }

        if self.delay != 0:
            ob['delay'] = self.delay

        if self.condition:
            ob['condition'] = self.condition.get_ob()

        return ob

class hsb_scene_condition:
    def __init__(self, devid, epid, expr, val):
        self.devid = devid
        self.epid = epid
        self.expr = expr
        self.val = val

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

class hsb_scene:
    def __init__(self, ob):
        self.name = ''
        self.actions = []

        self.valid = self.load(ob)

    def load(self, ob):
        actions = []
        if not ('name' in ob and 'actions' in ob):
            return False

        name = ob['name']
        acts = ob['actions']

        for act in acts:
            if not ('delay' in act and 'devid' in act and 'epid' in act and 'val' in act):
                continue

            delay = act['delay']
            devid = act['devid']
            epid = act['epid']
            val = act['val']

            action = hsb_scene_action(devid, epid, val, delay)
            actions.append(action)

            if not 'condition' in act:
                continue

            cond = act['condition']

            if not ('devid' in cond and 'epid' in cond and 'expr' in cond and 'val' in cond):
                continue

            devid = cond['devid']
            epid = cond['epid']
            expr = cond['expr']
            val = cond['val']

            conditon = hsb_scene_condition(devid, epid, expr, val)
            action.condition = condition

        self.name = name
        self.actions = actions
        return True

    def get_ob(self):
        ob = { 'name': self.name }

        actions = [ act.get_ob() for act in self.actions ]

        ob['actions'] = actions

        return ob



