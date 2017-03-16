

class hsb_dev_action:
    def __init__(self, act):
        self.set_ob(act)

    def set_ob(self, act):
        self.valid = False
        if not ('devid' in act and 'epid' in act and 'val' in act):
            return

        self.devid = act['devid']
        self.epid = act['epid']
        self.val = act['val']

        self.valid = True

    def get_ob(self):
        ob = { 'devid': self.devid, 'epid': self.epid, 'val': self.val }
        return ob



