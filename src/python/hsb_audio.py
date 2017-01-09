#!/usr/bin/env python3

from unix_socket import un_send, un_new_listen
from hsb_debug import log
import threading, select

class hsb_audio(threading.Thread):
    IDLE = 1
    AWAKEN = 2
    ASR = 3

    def __init__(self, manager):
        threading.Thread.__init__(self)
        self.manager = manager
        self.state = hsb_audio.IDLE

    def start_asr(self):
        if not self.state == hsb_audio.IDLE:
            log('audio state not IDLE')
            return

        un_send('/tmp/hsb/hsb_asr_daemon.listen', 'start')
        self.state = hsb_audio.ASR

    def set_grammar(self, grammar):
        if not self.state == hsb_audio.IDLE:
            log('audio state not IDLE')
            return

        f = file('/tmp/hsb/grammar', 'w+')
        f.write(grammar)
        f.close()

        un_send('/tmp/hsb/hsb_asr_daemon.listen', 'set_grammar=/tmp/hsb/grammar')

    def stop_asr(self):
        if not self.state == ASR:
            log('audio state not ASR')
            return

        un_send('/tmp/hsb/hsb_asr_daemon.listen', 'stop')
        self.state = hsb_audio.IDLE

    def start_awaken(self):
        if not self.state == hsb_audio.IDLE:
            log('audio state not IDLE')
            return

        un_send('/tmp/hsb/hsb_awaken_daemon.listen', 'start')
        self.state = hsb_audio.AWAKEN

    def stop_awaken(self):
        if not self.state == hsb_audio.AWAKEN:
            log('audio state not AWAKEN')
            return

        un_send('/tmp/hsb/hsb_awaken_daemon.listen', 'stop')
        self.state = hsb_audio.IDLE

    def on_awaken(self):
        self.start_asr()

    def on_asr_result(self, result):
        log('asr result: %s' % result)
        manager.on_asr_result(result)

    def on_result(self, result):
        if result.startswith('awaken_result='):
            self.on_awaken()
        elif result.startswith('asr_result='):
            self.on_asr_result(result[len('asr_result='):])

    def run(self):
        self.listen = un_new_listen('/tmp/hsb/hsb_audio.listen')
        self.exit = False

        if not self.listen:
            return

        inputs = [ self.listen ]
        outputs = []

        while not self.exit:
            readable, writable, exceptional = select.select(inputs, outputs, inputs)

            if self.exit:
                break

            for s in readable:
                if s is self.listen:
                    data, addr = s.recvfrom(1024)
                    data = data.decode()

                    self.on_result(data)

