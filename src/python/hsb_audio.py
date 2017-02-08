#!/usr/bin/env python3

from unix_socket import un_sends, un_new_listen
from hsb_debug import log
import serial, threading, select, os

class hsb_asr:
    def __init__(self):
        self.name = 'asr'
        self.daemon_path = '/tmp/hsb/hsb_asr_daemon.listen'
        self.grammar_path = '/tmp/hsb/grammar'

    def enter(self):
        un_sends(self.daemon_path, 'start')

    def exit(self):
        un_sends(self.daemon_path, 'stop')

    def set_grammar(self, grammar):
        f = file(self.grammar_path, 'w+')
        f.write(grammar)
        f.close()

        un_sends(self.daemon_path, 'set_grammar')

class hsb_awaken:
    def __init__(self):
        self.name = 'awaken'
        self.daemon_path = '/tmp/hsb/hsb_awaken_daemon.listen'

    def enter(self):
        un_sends(self.daemon_path, 'start')

    def exit(self):
        un_sends(self.daemon_path, 'stop')

class hsb_call:
    def __init__(self):
        self.name = 'call'

    def enter(self):
        pass

    def exit(self):
        pass

def control_input3(on):
    if on:
        cmd = "amixer cset name='Left Output Mixer LINPUT3 Switch' on"
        os.system(cmd)
        cmd = "amixer cset name='Right Output Mixer RINPUT3 Switch' on"
        os.system(cmd)
        cmd = "amixer cset name='Left Output Mixer LINPUT3 Volume' 7"
        os.system(cmd)
        cmd = "amixer cset name='Right Output Mixer RINPUT3 Volume' 7"
        os.system(cmd)
    else:
        cmd = "amixer cset name='Left Output Mixer LINPUT3 Volume' 0"
        os.system(cmd)
        cmd = "amixer cset name='Right Output Mixer RINPUT3 Volume' 0"
        os.system(cmd)

class hsb_handsfree:
    def __init__(self):
        self.name = 'handsfree'

    def enter(self):
        control_input3(True)

    def exit(self):
        control_input3(False)

class hsb_a2dp:
    def __init__(self):
        self.name = 'a2dp'

    def enter(self):
        control_input3(True)

    def exit(self):
        control_input3(False)

class hsb_idle:
    def __init__(self):
        self.name = 'idle'

    def enter(self):
        pass

    def exit(self):
        pass

class hsb_audio(threading.Thread):
    IDLE = 0
    SLEEP = 1
    RECOGNIZE = 2
    CALL = 3
    HANDSFREE = 4
    A2DP = 5

    def __init__(self, manager):
        threading.Thread.__init__(self)
        self.manager = manager
        self.state = hsb_audio.IDLE

        statem[hsb_audio.IDLE] = hsb_idle()
        statem[hsb_audio.SLEEP] = hsb_awaken()
        statem[hsb_audio.RECOGNIZE] = hsb_asr()
        statem[hsb_audio.CALL] = hsb_call()
        statem[hsb_audio.HANDSFREE] = hsb_handsfree()
        statem[hsb_audio.A2DP] = hsb_a2dp()

        self.statem = statem

        self.uart_interface = '/dev/ttyS0'
        self.uart_baudrate = 38400

        self.bt_state = 0

    def transit(self, new_state):
        old = self.state
        if old == new_state:
            return

        statem = self.statem
        if not new_state in statem:
            return

        state_ob = statem[old]
        state_ob.exit()

        state_ob = statem[new_state]
        state_ob.enter()

        self.state = new_state

        log('transit: [%s] to [%s]' % (statem[old].name, statem[new_state].name))

    def recognize(self):
        self.transit(hsb_audio.RECOGNIZE)

    def call(self, number):
        # TODO: call other room
        pass

    def set_grammar(self, grammar):
        asr = self.statem[hsb_audio.RECOGNIZE]
        asr.set_grammar(grammar)

    def on_awaken(self):
        self.transit(hsb_audio.RECOGNIZE)

    def on_asr_result(self, result):
        log('asr result: %s' % result)
        manager.on_asr_result(result)

    def on_result(self, result):
        if result.startswith('awaken_result='):
            self.on_awaken()
        elif result.startswith('asr_result='):
            self.on_asr_result(result[len('asr_result='):])

    def on_data(self, data):
        while len(data) >= 4:
            if not (data[0] == 0x55 and data[1] == 0xAA and data[2] == 0x01):
                data = data[1:]
                continue

            old = self.bt_state
            new_state = data[3]
            if new_state < 4:
                if old >=4:
                    self.transit(hsb_audio.SLEEP)
            elif new_state >= 4 and new_state < 13:
                self.transit(hsb_audio.HANDSFREE)
            elif new_state == 13:
                self.transit(hsb_audio.A2DP)

            self.bt_state = new_state

    def exit(self):
        self._exit = True

        un_sends('/tmp/hsb/hsb_audio.listen', 'exit')
        self.join()

    def run(self):
        self.listen = un_new_listen('/tmp/hsb/hsb_audio.listen')
        self._exit = False

        if not self.listen:
            return

        uart = serial.Serial(self.uart_interface, self.uart_baudrate)
        if not uart:
            log('open uart %s fail' % self.uart_interface)
            return

        # TODO: enter sleep state

        inputs = [ self.listen, uart ]
        outputs = []

        while not self._exit:
            readable, writable, exceptional = select.select(inputs, outputs, inputs)

            if self._exit:
                break

            for s in readable:
                if s is self.listen:
                    data, addr = s.recvfrom(1024)
                    data = data.decode()

                    log('get msg: %s' % data)
                    self.on_result(data)
            elif s is uart:
                n = s.inWaiting()
                data = s.read(n)
                self.on_data(data)


