#!/usr/bin/env python3
"""Compile/run native protocol tests. --host also drives unmodified herdsman."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler',default=os.environ.get('CXX','g++'))
    p.add_argument('--host',action='store_true')
    args=p.parse_args()
    compiler=shutil.which(args.compiler)
    if not compiler and Path(args.compiler).is_file():compiler=str(Path(args.compiler).resolve())
    if not compiler:p.error('C++ compiler not found; install g++ or set --compiler/CXX')
    env=os.environ.copy();env['PATH']=str(Path(compiler).parent)+os.pathsep+env.get('PATH','')
    src=ROOT/'Arduino/TI_ZNP_ESP32_RealBridge';out=ROOT/'build/native';out.mkdir(parents=True,exist_ok=True)
    common=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-I',str(src),str(src/'ZnpFrame.cpp'),str(src/'ZnpBridge.cpp'),str(src/'ZnpNv.cpp')]
    suffix='.exe' if os.name=='nt' else ''
    for name in ['test_bridge','test_framing','test_nv','test_aps_tx','test_lqi','znp_native_peer']:
        target=out/(name+suffix)
        flags=['-static'] if os.name=='nt' and name=='znp_native_peer' else []
        if os.name!='nt' and name!='znp_native_peer':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run([*common,*flags,str(ROOT/'tests'/f'{name}.cpp'),'-o',str(target)],check=True,env=env)
        if name!='znp_native_peer':subprocess.run([str(target)],check=True,env=env)
    if args.host:subprocess.run([sys.executable,str(ROOT/'tests/host_startup.py'),'--peer',str(out/('znp_native_peer'+suffix))],check=True,env=env)
    return 0

if __name__=='__main__':raise SystemExit(main())
