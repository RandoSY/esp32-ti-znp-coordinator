#!/usr/bin/env python3
"""Operator-approved read-only framing check; normal serial host must be stopped.

Uses an unsupported F1 query in a diagnostics-OFF build, immediately followed
by PING. Its valid FE checksum must not consume the next request's SOF.
No reset, startup, NV, permit-join or radio-transmit command is sent.
The intentional unsupported query may briefly show the protocol-warning LED.
"""
import argparse
from datetime import datetime,timezone
import json
from pathlib import Path
import time
from znp_lab import Connection
from znp_vm_probe import build_frame

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--operator-approved-port-access',action='store_true')
    a=p.parse_args()
    if not a.operator_approved_port_access:p.error('Operator approval and exclusive serial ownership are required')
    if a.output.exists():p.error('Select a new output file')
    import serial
    port=serial.Serial(port=None,baudrate=115200,timeout=.02,rtscts=False)
    port.dtr=False;port.rts=False;port.port=a.port
    port.open()
    with port:
        c=Connection(port)
        version=c.request(0x21,2)
        first=build_frame(0x27,0xF1,b'\x01\x2c\x00\x00\x00')
        assert first[-1]==0xFE
        # One write forces close spacing and prevents the decoder's 750 ms
        # timeout from hiding its old erroneous READ_LEN state.
        port.write(first+build_frame(0x21,1));port.flush()
        frames=[];deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            frames.extend(c.read())
            if any(f.cmd0==0x60 and f.cmd1==0 and f.payload==b'\x02\x27\xf1' for f in frames) and any(f.cmd0==0x61 and f.cmd1==1 and f.payload==b'\x59\x80' for f in frames):break
        rejected=any(f.cmd0==0x60 and f.cmd1==0 and f.payload==b'\x02\x27\xf1' for f in frames)
        ping=any(f.cmd0==0x61 and f.cmd1==1 and f.payload==b'\x59\x80' for f in frames)
        result={'utc':datetime.now(timezone.utc).isoformat(),'first_request':first.hex(),'first_request_valid_fcs':254,'diagnostics_off_rejection_received':rejected,'following_ping_received_without_retry':ping,'response_command_ids':[f.identity for f in frames],'version_response_length':len(version),'parser_bad_fcs':c.parser.bad_fcs,'result':'PASS' if rejected and ping else 'FAIL','scope':'Read-only F1 and PING; no reset/NV/network changes; no automatic retry.'}
        a.output.parent.mkdir(parents=True,exist_ok=True)
        with a.output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
        print(json.dumps(result,indent=2));return 0 if result['result']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())
