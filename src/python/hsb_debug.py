#!/usr/bin/env python3

from syslog import openlog, syslog
from os import getpid, readlink, path

log_func = print

def log_init():
    global log_func
    path = '/proc/%d/fd/0' % getpid()
    out = readlink(path)
    if out == '/dev/null':
        openlog('qb-spider')
        log_func = syslog

def log(logstr):
    log_func(logstr)

if __name__ == '__main__':
    log_init()
    log('hello world')

