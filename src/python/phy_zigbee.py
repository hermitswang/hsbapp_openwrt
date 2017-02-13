#!/usr/bin/env python3

from hsb_debug import log
from unix_socket import un_send, un_sends, un_new_listen
from hsb_phy import hsb_phy, hsb_phy_enum, hsb_phy_data
import serial, queue, threading, select, struct


class phy_data_zigbee(hsb_phy_data):
    MAGIC = 0x55AA
    def __init__(self, data):
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

    def check_magic(data):
        length = len(data)
        if length < 8:
            return False

        header = data[:8]

        h = struct.unpack('4H', header)
        if h[0] != phy_data_zigbee.MAGIC:
            #log('invalid magic %s' % h[0])
            return False

        return True

    def check_length(data):
        length = len(data)
        if length < 8:
            return False

        header = data[:8]
        h = struct.unpack('4H', header)
        if length != h[1]:
            log('bad len %d/%d' % (h[1], length))
            return False
   
        return True
 
    def make_header(self, addr, port, data):
        length = 8 + len(data)
        _data = struct.pack('4H', phy_data_zigbee.MAGIC, length, addr, port)
        return _data + data

    def parse_header(self, data):
        length = len(data)
        if length < 8:
            return (None, None, None)

        header = data[:8]

        h = struct.unpack('4H', header)
        if h[0] != phy_data_zigbee.MAGIC:
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

        self.test = True

        self.uart_interface = '/dev/ttyS1'
        self.uart_baudrate = 115200

        self._exit = False
        self.un_path = '/tmp/hsb/un_phy_zigbee.listen'
        self.outq = queue.Queue()

        t = threading.Thread(target=self.work_proc)
        t.start()
        self.work_thread = t

        self.clear()

    def clear(self):
        self.buf = ''.encode()

    def write(self, data):
        _data = phy_data_zigbee(data)
        #log('write uart: %s' % _data.raw_data)
        self.outq.put(_data)
        un_sends(self.un_path, 'notify')

    def on_data(self, data):
        buf = self.buf + data

        if not phy_data_zigbee.check_magic(buf):
            self.clear()
            return

        if not phy_data_zigbee.check_length(buf):
            self.buf = buf
            return

        phy_data = phy_data_zigbee(buf)

        #log(buf)

        if not phy_data.valid:
            self.clear()
            return

        self.on_data_ind(phy_data)
        self.clear()

    def exit(self):
        self._exit = True
        un_sends(self.un_path, 'notify')
        self.work_thread.join()

    def work_proc(self):
        un_sock = un_new_listen(self.un_path)
        if not un_sock:
            log('un_sock fail')
            return

        if not self.test:
            uart = serial.Serial(self.uart_interface, self.uart_baudrate)
            if not uart:
                log('open uart %s fail' % self.uart_interface)
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
                        n = s.inWaiting()
                        data = s.read(n)
                    else:
                        data, addr = s.recvfrom(1024)

                    # log(data)
                    self.on_data(data)

            for s in writable:
                if s is uart:
                    try:
                        data = self.outq.get_nowait()
                    except queue.Empty:
                        outputs.remove(s)
                    else:
                        if isinstance(s, serial.Serial):
                            s.write(data.raw_data)
                        else:
                            path = '/tmp/hsb/un_zigbee_sim-%d:%d.listen' % (data.addr, data.port)
                            s.sendto(data.raw_data, path)


