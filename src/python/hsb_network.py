#!/usr/bin/env python3

import select, socket, sys, queue, os, threading, struct
from hsb_debug import log
from hsb_manager import hsb_manager
from hsb_cmd import hsb_cmd, hsb_reply, hsb_event
from unix_socket import un_new_listen, un_send

class hsb_client:
    def __init__(self, sock, address):
        self.sock = sock
        self.address = address
        self.valid = True
        self.outq = queue.Queue()

    def __str__(self):
        return str(self.address)

class hsb_network(threading.Thread):
    magic = 0x55AA
    def __init__(self, manager, udp_port=18000, tcp_port=18002):
        threading.Thread.__init__(self)
        self.manager = manager
        manager.set_network(self)

        self.udp_port = udp_port
        self.tcp_port = tcp_port
        self.inq = queue.Queue()
        self.outq = queue.Queue()
        self.un_path = '/tmp/hsb/un_network.listen'

        t = threading.Thread(target=self.cmd_proc)
        t.start()
        self.cmd_thread = t

    def deal_cmd(self, cmd):
        self.inq.put(cmd)

    def parse_cmd(self, cmd):
        self.manager.dispatch(cmd)

    def cmd_proc(self):
        cmdq = self.inq

        while True:
            cmd = cmdq.get()

            if not cmd:
                break

            self.parse_cmd(cmd)

    def exit(self):
        self.inq.put(None)

        t = self.cmd_thread
        t.join()

        self.on_reply(None)
        self.join()

    def on_reply(self, rdata):
        self.outq.put(rdata)

        un_send(self.un_path, 'notify'.encode())

    def on_event(self, rdata):
        self.outq.put(rdata)

        un_send(self.un_path, 'notify'.encode())

    def run(self):
        tcp_serv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_serv.setblocking(False)
        tcp_serv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        tcp_serv_addr = ('', self.tcp_port)

        try:
            tcp_serv.bind(tcp_serv_addr)
            tcp_serv.listen(5)
        except Exception as e:
            log(e)
            sys.exit(0)

        udp_serv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_serv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        udp_serv.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        udp_serv_addr = ('', self.udp_port)

        try:
            udp_serv.bind(udp_serv_addr)
        except Exception as e:
            log(e)
            sys.exit(0)

        un_sock = un_new_listen(self.un_path)
        if not un_sock:
            sys.exit(0)

        inputs = [ tcp_serv, udp_serv, un_sock ]
        outputs = []
        clients = {}

        while inputs:
            readable, writable, exceptional = select.select(inputs, outputs, inputs)

            for s in readable:
                if s is tcp_serv: # new connection
                    cli, cliaddr = s.accept()
                    cli.setblocking(False)
                    inputs.append(cli)

                    clients[cli] = hsb_client(cli, cliaddr)
                    log('client %s:%d connected' % cli.getpeername())
                elif s is udp_serv:
                    data, addr = s.recvfrom(1024)
                    data = data.decode()
                    if data == 'are you hsb?':
                        log('get udp cmd from %s: %s' % (addr, data))
                        reply = 'i am hsb'
                        s.sendto(reply.encode(), addr)
                    else:
                        log('unknown udp cmd from %s: %s' % (addr, data))
                elif s is un_sock:
                    data, addr = s.recvfrom(1024)

                    while not self.outq.empty():
                        rdata = self.outq.get_nowait()
                        if not rdata: # exit
                            return

                        if isinstance(rdata, hsb_reply):
                            cli = rdata.client
                            if not cli.valid:
                                continue
    
                            cli.outq.put(rdata)
                            if not cli.sock in outputs:
                                outputs.append(cli.sock)
                        elif isinstance(rdata, hsb_event):
                            clis = list(clients.values())
                            for cli in clis:
                                if not cli.valid:
                                    continue

                                cli.outq.put(rdata)
                                if not cli.sock in outputs:
                                    outputs.append(cli.sock)

                else: # client get data
                    try:
                        data = s.recv(1024)
                    except Exception:
                        data = None

                    cli = clients[s]
                    if data:
                        while len(data) > 4:
                            hdr, length = struct.unpack('=2H', data[:4])
                            if hdr != hsb_network.magic:
                                log('bad magic message')
                                break

                            if length <= 4 or length > len(data):
                                log('bad length %d' % length)
                                break

                            cmd = data[4:length].decode()
                            log('received %s from %s' % (cmd, s.getpeername()))
                            command = hsb_cmd(cli, cmd)
                            if command.valid:
                                self.deal_cmd(command)

                            data = data[length:]
                    else:
                        if s in outputs:
                            outputs.remove(s)

                        inputs.remove(s)
                        log('client %s:%d disconnected' % cli.address)
                        s.close()
                        del clients[s]
                        cli.valid = False

            for s in writable:
                cli = clients[s]
                try:
                    reply = cli.outq.get_nowait()
                except queue.Empty:
                    outputs.remove(s)
                else:
                    cmd = reply.get()
                    raw = cmd.encode()
                    data = struct.pack('=2H', hsb_network.magic, len(raw) + 4) + raw
                    
                    s.send(data)
                    log('send %s to client %s' % (cmd, s.getpeername()))

            for s in exceptional:
                cli = clients[s]
                inputs.remove(s)
                if s in outputs:
                    outputs.remove(s)

                log('client %s exception' % s.getpeername())
                s.close()
                del clients[s]
                cli.valid = False

if __name__ == '__main__':
    network = hsb_network()
    network.start()

    from time import sleep
    while True:
        try:
            sleep(10)
        except KeyboardInterrupt:
            network.exit()
            break
        except Exception as e:
            log(e)

