#!/usr/bin/env python3
"""Operator-controlled MQTT observation, or offline statistics; never uses serial.
Records only receipt time, contact state and LQI, rather than complete payloads.
"""
import argparse
from collections import Counter
from datetime import datetime,timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

def observation(payload,now=None):
    d=json.loads(payload)
    if not isinstance(d,dict):raise ValueError('MQTT body must be an object')
    lqi=d.get('linkquality');contact=d.get('contact')
    valid=type(lqi)is int and 0<=lqi<=255
    return {'received_utc':now or datetime.now(timezone.utc).isoformat(),'contact':contact if type(contact)is bool else None,'mqtt_lqi':lqi if valid else None,'mqtt_lqi_is_u8':valid,'rssi':None}

def summarize(rows):
    lqis=[r['mqtt_lqi'] for r in rows if r.get('mqtt_lqi_is_u8')]
    contacts=[r['contact'] for r in rows if type(r.get('contact'))is bool]
    changes=sum(a!=b for a,b in zip(contacts,contacts[1:]))
    return {'mqtt_publications':len(rows),'lqi_observations':len(lqis),'zero_lqi_publications':lqis.count(0),'zero_fraction':lqis.count(0)/len(lqis) if lqis else None,'minimum':min(lqis) if lqis else None,'maximum':max(lqis) if lqis else None,'lqi_histogram':dict(sorted(Counter(lqis).items())),'observed_contact_transitions_after_first_state':changes,'same_state_publications':len(contacts)-1-changes if contacts else 0,'physical_event_count':None,'delivery_failures':None,'delays_or_duplicates':None,'note':'Publication counts are not RF packet counts; repeated states may be battery updates/cache. Compare against operator action log and raw diagnostic records.'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--analyze',type=Path,help='Offline analysis only; no network connection')
    p.add_argument('--output',type=Path)
    p.add_argument('--seconds',type=int,default=120)
    p.add_argument('--topic',default='esp32_znp_lab/UZ-6D_Door')
    p.add_argument('--host',default='127.0.0.1')
    p.add_argument('--port',type=int,default=1883)
    p.add_argument('--operator-approved-capture',action='store_true')
    a=p.parse_args()
    if a.analyze:
        rows=[json.loads(line) for line in a.analyze.read_text().splitlines() if line.strip()]
        print(json.dumps(summarize(rows),indent=2));return 0
    if not a.operator_approved_capture:p.error('Live capture requires operator approval; --analyze is offline')
    if not a.output or a.output.exists():p.error('Select a new --output file')
    if not 1<=a.seconds<=3600:p.error('--seconds must be 1..3600')
    executable=shutil.which('mosquitto_sub')
    if not executable and os.name=='nt':executable=str(Path(os.environ['LOCALAPPDATA'])/'ESP32-ZNP-Lab/mosquitto/mosquitto_sub.exe')
    if not executable or not Path(executable).is_file():p.error('Installed mosquitto_sub client not found')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    rows=[];invalid=0
    # -R ignores retained publications. A new publish may still contain cached
    # contact attributes; receiver time is not an RF reception timestamp.
    cmd=[executable,'-h',a.host,'-p',str(a.port),'-t',a.topic,'-R','-W',str(a.seconds)]
    started=time.monotonic()
    with a.output.open('x',encoding='utf-8') as out:
        proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,text=True)
        try:
            for line in proc.stdout:
                try:r=observation(line)
                except (ValueError,TypeError):invalid+=1;continue
                rows.append(r);out.write(json.dumps(r)+'\n');out.flush()
            proc.wait()
        except KeyboardInterrupt:
            proc.terminate();proc.wait(timeout=5)
        finally:
            if proc.poll()is None:proc.terminate();proc.wait(timeout=5)
    elapsed=time.monotonic()-started
    result=summarize(rows);result['invalid_publications']=invalid;result['subscriber_exit']=proc.returncode;result['elapsed_seconds']=elapsed
    result['capture_window_elapsed']=elapsed>=a.seconds-.5
    # Client versions use different timeout exit codes. Retain the raw code;
    # an early nonzero exit is a capture failure, not a radio-loss measurement.
    print(json.dumps(result,indent=2));return 0 if proc.returncode==0 or result['capture_window_elapsed'] else 1

if __name__=='__main__':raise SystemExit(main())
