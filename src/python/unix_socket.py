#!/usr/bin/env python3

import socket, os

def un_new_listen(path):
    unsock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    if os.path.exists(path):
        os.unlink(path)

    if os.path.exists(path):
        print('unix socket %s unlink fail' % path)
        return None

    try:
        unsock.bind(path)
    except Exception as e:
        print('unix socket bind fail: %s' % e)
        return None

    return unsock

def un_send(path, data):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        s.sendto(data, path)
    except Exception:
        return False

    return True


