#!/usr/bin/env python3

import threading, queue

from hsb_debug import log
from hsb_config import hsb_config
from hsb_scene import hsb_scene
from hsb_phy import hsb_phy, hsb_phy_data
from hsb_cmd import hsb_cmd, hsb_reply, hsb_event
from hsb_audio import hsb_audio
from hsb_scene import hsb_scene
from hsb_device import hsb_ep_type, hsb_dev_type
from dev_ir import dev_ir

class hsb_manager(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.config = hsb_config()
        self._exit = False
        self.exit_event = threading.Event()
        self.phys = {}
        self.drivers = {}
        self.first_devid = 1
        self.devices = {}

        self.dataq = queue.Queue()
        self.data_event = threading.Event()

        self.network = None
        self.audio = None
        self.scenes = {}

    def set_network(self, network):
        self.network = network

    def set_audio(self, audio):
        self.audio = audio

    def run(self):
        self._exit = False
        event = self.data_event
        dataq = self.dataq
        devices = self.devices

        self.load_config()
        self.first_devid = self.config.get_max_devid()
        log('first devid: %d' % self.first_devid)

        while not self._exit:
            while not self._exit and dataq.empty():
                ret = event.wait(1)
                if ret:
                    event.clear()
                else:
                    self.keepalive()
                    continue

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
                self.network.on_reply(data)
            elif isinstance(data, hsb_event):
                self.network.on_event(data)
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
        supported_cmds = [ 'get_devices', 'set_devices', 'get_scenes', 'set_scenes', 'del_scene', 'enter_scene', 'add_ir_devices', 'del_ir_devices', 'get_asrkey' ]
        command = cmd.get('cmd')

        if not command in supported_cmds:
            return

        if command == 'get_devices':
            print('%d devices' % len(devices))
            obs = [ dev.get_ob() for dev in devices.values() ]

            ob = { 'devices': obs }
            reply = hsb_reply(cmd, ob)

            self.dispatch(reply)

        elif command == 'set_devices':
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
        elif command == 'get_asrkey':
            asrkey = self.get_asrkey()
            log('asrkey: %s' % asrkey)
        elif command == 'get_scenes':
            obs = [ s.get_ob() for s in self.scenes.values() ]
            ob = { 'scenes': obs }
            reply = hsb_reply(cmd, ob)

            self.dispatch(reply)

        elif command == 'set_scenes':
            scenes = cmd.get('scenes')
            if not scenes:
                return

            for s in scenes:
                scene = hsb_scene(s)
                if not scene.valid:
                    log('scene not valid')
                    continue

                self.add_scene(scene)

            self.config.save_scenes(self.scenes)
        elif command == 'del_scene':
            name = cmd.get('name')
            self.del_scene(name)

            self.config.save_scenes(self.scenes)
        elif command == 'enter_scene':
            name = cmd.get('name')
            self.enter_scene(name)
        elif command == 'add_ir_devices':
            ob = cmd.get('devices')
            if not ob:
                return

            self.add_ir_devices(ob)

        elif command == 'del_ir_devices':
            ob = cmd.get('devices')
            if not ob:
                return

            self.del_ir_devices(ob)

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

    def add_ir_devices(self, ob):
        for dev in ob:
            if not 'attrs' in dev:
                log('unknown ir device: %s' % dev)
                continue

            attrs = dev['attrs']

            if not 'irtype' in attrs:
                log('unknown ir device: %s' % dev)
                continue

            irtype = attrs['irtype']
            device = dev_ir.new_device(irtype)
            if not device:
                continue

            device.set_ob(dev)
            self.add_device(device)

    def del_ir_devices(self, ob):
        for dev in ob:
            if not 'devid' in dev:
                log('unknown ir device: %d' % dev)
                continue

            devid = dev['devid']
            device = self.find_device(devid)
            if device:
                self.del_device(device)
                self.config.del_device(device)

    def find_dummy(self, dev):
        for dev in self.devices.values():
            if dev.get_attr('location') != dev.get_attr('location'):
                continue

            eps = dev.find_eps_by_type(hsb_ep_type.REMOTE_CTL)
            if len(eps) == 0:
                continue

            return (dev, eps[0])

        return (None, None)

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

    def load_config(self):
        config = self.config
        devs = config.devices
        for dev in devs.values():
            if not ('devtype' in dev and 'devid' in dev):
                continue

            devid = dev['devid']
            devtype = dev['devtype']
            if devtype != hsb_dev_type.IR:
                continue

            if not 'attrs' in dev:
                continue

            attrs = dev['attrs']
            if not 'irtype' in attrs:
                continue

            irtype = attrs['irtype']

            device = dev_ir.new_device(irtype)
            if not device:
                continue

            device.devid = devid
            device.set_ob(dev)
            device.manager = self
            self.devices[devid] = device

            device.online()

        scenes = config.scenes
        for s in scenes.values():
            scene = hsb_scene(s)
            if not scene.valid:
                log('scene not valid')
                continue

            self.add_scene(scene)

    def alloc_dev_id(self):
        devid = self.first_devid
        self.first_devid = devid + 1
        return devid

    def add_device(self, dev):
        ob = None
        if len(dev.smac) > 0:
            ob = self.config.find_device_by_smac(dev.smac)

        if ob:
            devid = ob['devid']
            dev.set_ob(ob)
        else:
            devid = self.alloc_dev_id()

        dev.set_id(devid)

        devices = self.devices
        dev.manager = self
        devices[devid] = dev

        if not ob:
            self.config.save_devices(self.devices)

        dev.online()

    def del_device(self, dev):
        devices = self.devices
        devid = dev.get_id()
        if not devid in devices:
            log('device %d not found' % devid)

        dev.offline()
        del devices[devid]

    def find_device(self, devid):
        devices = self.devices
        if not devid in devices:
            return None

        return devices[devid]

    def add_scene(self, scene):
        name = scene.name
        if len(name) == 0:
            log('scene name null')
            return

        self.scenes[name] = scene

    def del_scene(self, name):
        if not name in self.scenes:
            log('scene %s not exists' % name)
            return

        del self.scenes[name]

    def enter_scene(self, name):
        if not name in self.scenes:
            log('scene %s not exists' % name)
            return

        scene = self.scenes[name]

        for act in scene.actions:
            cond = act.condition
            if cond:
                device = self.find_device(cond.devid)
                if not device:
                    continue

                ep = device.get_ep(cond.epid)
                if not ep:
                    continue

                result = cond.check(ep.val)
                if not result:
                    continue

            device = self.find_device(act.devid)
            if not device:
                continue

            ep = device.get_ep(act.epid)
            if not ep:
                continue

            eps = [{ 'epid': act.epid, 'val': act.val }]

            if act.delay > 0:
                def _callback(device, eps):
                    device.set_endpoints(eps)

                t = threading.Timer(act.delay, _callback, (device, eps))
                t.start()
            else:
                device.set_endpoints(eps)


    def dispatch(self, data):
        self.dataq.put(data)
        self.data_event.set()

    def keepalive(self):
        devices = [ dev for dev in  self.devices.values() if dev.keepalive ]
        for dev in devices:
            dev.add_ticks()
            if dev.ticks > 10:
                dev.offline()

    def get_asrkey(self):
        keys = []

        for dev in self.devices.values():
            key = dev.get_asrkey()
            if not key:
                continue

            keys.append(key)

        names = [ s for s in self.scenes.keys() ]
        if len(names) > 0:
            if len(names) > 1:
                key = '(%s)' % '|'.join(names)
            else:
                key = names

            act = '(进入 {act} %s {scene})' % key
            keys.append(act)

        if len(keys) > 0:
            asrkey = '|'.join(keys)
        else:
            asrkey = None

        return asrkey





