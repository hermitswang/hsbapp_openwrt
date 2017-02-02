#!/usr/bin/env python3

import struct, queue, sys
from unix_socket import un_send, un_new_listen
from sys import stdin
from select import select
from drv_orange import orange_cmd

magic = 0x55AA
addr = 100
port = 8000

eps = [ { 'epid': 0, 'val': 0 } ]

def find_ep(epid):
    for ep in eps:
        if not 'epid' in ep:
            continue

        _id = ep['epid']
        if _id == epid:
            return ep

    return None

def parse_data(data):
    if len(data) < 12:
        return

    words = struct.unpack('7H', data[:14])
    _magic, _len, _addr, _port, cmd, cmdlen, transid = words

    if _magic != magic:
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
        ep = find_ep(epid)
        if not ep:
            return

        if val == ep['val']:
            return

        ep['val'] = val
        print('set ep %d to %d' % (epid, val))
        send_update(ep)
    if cmd == orange_cmd.DISCOVER:
        send_discover_resp()

def send_discover_resp():
    ep = eps[0]
    data = struct.pack('7HI8sHB', magic, 29, addr, port, orange_cmd.DISCOVER_RESP, 21, 0, 0, b'\x01\x02\x03\x04\x05\x06\x07\x08', 0xC100, ep['val'])

    un_send('/tmp/hsb/un_zigbee_test.listen', data)

def send_keepalive():
    ep = eps[0]
    data = struct.pack('7H', magic, 14, addr, port, orange_cmd.KEEP_ALIVE, 4, 0)

    un_send('/tmp/hsb/un_zigbee_test.listen', data)

def parse_cmd(cmd):
    words = cmd.split(' ')

    if words[0] == 'set':
        if len(words) != 3:
            return

        epid = int(words[1])
        val = int(words[2])
        ep = find_ep(epid)
        print(ep)
        if not ep:
            return

        if ep['val'] == val:
            return 

        ep['val'] = val
        send_update(ep)

def send_update(ep):
    length = 17
    epdata = ep['epid'] | 0xC100
    data = struct.pack('8HB', magic, length, addr, port, orange_cmd.UPDATE, 9, 0, epdata, ep['val'])

    un_send('/tmp/hsb/un_zigbee_test.listen', data)

if __name__ == '__main__':
    path = '/tmp/hsb/un_zigbee_test2.listen'

    sock = un_new_listen(path)

    inputs = [ sock, stdin ]
    outputs = []

    send_discover_resp()

    while inputs:
        try:
            readable, writable, exceptional = select(inputs, outputs, inputs, 3)
        except KeyboardInterrupt:
            sys.exit(0)

        if not (readable or writable or exceptional):
            send_keepalive()
            continue

        for s in readable:
            if s is sock:
                try:
                    data = s.recv(1024)
                except Exception:
                    data = None

                if data:
                    parse_data(data)

            elif s is stdin:
                cmd = stdin.readline()
                parse_cmd(cmd)


 
