#!/usr/bin/env python3

from hsb_device import hsb_device


class dev_orange(hsb_device):
    def __init__(self, driver, mac):
        hsb_device.__init__(self, driver, mac)



