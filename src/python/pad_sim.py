#!/usr/bin/env python3

import sys, socket, json

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

def parse_cmd(cmd):
    words = [ word for word in cmd.split(' ') if len(word) > 0 ]
    if len(words) == 0:
        return None

    if words[0] == '1':
        ob = { 'cmd': 'set', 'devices': [ { 'devid': 1, 'endpoints': [ { 'epid': 0, 'val': 1 } ] } ] }

        content = json.dumps(ob)

        print(content)
        return content

    return None

if __name__ == '__main__':

    ip, port = probe_hsb()

    servaddr = (ip, 18002)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        s.connect(servaddr)
    except Exception as e:
        print(e)
        sys.exit(0)

    while True:
        try:
            cmd = input('> ')
        except KeyboardInterrupt:
            s.close()
            sys.exit(0)

        if len(cmd) == 0:
            continue

        cmd = parse_cmd(cmd)
        if not cmd:
            continue

        try:
            s.send(cmd.encode())
        except BrokenPipeError:
            print('connect broken')
            sys.exit(0)

