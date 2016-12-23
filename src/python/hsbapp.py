#!/usr/bin/env python3

from hsb_debug import log, log_init
from hsb_network import hsb_network
from hsb_manager import hsb_manager
from hsb_audio import hsb_audio

from drv_orange import drv_orange
from phy_zigbee import phy_zigbee

from time import sleep

if __name__ == '__main__':
    log_init()

    manager = hsb_manager()

    zigbee = phy_zigbee(manager)
    manager.add_phy(zigbee)

    orange = drv_orange(manager)
    manager.add_driver(orange)

    audio = hsb_audio(manager)
    manager.set_audio(audio)

    network = hsb_network(manager)
    network.start()

    manager.start()

    def debug_prompt():
        while True:
            cmd = input('> ')
            if len(cmd) == 0:
                continue

            print(cmd)

    try:
        debug_prompt()
    except KeyboardInterrupt:
        # log('keyboard interrupt')
        pass
    except Exception as e:
        print(e)
    finally:
        network.exit()
        manager.exit()



