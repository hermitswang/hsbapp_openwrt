#!/usr/bin/env python3

from hsb_debug import log
from enum import Enum

class hsb_driver:
    def __init__(self, manager):
        self.manager = manager
        self.phy = None
        self.devices = {}

    def get_phy_name(self):
        return self.phy_name

    def get_port(self):
        return self.port

    def set_phy(self, phy):
        self.phy = phy

    def on_data(self, data):
        log('not implemented')

    def exit(self):
        pass

    def add_device(self, dev):
        devices = self.devices
        addr = dev.addr
        if addr in devices:
            log('dev %s already exists' % addr)
            return

        devices[addr] = dev

        self.manager.add_device(dev)

    def del_device(self, dev):
        devices = self.devices
        addr = dev.addr
        if not addr in devices:
            log('dev %s not exists' % addr)
            return

        del devices[addr]

        self.manager.del_device(dev)

    def find_device(self, addr):
        devices = self.devices
        if not addr in devices:
            return None

        return devices[addr]

    def timeout(self, dev):
        self.del_device(dev)

    def set_eps(self, device, ep):
        pass


