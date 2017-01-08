#!/usr/bin/env python3

from hsb_device import hsb_dev_action
from hsb_debug import log
from time import time, localtime, strftime, strptime, mktime

class hsb_dev_timer:
    ONE_SHOT = "oneshot"
    DAILY = "daily"

    def __init__(self, timer):
        self.set_ob(timer)
        self.expired = False

    # { "tmid": 0, "type": "oneshot", "date": "2017-01-05", "time": "21:30:00", "action": { "devid": 1, "epid": 0, "val": 1 } }
    def set_ob(self, timer):
        self.valid = False
        if not ('tmid' in timer and 'type' in timer and 'time' in timer and 'action' in timer):
            return

        tmid = timer['tmid']
        tmtype = timer['type']
        time = timer['time']
        action = hsb_dev_action(timer['action'])
        if not action.valid:
            return

        if tmtype == hsb_dev_timer.ONE_SHOT:
            if not 'date' in timer:
                return

            date = timer['date']
            self.date = date

        self.tmid = tmid
        self.tmtype = tmtype
        self.time = time
        self.action = action

        self.valid = True

    def get_ob(self):
        act = self.action.get_ob()
        ob = { 'tmid': self.tmid, 'type': self.tmtype, 'time': self.time, 'action': act }

        if self.tmtype == hsb_dev_timer.ONE_SHOT:
            ob['date'] = self.date

        return ob

    def update(self, now_ts, now_lt):
        if self.tmtype == hsb_dev_timer.ONE_SHOT:
            target = self.date + ' ' + self.time
            lt = strptime(target, '%Y-%m-%d %H:%M:%S')
            self.ts = mktime(lt)

            if now_lt.tm_year == lt.tm_year and now_lt.tm_mon == lt.tm_mon and now_lt.tm_mday == lt.tm_mday:
                self.expired = False
            else:
                self.expired = True
        else:
            target = '%d-%02d-%02d %s' % (now_lt.tm_year, now_lt.tm_mon, now_lt.tm_mday, self.time)
            ts = mktime(strptime(target, '%Y-%m-%d %H:%M:%S'))

            self.ts = ts
            if ts > now_ts:
                self.expired = False

    def check(self, ts):
        if self.expired:
            return False

        if ts >= self.ts:
            offset = ts - self.ts
            self.expired = True
            if offset < 3:
                return True

        return False

class hsb_timer:
    def __init__(self, manager):
        self.manager = manager
        self.timers = {}

        tm = localtime()
        self.mday = tm.tm_mday

    def get_ob(self):
        ob = [ timer.get_ob() for timer in self.timers.values() if timer.valid ]
        return ob

    def set_timers(self, timers):
        for tm in timers:
            timer = hsb_dev_timer(tm)
            if not timer.valid:
                log('timer invalid: %s' % tm)
                continue

            timer.update(int(time()), localtime())
            tmid = timer.tmid
            self.timers[tmid] = timer

    def del_timers(self, timers):
        for tm in timers:
            if not 'tmid' in tm:
                log('timer invalid: %s' % tm)
                continue

            tmid = tm['tmid']
            if tmid in self.timers:
                del self.timers[tmid]

    def check(self):
        now = int(time())
        lt = localtime()

        for timer in self.timers.values():
            if not timer.check(now):
                continue

            log('timer expiring...')
            self.manager.do_actions([ timer.action ])

        if lt.tm_mday != self.mday:
            self.mday = lt.tm_mday

            for timer in self.timers.values:
                timer.update(now, lt)



