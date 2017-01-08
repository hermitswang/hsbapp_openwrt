#!/usr/bin/env python3

from hsb_debug import log
from hsb_driver import hsb_driver
from hsb_device import hsb_device, hsb_endpoint, hsb_dev_type

class ep_ir(hsb_endpoint):
    def __init__(self, epid):
        hsb_endpoint.__init__(self, epid, False, True, 0)

class dev_ir_type:
    TV = 'tv'

class dev_ir(hsb_device):
    def __init__(self, eps):
        self.dev_type = hsb_dev_type.IR
        hsb_device.__init__(self, None, '', 0, eps)

    def new_device(irtype):
        if irtype == dev_ir_type.TV:
            dev = dev_ir_tv()
            return dev
        else:
            return None

    def set_ep_val(self, eps):
        pass

class dev_ir_tv(dev_ir):
    def __init__(self):
        ep = ep_ir(0)
        ep.set_attr('name', 'key')
        ep.set_attr('codec', 'NEC')

        eps = [ ep ]

        dev_ir.__init__(self, eps)

        self.set_attr('irtype', dev_ir_type.TV)

    def set_ep_val(self, eps):
        dummy_dev, dummy_ep = self.manager.find_dummy(self)
        if not dummy_dev:
            return

        if len(eps) != 1:
            return

        _ep = eps[0]
        epid, val = _ep['epid'], _ep['val']

        ep = self.eps[epid]

        # TODO
        data = dummy_ep.translate(ep.get_attr('codec'), val)
        if not data:
            return

        new_eps = [ { 'epid': dummy_ep.epid, 'val': data } ]
        dummy_dev.set_ep_val(new_eps)

    def press_key(self, key):
        pass # TODO




