#!/usr/bin/env python3

from hsb_debug import log
from hsb_driver import hsb_driver
from hsb_device import hsb_device, hsb_endpoint, hsb_dev_type, hsb_ep_type
from hsb_phy import hsb_phy_enum, hsb_phy_data
import struct

class orange_cmd:
    DISCOVER = 0x9101
    DISCOVER_RESP = 0x9102
    GET = 0x9111
    SET = 0x9113
    UPDATE = 0x9121
    KEEP_ALIVE = 0x9141
    REPLY = 0x9191

class ep_orange(hsb_endpoint):
    def __init__(self, ep, data=0):
        epid = (ep & 0xFF00) >> 8
        bits = ep & 0x3F
        byte_num = int((bits + 7) / 8)
        readable = (ep & 0x40) > 0
        writable = (ep & 0x80) > 0

        self.bits = bits
        self.byte_num = byte_num

        hsb_endpoint.__init__(self, epid, readable, writable, data)

        self.epdata = ep

        # print('add ep: id=%d, bits=%d' % (self.epid, self.bits))

    def get_epdata(self):
        return self.epdata

    def translate(self, codec, data):
        # TODO
        return data

class dev_orange(hsb_device):
    def __init__(self, driver, mac, addr, eps):
        hsb_device.__init__(self, driver, mac, addr, eps)
        self.keepalive = True

class dev_orange_plug(dev_orange):
    def __init__(self, driver, mac, addr, eps):
        for ep in eps:
            ep.set_attr('name', 'plug endpoint')

        dev_orange.__init__(self, driver, mac, addr, eps)
        self.set_attr('name', 'plug')

        self.dev_type = hsb_dev_type.PLUG

class dev_orange_remote_ctl(dev_orange):
    def __init__(self, driver, mac, addr, eps):
        for ep in eps:
            ep.set_attr('name', 'remotectl endpoint')
            ep.eptype = hsb_ep_type.REMOTE_CTL

        dev_orange.__init__(self, driver, mac, addr, eps)
        self.set_attr('name', 'remotectl')

        self.dev_type = hsb_dev_type.REMOTE_CTL

class dev_orange_relay(dev_orange):
    def __init__(self, driver, mac, addr, eps):
        for ep in eps:
            ep.set_attr('name', 'switch endpoint') 
          
        dev_orange.__init__(self, driver, mac, addr, eps)
        self.set_attr('name', 'relay')

        self.dev_type = hsb_dev_type.RELAY

class dev_orange_curtain(dev_orange):
    def __init__(self, driver, mac, addr, eps):
        for ep in eps:
            ep.set_attr('name', 'curtain endpoint') 
            ep.set_val_range(0, 100)
          
        dev_orange.__init__(self, driver, mac, addr, eps)
        self.set_attr('name', 'curtain')

        self.dev_type = hsb_dev_type.CURTAIN

class orange_dev_type:
    PLUG = 0
    SENSOR = 1
    REMOTECTL = 2
    RELAY = 3
    CURTAIN = 4

class drv_orange(hsb_driver):
    def __init__(self, manager):
        hsb_driver.__init__(self, manager)
        self.phy_name = hsb_phy_enum.ZIGBEE
        self.port = 8000
        self.transid = 1

        self.data_cb = {}
        self.data_cb[orange_cmd.DISCOVER_RESP] = self.on_discover_resp
        self.data_cb[orange_cmd.UPDATE] = self.on_update
        self.data_cb[orange_cmd.KEEP_ALIVE] = self.on_keepalive
        self.data_cb[orange_cmd.REPLY] = self.on_reply

    def new_device(self, dev_type, mac, addr, eps):
        if dev_type == orange_dev_type.PLUG:
            return dev_orange_plug(self, mac, addr, eps)
        elif dev_type == orange_dev_type.REMOTECTL:
            return dev_orange_remote_ctl(self, mac, addr, eps)
        elif dev_type == orange_dev_type.RELAY:
            return dev_orange_relay(self, mac, addr, eps)
        elif dev_type == orange_dev_type.CURTAIN:
            return dev_orange_curtain(self, mac, addr, eps)
        else:
            return None

    def parse_ep(self, buf):
        data = struct.unpack('H', buf[:2])
        ep = data[0]
        ep = ep_orange(ep)
        byte_num = ep.byte_num

        if byte_num + 2 > len(buf):
            log('ep data error: %d/%d' % (byte_num + 2, len(buf)))
            return (None, buf)

        if byte_num == 1:
            val = struct.unpack('B', buf[2:3])
        elif byte_num == 2:
            val = struct.unpack('H', buf[2:4])
        elif byte_num == 4:
            val = struct.unpack('I', buf[2:6])
        else:
            log('ep size error %d' % ep.byte_num)
            return (None, buf)

        ep.val = val[0]

        off = byte_num + 2
        buf = buf[off:]
        return (ep, buf)

    def errcode_convert(self, errcode):
        return errcode

    def discover(self, addr):
        data = struct.pack('3H', orange_cmd.DISCOVER, 6, 0)
        phy_data = hsb_phy_data(self.phy_name, addr, self.port, data, 1)

        self.manager.dispatch(phy_data)

    def on_discover_resp(self, addr, transid, buf):
        total = len(buf)
        if total < 12:
            log('discover resp len error: %d' % total)
            return

        data = struct.unpack('i8s', buf[:12])
        dev_type, mac = data

        if total == 12:
            log('device without any ep')
            return
 
        device = self.find_device(addr)
        if device:
            return

        buf = buf[12:]
        eps = []

        while len(buf) > 2:
            ep, buf = self.parse_ep(buf)

            if not ep:
                return

            eps.append(ep)

        device = self.new_device(dev_type, mac, addr, eps)
        if not device:
            log('unknown dev_type %d' % dev_type)
            return
 
        self.add_device(device)

        log('add device %d %s' % (device.devid, device.mac))

    def on_update(self, addr, transid, buf):
        device = self.find_device(addr)
        if not device:
            self.discover()
            return

        eps = []
        while len(buf) > 2:
            ep, buf = self.parse_ep(buf)
            if ep:
                eps.append(ep)

        device.on_update(eps)

    def on_keepalive(self, addr, transid, buf):
        device = self.find_device(addr)
        if not device:
            self.discover(addr)
            return

        #log('keepalive')
        device.on_keepalive()

    def on_reply(self, addr, transid, buf):
        device = self.find_device(addr)
        if not device:
            self.discover()
            return

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
                    eps.append(ep)
 
            device.on_get_result(ret, eps)

    def on_data(self, phy_data):
        if not isinstance(phy_data, hsb_phy_data):
            log('drv_orange get unknown data')
            return

        addr = phy_data.addr
        port = phy_data.port
        buf = phy_data.data

        #log('addr: %d, port: %d' % (addr, port))

        total = len(buf)

        if total < 4:
            log('orange packet bad len: %d' % total)
            return

        data = struct.unpack('3H', buf[:6])
        cmd, length, transid = data
        
        # log('cmd: %x, len=%d' % (cmd, length))

        if not cmd in self.data_cb:
            log('unknown cmd 0x%x' % cmd)

        cb = self.data_cb[cmd]

        cb(addr, transid, buf[6:])

    def set_eps(self, device, eps):

        data = ''.encode()
        for ep in eps:
            epid = ep['epid']
            if not epid in device.eps:
                continue

            _ep = device.eps[epid]
            val = ep['val']
            data += struct.pack('H', _ep.get_epdata())
            if _ep.byte_num == 1:
                data += struct.pack('B', val)
            elif _ep.byte_num == 2:
                data += struct.pack('H', val)
            elif _ep.byte_num == 4:
                data += struct.pack('I', val)

        length = len(data) + 4
        transid = self.get_transid()

        _data = struct.pack('3H', orange_cmd.SET, length, transid) + data

        phy_data = hsb_phy_data(self.phy_name, device.addr, self.port, _data, 1)

        self.manager.dispatch(phy_data)

    def get_transid(self):
        self.transid = self.transid + 1
        return self.transid


