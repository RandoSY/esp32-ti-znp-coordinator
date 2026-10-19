#!/usr/bin/env python3
"""ZNP hardware lab: status, saved-network resume, joining, traffic and TX.
Zigbee2MQTT must release the serial port before running this utility.
"""
from __future__ import annotations
import argparse
from collections import deque
import json
import sys
import time
from znp_vm_probe import FrameParser, build_frame, interpret

ERROR_STAGES={1:'radio platform configuration',2:'endpoint allocation',3:'cluster registration',4:'endpoint registration',5:'device registration',6:'stack startup',7:'Zigbee initialization/formation signal'}

class Connection:
    def __init__(self, port, journal=None):
        self.port=port
        self.parser=FrameParser()
        self.events=deque(maxlen=256)
        self.journal=journal

    def record(self,direction,raw):
        if self.journal:
            self.journal.write(json.dumps({'time':time.time(),'dir':direction,'hex':raw.hex(' ').upper()})+'\n')
            self.journal.flush()

    def send(self,cmd0,cmd1,payload=b''):
        raw=build_frame(cmd0,cmd1,payload)
        self.record('TX',raw)
        self.port.write(raw)
        self.port.flush()

    def read(self):
        frames=self.parser.feed(self.port.read(512))
        for frame in frames:
            self.record('RX',frame.raw)
        return frames

    def request(self,cmd0,cmd1,payload=b'',timeout=3):
        self.send(cmd0,cmd1,payload)
        deadline=time.monotonic()+timeout
        matched=None
        while time.monotonic()<deadline:
            for frame in self.read():
                if (frame.cmd0,frame.cmd1)==(cmd0|0x40,cmd1) and matched is None:
                    matched=frame
                elif (frame.cmd0,frame.cmd1)==(0x60,0) and len(frame.payload)==3 and frame.payload[1:]==bytes([cmd0,cmd1]):
                    raise RuntimeError(f'Firmware rejected command {cmd0:02X}{cmd1:02X}: RPC error {frame.payload[0]}')
                else:
                    self.events.append(frame)
            if matched is not None:
                return matched.payload
        raise RuntimeError(f'Timeout waiting for {cmd0|0x40:02X}{cmd1:02X}')

    def command(self,cmd0,cmd1,payload=b''):
        response=self.request(cmd0,cmd1,payload)
        if len(response)!=1 or response[0]!=0:
            raise RuntimeError(f'Command {cmd0:02X}{cmd1:02X} failed: {response.hex(" ").upper()}')

    def watch(self,seconds):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            while self.events:
                print_event(self.events.popleft())
            for frame in self.read():
                print_event(frame)

def print_event(frame):
    p=frame.payload
    if frame.identity=='45C0' and len(p)==1:
        print(f'STATE: {p[0]} (9 = coordinator running)')
    elif frame.identity=='45C1' and len(p)==13:
        print(f'DEVICE ANNOUNCEMENT: NWK=0x{int.from_bytes(p[2:4],"little"):04X} IEEE=0x{p[4:12][::-1].hex().upper()} capabilities=0x{p[12]:02X}')
    elif frame.identity=='4481' and len(p)>=17:
        print(f'RX: NWK=0x{int.from_bytes(p[4:6],"little"):04X} EP={p[6]}->{p[7]} cluster=0x{int.from_bytes(p[2:4],"little"):04X} LQI={p[9]} data={p[17:17+p[16]].hex(" ").upper()}')
    elif frame.identity=='4480' and len(p)==3:
        print(f'TX CONFIRM: status=0x{p[0]:02X} source EP={p[1]} transaction={p[2]}')
    else:
        print(f'EVENT {frame.identity}: {p.hex(" ").upper()}')

def status(connection):
    version=interpret('SYS_VERSION',connection.request(0x21,2))
    device=interpret('UTIL_GET_DEVICE_INFO',connection.request(0x27,0))
    network=connection.request(0x25,0x50)
    diagnostic=connection.request(0x27,0xF0)
    if len(network)!=24 or len(diagnostic)!=4:
        raise RuntimeError('Unexpected status response length')
    code=int.from_bytes(diagnostic,'little')
    print('Version:',json.dumps(version,sort_keys=True))
    print('Device:',json.dumps(device,sort_keys=True))
    print(f'Network: PAN=0x{int.from_bytes(network[3:5],"little"):04X} channel={network[23]} extended PAN=0x{network[7:15][::-1].hex().upper()}')
    print(f'Radio diagnostic: 0x{code:08X}')
    if code:
        stage=code>>24
        print(f'ERROR: {ERROR_STAGES.get(stage,"unknown phase")}; endpoint/signal={(code>>16)&255}; SDK code=0x{code&65535:04X}')
        return False
    print('RGB: green = ready/idle; white flash = received Zigbee data; cyan = joining; blue = waiting for host startup.')
    return True

def number(value):
    return int(value,0)

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True,help='COM17 or /dev/serial/by-id/...')
    parser.add_argument('--jsonl',help='Write raw TX/RX transcript (may contain device payloads)')
    sub=parser.add_subparsers(dest='action',required=True)
    sub.add_parser('status',help='Read identity, radio state, PAN/channel and diagnostic code')
    resume=sub.add_parser('resume',help='Start the previously configured lab network; preserves keys')
    resume.add_argument('--timeout',type=float,default=60)
    join=sub.add_parser('join',help='Open or close local coordinator joining')
    join.add_argument('--seconds',type=int,default=60)
    join.add_argument('--watch',type=float,default=60)
    watch=sub.add_parser('watch',help='Passively display device announcements, messages and confirmations')
    watch.add_argument('--seconds',type=float,default=60)
    tx=sub.add_parser('tx',help='Send an explicit AF application payload to a device')
    tx.add_argument('--dst',type=number,required=True)
    tx.add_argument('--dst-ep',type=int,default=1)
    tx.add_argument('--src-ep',type=int,default=1)
    tx.add_argument('--cluster',type=number,required=True)
    tx.add_argument('--data',required=True,help='Hex bytes including the complete ZCL header')
    tx.add_argument('--transaction',type=int,default=1)
    tx.add_argument('--watch',type=float,default=5)
    args=parser.parse_args(argv)
    try:
        import serial
    except ImportError:
        print('Install pyserial: python -m pip install pyserial',file=sys.stderr)
        return 2
    journal=None
    try:
        if args.jsonl:journal=open(args.jsonl,'a',encoding='utf-8')
        port=serial.Serial(port=None,baudrate=115200,timeout=.1,rtscts=False)
        port.dtr=False;port.rts=False;port.port=args.port;port.open()
        with port:
            c=Connection(port,journal)
            if args.action=='status':return 0 if status(c) else 1
            if args.action=='resume':
                c.command(0x25,0x40,b'\x64\x00')
                deadline=time.monotonic()+args.timeout
                while time.monotonic()<deadline:
                    info=c.request(0x27,0)
                    if len(info)>=14 and info[12]==9:
                        print('PASS: saved coordinator network is running')
                        return 0 if status(c) else 1
                    code=int.from_bytes(c.request(0x27,0xF0),'little')
                    if code:raise RuntimeError(f'Radio startup failed: 0x{code:08X}; run status for detail')
                    time.sleep(.25)
                raise RuntimeError('Network startup timed out')
            if args.action=='join':
                if not 0<=args.seconds<=255:raise ValueError('Joining seconds must be 0..255')
                c.command(0x25,0x36,bytes([2,0,0,args.seconds,1]))
                print(f'PASS: joining {"closed" if args.seconds==0 else "opened"}; RGB {"green" if args.seconds==0 else "cyan"}')
                c.watch(args.watch)
            elif args.action=='watch':c.watch(args.seconds)
            elif args.action=='tx':
                data=bytes.fromhex(args.data)
                if len(data)>220:raise ValueError('Payload exceeds 220 bytes')
                payload=args.dst.to_bytes(2,'little')+bytes([args.dst_ep,args.src_ep])+args.cluster.to_bytes(2,'little')+bytes([args.transaction,0x10,30,len(data)])+data
                c.command(0x24,1,payload)
                print('AF request accepted. A TX CONFIRM status 0 means successful stack delivery.')
                c.watch(args.watch)
        return 0
    except (OSError,ValueError,RuntimeError,serial.SerialException) as exc:
        print(f'FAIL: {exc}',file=sys.stderr)
        return 1
    finally:
        if journal:journal.close()

if __name__=='__main__':
    try:raise SystemExit(main())
    except KeyboardInterrupt:print('\nStopped.');raise SystemExit(130)
