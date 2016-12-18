#!/usr/bin/env python3

from hsb_debug import log
from unix_socket import un_send, un_new_listen
from hsb_phy import hsb_phy, hsb_phy_enum, hsb_phy_data
import serial, queue, threading, select, struct


class phy_data_zigbee(hsb_phy_data):
    def __init__(self, data):
        self.phy_zigbee_magic = 0x55AA
        if isinstance(data, hsb_phy_data):
            direction = 1
        else:
            direction = 0

        if 0 == direction:
            self.raw_data = data
            addr, port, data = self.parse_header(data)
            hsb_phy_data.__init__(self, hsb_phy_enum.ZIGBEE, addr, port, data, direction)
        else:
            addr, port, _data = data.addr, data.port, data.data
            self.raw_data = self.make_header(addr, port, _data)
            hsb_phy_data.__init__(self, hsb_phy_enum.ZIGBEE, addr, port, _data, direction)

    def make_header(self, addr, port, data):
        length = 8 + len(data)
        _data = struct.pack('4H', self.phy_zigbee_magic, length, addr, port)
        return _data + data

    def parse_header(self, data):
        length = len(data)
        if length < 8:
            return (None, None, None)

        header = data[:8]

        h = struct.unpack('4h', header)
        if h[0] != self.phy_zigbee_magic:
            log('invalid magic %s' % h[0])
            return (None, None, None)

        if length != h[1]:
            log('bad len %d/%d' % (h[1], length))
            return (None, None, None)
   
        addr = h[2]
        port = h[3]

        return (addr, port, data[8:])

class phy_zigbee(hsb_phy):
    def __init__(self, manager):
        hsb_phy.__init__(self, manager)
        self.name = hsb_phy_enum.ZIGBEE

        self.uart_interface = '/dev/ttyS1'
        self.uart_baudrate = 115200

        self._exit = False
        self.un_path = '/tmp/hsb/un_phy_zigbee.listen'
        self.outq = queue.Queue()

        t = threading.Thread(target=self.work_proc)
        t.start()
        self.work_thread = t

    def write(self, data):
        _data = phy_data_zigbee(data)
        self.outq.put(_data.raw_data)
        log(_data.raw_data)

    def on_data(self, data):
        phy_data = phy_data_zigbee(data)

        if not phy_data.valid:
            return

        self.on_data_ind(phy_data)

    def exit(self):
        self._exit = True
        un_send(self.un_path, 'notify'.encode())
        self.work_thread.join()

    def work_proc(self):
        un_sock = un_new_listen(self.un_path)
        if not un_sock:
            log('un_sock fail')
            return

        test = True
        if not test:
            uart = serial.Serial(self.uart_interface, self.uart_baudrate)
            if not uart:
                log('open uart fail')
                return
        else: # test
            test_path = '/tmp/hsb/un_zigbee_test.listen'
            uart = un_new_listen(test_path)

        inputs = [ un_sock, uart ]
        outputs = []

        while not self._exit:
            readable, writable, exceptional = select.select(inputs, outputs, inputs)

            for s in readable:
                if s is un_sock:
                    data, addr = s.recvfrom(1024)
                    if not self.outq.empty():
                        if not uart in outputs:
                            outputs.append(uart)

                elif s is uart:
                    if isinstance(s, serial.Serial):
                        data = s.read(1024)
                    else:
                        data, addr = s.recvfrom(1024)

                    self.on_data(data)

            for s in writable:
                if s is uart:
                    try:
                        data = self.outq.get_nowait()
                    except queue.Empty:
                        outputs.remove(s)
                    else:
                        if isinstance(s, serial.Serial):
                            s.write(data)
                        else:
                            s.sendto(data.encode(), test_path)



