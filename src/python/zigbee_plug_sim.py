#!/usr/bin/env python3

import struct
from unix_socket import un_send, un_new_listen

magic = 0x55AA
length = 27
addr = 100
port = 8000

data = struct.pack('6Hi8sHB', magic, length, addr, port, 0x9102, 19, 1, b'\x01\x02\x03\x04\x05\x06\x07\x08', 0xC100, 0)

un_send('/tmp/hsb/un_zigbee_test.listen', data)

