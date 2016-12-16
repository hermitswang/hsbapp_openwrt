#!/usr/bin/env python3

from hsb_debug import log
from hsb_driver import hsb_driver
from hsb_device import hsb_device, hsb_endpoint
from hsb_phy import hsb_phy_enum, hsb_phy_data
import struct

class orange_cmd:
    DISCOVER = 0x9101
    DISCOVER_RESP = 0x9102
    GET = 0x9111
    SET = 0x9113
    EVENT = 0x9121
    KEEP_ALIVE = 0x9141
    REPLY = 0x9191

class ep_orange(hsb_endpoint):
    def __init__(self, ep, data=0):
        epid = ep & 0xFF
        datalen = (ep & 0x3F00) >> 8
        readable = (ep & 0x4000) > 0
        writable = (ep & 0x8000) > 0
        hsb_endpoint.__init__(self, epid, datalen, readable, writable, data)

        print('add ep: id=%d, bits=%d' % (self.epid, self.datalen))

class dev_orange_type:
    PLUG = 1
    REMOTE_CTL = 2

class dev_orange_plug(hsb_device):
    def __init__(self, driver, mac, addr, eps):
        hsb_device.__init__(self, driver, mac, addr)

        for ep in eps:
            self.add_ep(ep)
        
class dev_orange_remote_ctl(hsb_device):
    def __init__(self, driver, mac, addr, eps):
        hsb_device.__init__(self, driver, mac, addr)

        for ep in eps:
            self.add_ep(ep)

class drv_orange(hsb_driver):
    def __init__(self, manager):
        hsb_driver.__init__(self, manager)
        self.phy_name = hsb_phy_enum.ZIGBEE
        self.port = 8000

        self.data_cb = {}
        self.data_cb[orange_cmd.DISCOVER_RESP] = self.on_discover_resp
        self.data_cb[orange_cmd.EVENT] = self.on_event
        self.data_cb[orange_cmd.KEEP_ALIVE] = self.on_keepalive
        self.data_cb[orange_cmd.REPLY] = self.on_reply

    def new_device(self, dev_type, mac, addr, eps):
        if dev_type == dev_orange_type.PLUG:
            return dev_orange_plug(self, mac, addr, eps)
        elif dev_type == dev_orange_type.REMOTE_CTL:
            return dev_orange_remote_ctl(self, mac, addr, eps)
        else:
            return None

    def parse_ep(self, buf):
        data = struct.unpack('H', buf[:2])
        ep = data[0]
        ep = ep_orange(ep)

        if ep.dsize + 2 > len(buf):
            log('ep data error')
            return (None, buf)

        if ep.dsize == 1:
            ep.data = struct.unpack('B', buf[2:3])
        elif ep.dsize == 2:
            ep.data = struct.unpack('H', buf[2:4])
        elif ep.dsize == 4:
            ep.data = struct.unpack('I', buf[2:6])
        else:
            log('ep size error %d' % ep.dsize)
            return (None, buf)

        off = ep.dsize + 2
        buf = buf[off:]
        return (ep, buf)

    def errcode_convert(self, errcode):
        return errcode

    def on_discover_resp(self, addr, device, buf):
        total = len(buf)
        if total < 12:
            log('discover resp len error: %d' % total)
            return

        data = struct.unpack('i8s', buf[:12])
        dev_type, mac = data

        if device:
            return

        if total == 12:
            log('device without any ep')
            return
 
        buf = buf[12:]
        eps = []

        while len(buf) > 2:
            ep, buf = self.parse_ep(buf)

            if ep:
                eps.append(ep)

        device = self.new_device(dev_type, mac, addr, eps)
        if not device:
            log('unknown dev_type %d' % dev_type)
 
        self.add_device(device)

        log('add device %d %s' % (device.devid, device.mac))

    def on_event(self, addr, device, buf):
        eps = []
        while len(buf) > 2:
            ep, buf = self.parse_ep(buf)
            if ep:
                eps.append(eps)

        device.on_update(eps)

    def on_keepalive(self, addr, device, buf):
        device.on_keepalive()

    def on_reply(self, addr, device, buf):
        if len(buf) < 4:
            log('unknown reply, len=%d' % len(buf))
            return

        data = struct.unpack('2H', buf[:4])
        cmd, errcode = data

        ret = self.errcode_convert(errcode)

        if cmd == orange_cmd.SET:
            device.on_set_result(ret)
        elif cmd == orange_cmd.GET:
            eps = []
            buf = buf[4:]
            while len(buf) > 2:
                ep, buf = self.parse_ep(buf)
                if ep:
                    eps.append(eps)
 
            device.on_get_result(ret, eps)

    def on_data(self, phy_data):
        if not isinstance(phy_data, hsb_phy_data):
            log('drv_orange get unknown data')
            return

        log('get orange data')

        addr = phy_data.addr
        port = phy_data.port
        buf = phy_data.data

        log('addr: %d, port: %d' % (addr, port))

        total = len(buf)

        if total < 4:
            log('orange packet bad len: %d' % total)
            return

        data = struct.unpack('2H', buf[:4])
        cmd, length = data
        
        log('cmd: %x, len=%d' % (cmd, length))

        if not cmd in self.data_cb:
            log('unknown cmd 0x%x' % cmd)

        cb = self.data_cb[cmd]
        device = self.find_device(addr)

        cb(addr, device, buf[4:])


