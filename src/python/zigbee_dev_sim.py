#!/usr/bin/env python3

import struct, queue, sys
from unix_socket import un_send, un_new_listen
from sys import stdin
from select import select
from drv_orange import orange_cmd, orange_dev_type

class zigbee_dev_sim:
    magic = 0x55AA

    def __init__(self, devtype, eps, port=8000):
        dtype = ('%d' % devtype).encode()
        mac = b'\x01\x02\x03\x04\x05\x06\x07' + dtype
        self.mac = mac
        self.addr = 100 + devtype
        self.port = port
        self.eps = eps
    
        self.un_hsb_path = '/tmp/hsb/un_zigbee_test.listen'
        self.un_dev_path = '/tmp/hsb/un_zigbee_sim-%d:%d.listen' % (self.addr, self.port)

    def find_ep(self, epid):
        for ep in self.eps:
            if not 'epid' in ep:
                continue
    
            _id = ep['epid']
            if _id == epid:
                return ep
    
        return None

    def parse_data(self, data):
        if len(data) < 12:
            return
    
        words = struct.unpack('7H', data[:14])
        _magic, _len, _addr, _port, cmd, cmdlen, transid = words
    
        if _magic != zigbee_dev_sim.magic:
            return
    
        if _len - cmdlen != 8:
            return
    
        if _addr != addr or port != _port:
            return
    
        data = data[14:]
        if cmd == orange_cmd.SET:
            if len(data) < 3:
                return
    
            epdata, val = struct.unpack('HB', data[:3])
            epid = epdata & 0xFF
            ep = self.find_ep(epid)
            if not ep:
                return
    
            if val == ep['val']:
                return
    
            ep['val'] = val
            print('set ep %d to %d' % (epid, val))
            self.send_update(ep)
        if cmd == orange_cmd.DISCOVER:
            self.send_discover_resp()

    def send_discover_resp(self):
        edata = b''
        for ep in self.eps:
            epid = ep['epid']
            val = ep['val']
            edata += struct.pack('=HB', ((epid << 8) | 0xC1), val)
    
        tlen = 26 + len(edata)
    
        data = struct.pack('=7HI8s', zigbee_dev_sim.magic, tlen, self.addr, self.port, orange_cmd.DISCOVER_RESP, tlen - 8, 0, 3, self.mac)
    
        data = data + edata
    
        un_send(self.un_hsb_path, data)


    def send_keepalive(self):
        data = struct.pack('=7H', zigbee_dev_sim.magic, 14, self.addr, self.port, orange_cmd.KEEP_ALIVE, 4, 0)
    
        un_send(self.un_hsb_path, data)
    
    def parse_cmd(self, cmd):
        words = cmd.split(' ')
    
        if words[0] == 'set':
            if len(words) != 3:
                return
    
            epid = int(words[1])
            val = int(words[2])
            ep = self.find_ep(epid)
            # print(ep)
            if not ep:
                return
    
            if ep['val'] == val:
                return 
    
            ep['val'] = val
            self.send_update(ep)
    
    def send_update(self, ep):
        length = 17
        epdata = (ep['epid'] << 8) | 0x00C1
        data = struct.pack('=8HB', zigbee_dev_sim.magic, length, self.addr, self.port, orange_cmd.UPDATE, 9, 0, epdata, ep['val'])
    
        un_send(self.un_hsb_path, data)

    def run(self):
        sock = un_new_listen(self.un_dev_path)
 
        inputs = [ sock, stdin ]
        outputs = []
 
        self.send_discover_resp()
 
        while inputs:
            try:
                readable, writable, exceptional = select(inputs, outputs, inputs, 3)
            except KeyboardInterrupt:
                sys.exit(0)
 
            if not (readable or writable or exceptional):
                self.send_keepalive()
                continue
 
            for s in readable:
                if s is sock:
                    try:
                        data = s.recv(1024)
                    except Exception:
                        data = None
 
                    if data:
                        self.parse_data(data)
 
                elif s is stdin:
                    cmd = stdin.readline()
                    self.parse_cmd(cmd)
    
class zigbee_plug(zigbee_dev_sim):
    def __init__(self):
        devtype = orange_dev_type.PLUG
        eps = [ { 'epid': 0, 'val': 0 } ]
        zigbee_dev_sim.__init__(self, devtype, eps)

class zigbee_rc(zigbee_dev_sim):
    def __init__(self):
        devtype = orange_dev_type.REMOTECTL
        eps = [ { 'epid': 0, 'val': 0 } ]
        zigbee_dev_sim.__init__(self, devtype, eps)

class zigbee_relay(zigbee_dev_sim):
    def __init__(self):
        devtype = orange_dev_type.RELAY
        eps = [ { 'epid': 0, 'val': 0 }, { 'epid': 1, 'val': 0 }, { 'epid': 2, 'val': 0 }, { 'epid': 3, 'val': 0 }, { 'epid': 4, 'val': 0 }, { 'epid': 5, 'val': 0 }, { 'epid': 6, 'val': 0 }, { 'epid': 7, 'val': 0 }, { 'epid': 8, 'val': 0 }, { 'epid': 9, 'val': 0 }, { 'epid': 10, 'val': 0 }, { 'epid': 11, 'val': 0 } ]
        zigbee_dev_sim.__init__(self, devtype, eps)

class zigbee_curtain(zigbee_dev_sim):
    def __init__(self):
        devtype = orange_dev_type.CURTAIN
        eps = [ { 'epid': 0, 'val': 0 } ]
        zigbee_dev_sim.__init__(self, devtype, eps)

if __name__ == '__main__':
    import sys, getopt

    devtype = 'undefined'

    opts, args = getopt.getopt(sys.argv[1:], 't:')
    for op, value in opts:
        if op == '-t':
            devtype = value

    if devtype == 'plug':
        dev = zigbee_plug()
    if devtype == 'remotectl':
        dev = zigbee_rc()
    elif devtype == 'relay':
        dev = zigbee_relay()
    elif devtype == 'curtain':
        dev = zigbee_curtain()
    else:
        print('unknown device type')
        exit(0)

    dev.run()


