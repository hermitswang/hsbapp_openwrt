#!/usr/bin/env python3

from hsb_debug import log
from hsb_cmd import hsb_cmd, hsb_reply
import queue

class hsb_dev_state:
    HSB_DEV_STATE_UNINIT = 0
    HSB_DEV_STATE_ONLINE = 1
    HSB_DEV_STATE_OFFLINE = 2

class hsb_endpoint:
    def __init__(self, epid, datalen, readable, writable, data=0):
        self.datalen = datalen
        self.readable = readable
        self.writable = writable
        self.epid = epid
        self.name = ''
        self.data = data
        self.dsize = int((datalen + 7) / 8)

    def set_name(self, name):
        self.name = name

    def get_name(self):
        return self.name

class hsb_device:
    def __init__(self, driver, mac, addr):
        self.driver = driver
        self.status = None
        self.mac = mac
        self.addr = addr
        self.name = ''
        self.location = ''
        self.state = hsb_dev_state.HSB_DEV_STATE_UNINIT
        self.devid = 0
        self.eps = {}
        self.ticks = 0

    def do_reply(self, rdata):
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

    def on_event(self, eps):
        log('ep event')
        pass




