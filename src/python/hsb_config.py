#!/usr/bin/env python3

from hsb_debug import log
import json
from hsb_device import mac_to_smac

class hsb_config:
    def __init__(self, workdir='.'):
        self.workdir = workdir
        devices = self.load(workdir + '/devices.json')
        scenes = self.load(workdir + '/scenes.json')

        if devices:
            self.devices = devices
        else:
            self.devices = {}

        if scenes:
            self.scenes = scenes
        else:
            self.scenes = {}

    def load(self, path):
        try:
            cfg = open(path, 'r')
        except Exception as e:
            # log(e)
            return

        try:
            data = cfg.read()
        except Exception:
            log('read config failed')
            return None
        finally:
            cfg.close()

        try:
            ob = json.loads(data)
        except Exception:
            log('json parse failed')
            return None

        return ob

    def save(self, path, ob):
        try:
            cfg = open(path, 'w')
        except Exception as e:
            log(e)
            return

        try:
            data = json.dumps(ob)
        except Exception as e:
            log(e)
            cfg.close()
            return

        try:
           cfg.write(data)
        except Exception as e:
            log(e)
            return
        finally:
            cfg.close()

    def save_devices(self, obs):
        _obs = { dev.devid: dev.get_ob() for dev in obs.values() }

        self.devices.update(_obs)
        self.save('devices.json', self.devices)

    def save_scenes(self, obs):
        _obs = { s.name: s.get_ob() for s in obs.values() }
        self.scenes = _obs
        self.save('scenes.json', _obs)

    def find_device_by_smac(self, smac):
        for dev in self.devices.values():
            if smac == dev['mac']:
                return dev

        return None

    def del_device(self, dev):
        devid = str(dev.devid)
        if not devid in self.devices:
            return

        del self.devices[devid]
        self.save('devices.json', self.devices)

    def get_max_devid(self):
        s = [ int(devid) for devid in self.devices.keys() ]
        if len(s) > 0:
            return max(s) + 1
        else:
            return 1



