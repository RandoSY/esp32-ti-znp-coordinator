#!/usr/bin/env python3
"""Offline LQI regressions only. Never opens a serial port or starts a service."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler',default=os.environ.get('CXX','g++'))
    p.add_argument('--output',type=Path,default=ROOT/'artifacts/lqi-tests')
    args=p.parse_args()
    compiler=shutil.which(args.compiler)
    if not compiler:p.error('Compiler not found; supply --compiler')
    args.output.mkdir(parents=True,exist_ok=True)
    out=ROOT/'build/lqi-native';out.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env['PATH']=str(Path(compiler).parent)+os.pathsep+env.get('PATH','')
    src=ROOT/'Arduino/TI_ZNP_ESP32_RealBridge';suffix='.exe' if os.name=='nt' else ''
    target=out/('test_lqi'+suffix)
    common=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-I',str(src)]
    if os.name!='nt':common+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run([*common,*map(str,[src/'ZnpFrame.cpp',src/'ZnpBridge.cpp',src/'ZnpNv.cpp',ROOT/'tests/test_lqi.cpp']),'-o',str(target)],check=True,env=env)
    frames=args.output/'lqi-standard-frames.jsonl'
    subprocess.run([str(target),str(frames)],check=True,env=env)
    subprocess.run(['node',str(ROOT/'tests/lqi_host.cjs'),str(frames)],check=True,env=env,cwd=ROOT)
    diagnostic=out/('test_lqi_diagnostics'+suffix)
    subprocess.run([*common,'-DZNP_LQI_DIAGNOSTICS=1',*map(str,[src/'ZnpFrame.cpp',src/'ZnpBridge.cpp',src/'ZnpNv.cpp',src/'LqiDiagnostics.cpp',ROOT/'tests/test_lqi_diagnostics.cpp']),'-o',str(diagnostic)],check=True,env=env)
    subprocess.run([str(diagnostic)],check=True,env=env)
    return 0

if __name__=='__main__':raise SystemExit(main())
