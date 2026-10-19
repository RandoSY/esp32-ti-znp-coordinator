#!/usr/bin/env python3
"""Reproducibly compile the complete ESP32 ZNP firmware; does not flash."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime,timezone

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--chip',choices=['esp32c6','esp32h2'],required=True)
    p.add_argument('--transport',choices=['native','uart'],default='native')
    p.add_argument('--flash-mb',type=int,choices=[2,4,8],default=4)
    p.add_argument('--jobs',type=int,default=8)
    args=p.parse_args()
    cli=shutil.which('arduino-cli')
    if not cli:p.error('arduino-cli is not on PATH')
    cores=subprocess.check_output([cli,'core','list','--format','json'],text=True)
    # Record exact installed core metadata alongside the resulting images.
    partition={2:'zigbee_zczr_2MB',4:'zigbee_zczr',8:'zigbee_zczr_8MB'}[args.flash_mb]
    fqbn=f'esp32:esp32:{args.chip}:CDCOnBoot={"cdc" if args.transport=="native" else "default"},FlashSize={args.flash_mb}M,PartitionScheme={partition},ZigbeeMode=zczr,DebugLevel=none'
    out=ROOT/'artifacts'/args.chip
    command=[cli,'compile','--jobs',str(args.jobs),'--fqbn',fqbn,'--build-path',str(ROOT/'build'/args.chip),'--output-dir',str(out),str(ROOT/'Arduino/TI_ZNP_ESP32_RealBridge')]
    result=subprocess.run(command,cwd=ROOT)
    if result.returncode:return result.returncode
    metadata={'utc':datetime.now(timezone.utc).isoformat(),'fqbn':fqbn,'arduino_cores':json.loads(cores),'sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.glob('*.bin')}}
    (out/'build.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(f'Build and hashes recorded in {out / "build.json"}')
    return 0

if __name__=='__main__':raise SystemExit(main())
