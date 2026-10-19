#!/usr/bin/env python3
"""Generate an isolated Zigbee2MQTT config matching the locally tested board."""
import argparse
import json
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--network',default='artifacts/esp32c6/lab-network.json')
    p.add_argument('--port',required=True)
    p.add_argument('--mqtt',default='mqtt://localhost')
    p.add_argument('--output',default='lab-zigbee2mqtt/configuration.yaml')
    args=p.parse_args()
    network=json.loads(Path(args.network).read_text())
    config={'mqtt':{'server':args.mqtt,'base_topic':'esp32_znp_lab'},'serial':{'port':args.port,'adapter':'zstack','baudrate':115200,'rtscts':False},'advanced':{'log_level':'debug','pan_id':network['panID'],'ext_pan_id':network['extendedPanID'],'channel':network['channelList'][0],'network_key':network['networkKey']}}
    out=Path(args.output)
    if out.exists():p.error(f'{out} already exists; select a new --output rather than overwriting it')
    out.parent.mkdir(parents=True,exist_ok=True)
    # JSON is valid YAML, so no external YAML package is needed.
    out.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
    print(f'Created {out}; contains the private lab network key. Keep it out of GitHub.')

if __name__=='__main__':main()
