"""Run unmodified herdsman's ZStackAdapter against production C++ ZNP code.
The native peer has a simulated radio; this verifies host protocol, not RF.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--peer',default=str(ROOT/'build/native/znp_native_peer.exe' if os.name=='nt' else ROOT/'build/native/znp_native_peer'))
    args=ap.parse_args()
    logs=ROOT/'artifacts/host-compatibility'
    logs.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='znp-host-') as work, socket.socket() as server, (logs/'startup.jsonl').open('w') as log:
        server.bind(('127.0.0.1',0));server.listen();server.settimeout(.2)
        stopping=threading.Event()
        lock=threading.Lock()
        peers=[]
        def relay(connection):
            peer=subprocess.Popen([args.peer,str(Path(work)/'nv')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            peers.append(peer)
            def record(direction,chunk):
                with lock:
                    log.write(json.dumps({'time':time.time(),'dir':direction,'hex':chunk.hex(' ').upper()})+'\n');log.flush()
            def receive():
                try:
                    while chunk:=os.read(peer.stdout.fileno(),4096):
                        record('RX',chunk);connection.sendall(chunk)
                except OSError:pass
            reader=threading.Thread(target=receive,daemon=True);reader.start()
            try:
                while chunk:=connection.recv(4096):
                    record('TX',chunk);peer.stdin.write(chunk);peer.stdin.flush()
            except (OSError,BrokenPipeError):pass
            finally:
                peer.stdin.close();connection.close()
                try:peer.wait(timeout=3)
                except subprocess.TimeoutExpired:peer.kill()
                reader.join(timeout=2)
        def accept():
            while not stopping.is_set():
                try:connection,_=server.accept()
                except socket.timeout:continue
                except OSError:break
                threading.Thread(target=relay,args=(connection,),daemon=True).start()
        thread=threading.Thread(target=accept,daemon=True);thread.start()
        try:
            result=subprocess.run(['node',str(ROOT/'tests/host_startup.cjs'),str(server.getsockname()[1]),str(Path(work)/'coordinator-backup.json')],capture_output=True,text=True,timeout=100)
            (logs/'result.txt').write_text(result.stdout+result.stderr,encoding='utf-8')
            print(result.stdout+result.stderr,end='')
            return result.returncode
        finally:
            stopping.set();thread.join(timeout=2)
            for peer in peers:
                if peer.poll() is None:peer.kill()

if __name__=='__main__':raise SystemExit(main())
