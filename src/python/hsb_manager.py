#!/usr/bin/env python3

import threading, queue

from hsb_debug import log
from hsb_channel import hsb_channel
from hsb_config import hsb_config
from hsb_scene import hsb_scene
from hsb_phy import hsb_phy, hsb_phy_data
from hsb_cmd import hsb_cmd

class hsb_manager(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.channel = hsb_channel()
        self.config = hsb_config()
        self.scene = hsb_scene()
        # self.zigbee = hsb_zigbee()
        self._exit = False
        self.exit_event = threading.Event()
        self.phys = {}
        self.drivers = {}
        self.first_dev_id = 1
        self.devices = {}

        self.dataq = queue.Queue()
        self.data_event = threading.Event()

        self.network = None

    def set_network(self, network):
        self.network = network

    def run(self):
        self._exit = False
        event = self.data_event
        dataq = self.dataq
        devices = self.devices

        while not self._exit:
            while not self._exit and dataq.empty():
                if event.wait():
                    event.clear()

            if self._exit:
                break

            try:
                data = dataq.get_nowait()
            except queue.Empty:
                log('dataq empty')
                continue

            if isinstance(data, hsb_phy_data):
                if data.direction == 0:
                    key = data.key()
                    drivers = self.drivers
                    if not key in drivers:
                        log('key %s not found in drivers' % str(key))
                        continue
    
                    driver = drivers[key]
                    driver.on_data(data)
                else:
                    phys = self.phys
                    phy_name = data.phy
                    if not phy_name in phys:
                        log('phy %s not found in phys' % phy_name)
                        continue

                    phy = phys[phy_name]
                    phy.write(data)
            elif isinstance(data, hsb_cmd):
                self.deal_hsb_cmd(data)
            elif isinstance(data, hsb_reply):
                network.on_reply(data)
            else:
                log('manager: unknown data')

        # log('hsb_manager exit')

    def exit(self):
        self._exit = True
        self.data_event.set()
        self.join()

        for key in self.drivers:
            drv = self.drivers[key]
            drv.exit()

        for key in self.phys:
            phy = self.phys[key]
            phy.exit()

    def deal_hsb_cmd(self, cmd):
        devices = self.devices
        supported_cmds = [ 'set', 'get' ]
        command = cmd.get('cmd')

        if not command in supported_cmds:
            log('unsupported cmd: %s' % command)
            return

        devs = cmd.get('devices')
        if not devs:
            log('devices not found')
            return

        for dev in devs:
            devid = dev.get('devid', -1)
            if devid < 0:
                continue

            device = self.find_device(devid)
            if not device:
                continue

            device.on_cmd(command, dev)

    def add_phy(self, phy):
        phys = self.phys
        name = phy.get_name()

        if not name in phys:
            phys[name] = phy
        else:
            log('phy %s already exists' % name)

    def del_phy(self, phy):
        phys = self.phys
        name = phy.get_name()

        if not name in phys:
            log('phy %s not exists' % name)
            return

        del phys[name]

    def add_driver(self, drv):
        drvs = self.drivers
        phy_name = drv.get_phy_name()
        port = drv.get_port()
        phys = self.phys

        if not phy_name in phys:
            log('driver phy name %s not found' % phy_name)
            return

        key = (phy_name, port)
        if key in drvs:
            log('driver %s already exists' % str(key))
            return

        drvs[key] = drv
        drv.set_phy(phys[phy_name])

    def del_driver(self, drv):
        drvs = self.drivers
        key = (drv.get_phy_name(), drv.get_port())

        if not key in drvs:
            log('driver key %s not exists' % str(key))
            return

        del drvs[key]

    def alloc_dev_id(self):
        devid = self.first_dev_id
        self.first_dev_id = devid + 1
        return devid

    def add_device(self, dev):
        devid = self.alloc_dev_id()
        dev.set_id(devid)

        devices = self.devices
        devices[devid] = dev

    def del_device(self, dev):
        devices = self.devices
        devid = dev.get_id()
        if not devid in devices:
            log('device %d not found' % devidi)

        del devices[devid]

    def find_device(self, devid):
        devices = self.devices
        if not devid in devices:
            return None

        return devices[devid]

    def dispatch(self, data):
        self.dataq.put(data)
        self.data_event.set()



