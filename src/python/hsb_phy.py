#!/usr/bin/env python3

from enum import Enum

class hsb_phy_enum(Enum):
    ZIGBEE = 'zigbee'
    WIFI = 'wifi'
    BT = 'bt'
    IR = 'IR'

class hsb_phy_data_direction:
    UP = 0
    DOWN = 1

class hsb_phy_data:
    def __init__(self, phy, addr, port, data, direction=hsb_phy_data_direction.UP):
        self.phy = phy
        self.addr = addr
        self.port = port
        self.data = data
        self.direction = direction
        if data:
            self.valid = True
        else:
            self.valid = False

    def key(self):
        return (self.phy, self.port)

class hsb_phy:
    def __init__(self, manager):
       self.manager = manager

    def write(self, data):
        pass

    def on_data_ind(self, data):
        self.manager.dispatch(data)

    def get_name(self):
        return self.name

    def exit(self):
        pass

