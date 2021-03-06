#!/usr/bin/env python3

import sys, socket, json, struct
from select import select

def probe_hsb():
    addr = ('<broadcast>', 18000)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    data = 'are you hsb?'
    s.sendto(data.encode(), addr)

    reply, servaddr = s.recvfrom(1024)
    print('get reply from %s: %s' % (servaddr, reply.decode()))
    s.close()

    return servaddr

def parse_data(data):
    while len(data) > 4:
        hdr, length = struct.unpack('=2H', data[:4])
        if hdr != 0x55AA:
            log('bad magic message')
            break

        if length <= 4 or length > len(data):
            log('bad length %d' % length)
            break

        cmd = data[4:length].decode()
        print(cmd)

        data = data[length:]

def parse_cmd(cmd, sock):
    cmd = cmd[:-1]

    if cmd.startswith('raw='):
        content = cmd[len('raw='):]
        data = struct.pack('=2H', 0x55AA, len(content) + 4) + content.encode()
        try:
            sock.send(data)
        except Exception:
            sys.exit(0)

        return

    words = [ word for word in cmd.split(' ') if len(word) > 0 ]
    if len(words) == 0:
        return

    cmd = ' '.join(words)

    if cmd == 'get devices':
        ob = { 'cmd': 'get_devices' }
    elif cmd.startswith('set device'):
        if len(words) < 5:
            return

        devid = int(words[2])
        epid = int(words[3])
        val = int(words[4])
        ob = { 'cmd': 'set_devices', 'devices': [ { 'devid': devid, 'endpoints': [ { 'epid': epid, 'val': val } ] } ] }
    else:
        print('unknown cmd: %s' % words)
        return

    content = json.dumps(ob, ensure_ascii=False)
    print(content)

    data = struct.pack('=2H', 0x55AA, len(content) + 4) + content.encode()

    try:
        sock.send(data)
    except Exception:
        sys.exit(0)


if __name__ == '__main__':

    ip, port = probe_hsb()

    servaddr = (ip, 18002)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        sock.connect(servaddr)
    except Exception as e:
        print(e)
        sys.exit(0)

    inputs = [ sock, sys.stdin ]
    outputs = []

    while inputs:
        try:
            readable, writable, exceptional = select(inputs, outputs, inputs)
        except KeyboardInterrupt:
            sys.exit(0)

        for s in readable:
            if s is sock:
                try:
                    data = s.recv(1024)
                except Exception:
                    data = None

                if data:
                    parse_data(data)
                else:
                    print('disconnected')
                    s.close()
                    sys.exit(0)

            elif s is sys.stdin:
                cmd = sys.stdin.readline()
                print(cmd)
                parse_cmd(cmd, sock)



