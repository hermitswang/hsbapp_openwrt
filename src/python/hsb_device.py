#!/usr/bin/env python3

from hsb_debug import log
from hsb_cmd import hsb_cmd, hsb_reply, hsb_event
import queue

def mac_to_smac(mac):
    smac = ''
    _mac = mac[:]
    while len(_mac) > 0:
        smac += '%02X' % _mac[0]
        _mac = _mac[1:]
        if len(_mac) > 0:
            smac += ':'

    return smac

class hsb_dev_state:
    HSB_DEV_STATE_UNINIT = 0
    HSB_DEV_STATE_ONLINE = 1
    HSB_DEV_STATE_OFFLINE = 2

class hsb_dev_type:
    PLUG = 'plug'
    SENSOR = 'sensor'
    REMOTE_CTL = 'remotectl'
    IR = 'ir'

class hsb_ep_type:
    NORMAL = 'normal'
    REMOTE_CTL = 'remotectl'

class hsb_ep_val_type:
    INT = "int"
    LIST = "list"

class hsb_endpoint:
    def __init__(self, epid, readable, writable, val=0, eptype=hsb_ep_type.NORMAL):
        self.readable = readable
        self.writable = writable
        self.epid = epid
        self.val = val

        self.attrs = {}
        self.actions = {}
        self.eptype = eptype

        self.valtype = hsb_ep_val_type.LIST
        self.values = []

    def set_val_range(self, minimum, maximum, unit=''):
        self.valtype = hsb_ep_val_type.INT
        self.minimum = minimum
        self.maximum = maximum
        self.unit = unit

    def add_val(self, val, desc):
        value = { 'val': val, 'desc': desc }
        self.values.append(value)

    def add_action(self, name, val):
        self.actions[name] = val

    def get_actions(self):
        acts = [ act for act in self.actions.keys() ]
        if len(acts) == 0:
            return None

        if len(acts) > 1:
            actions = '(' + '|'.join(acts) + ')'
        else:
            actions = acts[0]

        return actions

    def get_attr(self, name):
        return self.attrs.get(name, None)

    def set_attr(self, name, val):
        self.attrs[name] = val

    def set_name(self, name):
        self.name = name

    def get_name(self):
        return self.name

    def get_ob(self):
        ob = { 'epid': self.epid, 'readable': self.readable, 'writable': self.writable, 'valtype': self.valtype }
        if self.readable:
            ob['val'] = self.val

        if len(self.attrs) > 0:
            ob['attrs'] = self.attrs

        if self.valtype == hsb_ep_val_type.LIST:
            if len(self.values) > 0:
                ob['values'] = self.values
        elif self.valtype == hsb_ep_val_type.INT:
            ob['min'] = self.minimum
            ob['max'] = self.maximum
            ob['unit'] = self.unit

        return ob

    def set_ob(self, ob):
        if 'attrs' in ob:
            self.attrs.update(ob['attrs'])

    def press_key(self, key):
        pass

class hsb_device:
    def __init__(self, driver, mac, addr, eps):
        self.driver = driver
        self.status = None
        self.mac = mac
        self.addr = addr
        self.state = hsb_dev_state.HSB_DEV_STATE_UNINIT
        self.devid = 0
        self.eps = {}
        self.ticks = 0
        self.keepalive = False

        self.cmds = { 'set_devices': self.set_ob }

        self.attrs = { 'name': '', 'location': 'unset' }

        self.channels = {}

        self.smac = mac_to_smac(mac)

        for ep in eps:
            self.add_ep(ep)

    def get_asrkey(self):
        acts = []
        for ep in self.eps.values():
            act = ep.get_actions()
            if not act:
                continue

            act = '(%s {act} %s%s {ep%d-%d})' % (act, self.get_attr('name'), ep.get_attr('name'), self.devid, ep.epid)
            acts.append(act)

        channels = [ chan for chan in self.channels ]
        if len(channels) > 0:
            if len(channels) > 1:
                chan = '(%s)' % '|'.join(channels)
            else:
                chan = channels[0]

            act = '(看 {act} %s {channel})' % chan
            acts.append(act)

        if len(acts) > 0:
            asrkey = '|'.join(acts)
        else:
            asrkey = None

        return asrkey

    def get_attr(self, name):
        return self.attrs.get(name, None)

    def set_attr(self, name, val):
        self.attrs[name] = val

    def set_channel(self, name, cid):
        self.channels[name] = cid

    def del_channel(self, name):
        if not name in self.channels:
            return

        del self.channels[name]

    def switch_channel(self, name):
        if not name in self.channels:
            return

        cid = self.channels[name]

        eps = self.find_eps_by_type(hsb_ep_type.REMOTE_CTL)
        if len(eps) < 1:
            return

        ep = eps[0]

        while len(cid) > 0:
            c = cid[:1]
            if c >= '0' and c <= '9':
                ep.press_key(c)

            cid = cid[1:]

    def upload(self, rdata):
        manager = self.manager
        manager.dispatch(rdata)

    def set_id(self, devid):
        self.devid = devid

    def get_id(self):
        return self.devid

    def add_ticks(self, tick=1):
        self.ticks += tick

    def add_ep(self, ep):
        if ep.epid in self.eps:
            log('ep %d already exists' % ep.epid)
            return

        self.eps[ep.epid] = ep

    def on_keepalive(self):
        self.ticks = 0

    def on_update(self, eps):
        log('ep event')

        endpoints = [ { 'epid': ep.epid, 'val': ep.val } for ep in eps ]

        ob = { 'devid': self.devid, 'endpoints': endpoints }

        obs = { 'devices': [ ob ] }

        event = hsb_event(hsb_event.DEVS_UPDATED, ob)
        self.upload(event)     

    def on_cmd(self, cmd, ob):
        log('on_cmd: cmd=%s ob=%s' % (cmd, str(ob)))
        if not cmd in self.cmds:
            return

        cb = self.cmds[cmd]
        cb(ob)

    def set_ob(self, ob):
        if 'attrs' in ob:
            attrs = ob['attrs']
            self.attrs.update(attrs)

        if 'endpoints' in ob:
            self.set_endpoints(ob['endpoints'])

        if 'channels' in ob:
            self.channels.update(ob['channels'])

    def get_ob(self):
        ob = { 'devid': self.devid, 'mac': self.smac, 'addr': self.addr, 'devtype': self.dev_type }
        ob['attrs'] = self.attrs

        eps = self.get_endpoints()
        if eps:
            ob['endpoints'] = eps

        if len(self.channels) > 0:
            ob['channels'] = self.channels

        return ob

    def get_ep(self, epid):
        if not epid in self.eps:
            return None

        return self.eps[epid]

    def set_endpoints(self, eps):
        hwep = []
        for ep in eps:
            if not 'epid' in ep:
                continue

            epid = ep['epid']
            if not epid in self.eps:
                continue

            endpoint = self.eps[epid]
            endpoint.set_ob(ep)

            if 'val' in ep:
                hwep.append(ep)

        if len(hwep):
            self.set_ep_val(hwep)

    def get_endpoints(self):
        return [ ep.get_ob() for ep in self.eps.values() ]

    def set_ep_val(self, eps):
        if self.driver:
            self.driver.set_eps(self, eps)

    def offline(self):
        ob = { 'devices': [ { 'devid': self.devid } ] }

        event = hsb_event(hsb_event.DEVS_OFFLINE, ob)
        self.upload(event)     

    def online(self):
        ob = self.get_ob()

        obs = { 'devices': [ ob ] }

        event = hsb_event(hsb_event.DEVS_ONLINE, obs)
        self.upload(event)

    def find_eps_by_type(self, eptype):
        return [ ep for ep in self.eps.values() if ep.eptype == eptype ]

class hsb_dev_action:
    def __init__(self, act):
        self.set_ob(act)

    def set_ob(self, act):
        self.valid = False
        if not ('devid' in act and 'epid' in act and 'val' in act):
            return

        self.devid = act['devid']
        self.epid = act['epid']
        self.val = act['val']

        self.valid = True

    def get_ob(self):
        ob = { 'devid': self.devid, 'epid': self.epid, 'val': self.val }
        return ob



