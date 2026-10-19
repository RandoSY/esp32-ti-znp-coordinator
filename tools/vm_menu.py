#!/usr/bin/env python3
"""Interactive Windows / Ubuntu serial test menu for the ESP32 ZNP firmware."""
import znp_vm_probe as znp
import znp_lab as lab


def serial_ports():
    try:
        from serial.tools import list_ports
    except ImportError:
        print('pyserial not found. Install: python -m pip install pyserial')
        return []
    ports=list(list_ports.comports())
    if not ports:print('No serial devices detected; check USB cable or VM passthrough.')
    for i,device in enumerate(ports,1):
        print(f'  {i}. {device.device} -- {device.description}')
    return ports


def select_port():
    ports=serial_ports()
    if not ports:return None
    try:
        number=int(input('Port number [1]: ').strip() or '1')
        if not 1<=number<=len(ports):raise ValueError()
        return ports[number-1].device
    except ValueError:
        print('Invalid selection.')
        return None


def menu():
    while True:
        print('\n'+'='*62)
        print('  ESP32 TI ZNP lab: close Zigbee2MQTT before using this menu')
        print('='*62)
        print('  1. List serial devices')
        print('  2. Run four read-only ZNP checks and save transcript')
        print('  3. Show Zigbee2MQTT serial configuration')
        print('  4. Show troubleshooting and RGB meanings')
        print('  5. Read radio status / exact error code')
        print('  6. Resume the saved Zigbee network (preserves keys)')
        print('  7. Open joining for 60 seconds and watch devices')
        print('  8. Close joining')
        print('  9. Watch announcements and received traffic for 60 seconds')
        print('  0. Exit')
        choice=input('Choice [0-9]: ').strip()
        if choice=='0':return
        if choice=='1':serial_ports()
        elif choice=='2':
            port=select_port()
            if port:znp.main(['--port',port,'--jsonl','znp_test_transcript.jsonl'])
        elif choice=='3':
            print('''
serial:
  port: /dev/serial/by-id/REPLACE_WITH_YOUR_ESP32
  adapter: zstack
  baudrate: 115200
  rtscts: false

Use a dedicated test instance. First commissioning requires Zigbee2MQTT's
network configuration. Menu 6 resumes the configuration saved on the board.
Read docs/TESTING.md for the full Windows and Ubuntu procedure.
''')
        elif choice=='4':
            print('''
No port: connect ESP32 USB to the guest with VirtualBox Devices -> USB.
Permission denied: check 'dialout' membership and close other serial owners.
BLUE breathing: serial ready, waiting for network startup.
PURPLE blinking: initializing or forming the network.
GREEN steady: coordinator ready, including idle.
WHITE flash (120 ms): received Zigbee application data, including contact reports.
CYAN steady: joining open; returns to green when the window expires.
AMBER briefly: rejected request, checksum error, or queue overflow.
RED blinking: radio startup or persistent storage failure. Use menu 5.
The four read-only checks validate serial protocol, not RF pairing.
''')
        elif choice in {'5','6','7','8','9'}:
            port=select_port()
            if port:
                actions={'5':['status'],'6':['resume'],'7':['join','--seconds','60','--watch','60'],'8':['join','--seconds','0','--watch','0'],'9':['watch','--seconds','60']}
                lab.main(['--port',port,'--jsonl','znp_lab_transcript.jsonl',*actions[choice]])
        else:print('Please choose a number from 0 through 9.')


if __name__=='__main__':
    try:menu()
    except (KeyboardInterrupt,EOFError):print('\nExiting.')
