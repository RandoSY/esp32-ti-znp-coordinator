#!/usr/bin/env python3
"""Read opt-in RAM LQI diagnostics. Close the serial host only with operator approval.
Does not start/resume/reset the radio, read NV, or change network configuration.
"""
import argparse
import json
from pathlib import Path
import struct
import sys
from znp_lab import Connection

def read_only_request(connection,payload,retries,timeouts,label):
    for attempt in range(retries+1):
        try:return connection.request(0x27,0xF1,payload)
        except RuntimeError as e:
            if not str(e).startswith('Timeout waiting for '):raise
            timeouts.append({'request':label,'attempt':attempt+1})
            if attempt==retries:raise RuntimeError(f'{label}: {e}') from e

def decode_record(p):
    if len(p)!=30 or p[:2]!=bytes([0,1]):raise ValueError('Unexpected LQI record/schema')
    fields=struct.unpack('<IIiHHHH8B',p[2:])
    names=['rx_id','rx_ms','sdk_lqi_raw','source','cluster','profile','payload_length','source_endpoint','destination_endpoint','sdk_status','bridge_lqi','znp_lqi_byte','znp_sequence','flags','reserved']
    r=dict(zip(names,fields));flags=r.pop('flags');r.pop('reserved')
    r.update({'raw_lqi_in_u8_range':bool(flags&1),'queued':bool(flags&2),'encoded':bool(flags&4),'sdk_rssi_raw':None,'sdk_lqi_validity':'not exposed by APS API','transport_status':'not observed'})
    if not r['encoded']:r['znp_lqi_byte']=None;r['znp_sequence']=None
    return r

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--read-retries',type=int,choices=range(3),default=0,help='Bounded retries of identical read-only F1 requests; timeouts remain recorded (default 0)')
    p.add_argument('--operator-approved-port-access',action='store_true',help='Acknowledge approval and that the normal host has released this port')
    a=p.parse_args()
    if not a.operator_approved_port_access:p.error('Explicit operator approval and exclusive port ownership are required')
    if a.output.exists():p.error('Output exists; select a new file')
    import serial
    port=serial.Serial(port=None,baudrate=115200,timeout=.02,rtscts=False)
    port.dtr=False;port.rts=False;port.port=a.port
    try:
        port.open()
        with port:
            c=Connection(port);timeouts=[];s=read_only_request(c,b'\x00',a.read_retries,timeouts,'summary')
            if len(s)!=16 or s[:2]!=b'\x00\x01':raise ValueError('Diagnostics disabled or incompatible summary')
            capacity,count=s[2:4];last,overwritten,drops=struct.unpack('<III',s[4:])
            records=[];missing=[]
            for rx_id in range(max(1,last-count+1),last+1):
                raw=read_only_request(c,b'\x01'+rx_id.to_bytes(4,'little'),a.read_retries,timeouts,f'record {rx_id}')
                if raw==b'\x09':missing.append(rx_id)
                else:records.append(decode_record(raw))
            result={'schema':1,'capacity':capacity,'last_rx_id':last,'overwritten_total':overwritten,'queue_drops_total':drops,'missing_record_ids':missing,'records':records,'request_timeouts':timeouts,'read_retries_allowed':a.read_retries,'scope':'Accepted APS application indications only; no RSSI, SDK validity flag or transport-write acknowledgement exists in this capture.'}
            a.output.parent.mkdir(parents=True,exist_ok=True)
            with a.output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
            print(f'Saved {len(records)} metadata records. Overwritten={overwritten}; missing={len(missing)}; queue drops={drops}; request timeouts={len(timeouts)}. No payloads/NV/key material read.')
        return 0
    except (OSError,ValueError,RuntimeError,serial.SerialException) as e:
        print(f'FAIL: {e}',file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
