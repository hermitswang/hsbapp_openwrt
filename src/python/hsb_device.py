#!/usr/bin/env python3

from hsb_debug import log
from hsb_cmd import hsb_cmd, hsb_reply, hsb_event
import queue

class hsb_dev_state:
    HSB_DEV_STATE_UNINIT = 0
    HSB_DEV_STATE_ONLINE = 1
    HSB_DEV_STATE_OFFLINE = 2

class hsb_endpoint:
    def __init__(self, epid, bits, readable, writable, val=0):
        self.bits = bits
        self.readable = readable
        self.writable = writable
        self.epid = epid
        self.val = val
        self.byte_num = int((bits + 7) / 8)

        self.attrs = {}

    def get_attr(self, name):
        return self.attrs.get(name, None)

    def set_attr(self, name, val):
        self.attrs[name] = val

    def set_name(self, name):
        self.name = name

    def get_name(self):
        return self.name

    def get(self):
        ob = { 'epid': self.epid, 'bits': self.bits, 'readable': self.readable, 'writable': self.writable }
        if self.readable:
            ob['val'] = self.val

        if len(self.attrs) > 0:
            ob['attrs'] = self.attrs

        return ob

    def set(self, ob):
        if 'attrs' in ob:
            self.attrs.update(ob['attrs'])

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

        self.cmds = { 'set_devices': self.set }

        self.attrs = { 'name': '', 'location': '' }

        smac = ''
        _mac = mac[:]
        while len(_mac) > 0:
            smac += '%02X' % _mac[0]
            _mac = _mac[1:]
            if len(_mac) > 0:
                smac += ':'

        self.smac = smac

        for ep in eps:
            self.add_ep(ep)

    def get_attr(self, name):
        return self.attrs.get(name, None)

    def set_attr(self, name, val):
        self.attrs[name] = val

    def upload(self, rdata):
        manager = self.driver.manager
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

        endpoints = []
        for ep in eps:
            endpoints.append({ 'epid': ep.epid, 'val': ep.val })
        ob = { 'devid': self.devid, 'endpoints': endpoints }

        event = hsb_event('update', ob)
        self.upload(event)     

    def on_cmd(self, cmd, ob):
        log('on_cmd: cmd=%s ob=%s' % (cmd, str(ob)))
        if not cmd in self.cmds:
            return

        cb = self.cmds[cmd]
        cb(ob)

    def set(self, ob):
        if 'attrs' in ob:
            attrs = ob['attrs']
            self.attrs.update(attrs)

        if 'endpoints' in ob:
            self.set_endpoints(ob['endpoints'])

    def get(self):
        ob = { 'devid': self.devid, 'mac': self.smac, 'addr': self.addr }
        ob['attrs'] = self.attrs

        eps = self.get_endpoints()
        if eps:
            ob['endpoints'] = eps

        return ob

    def set_endpoints(self, eps):
        hwep = []
        for ep in eps:
            if not 'epid' in ep:
                continue

            epid = ep['epid']
            if not epid in self.eps:
                continue

            endpoint = self.eps[epid]
            endpoint.set(ep)

            if 'val' in ep:
                hwep.append(ep)

        if len(hwep):
            self.driver.set_eps(self, hwep)

    def get_endpoints(self):
        return [ ep.get() for ep in self.eps.values() ]

    def timeout(self):
        self.driver.del_device(self)

        ob = { 'devid': self.devid}

        event = hsb_event('offline', ob)
        self.upload(event)     

    def online(self):
        ob = self.get()

        event = hsb_event('online', ob)
        self.upload(event)



