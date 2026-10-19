"""Regression checks for interleaved ZNP lab transactions; no serial hardware."""
import contextlib
import io
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from znp_lab import Connection
from znp_vm_probe import build_frame

class Port:
    def __init__(self,chunks):self.chunks=list(chunks);self.sent=[]
    def write(self,data):self.sent.append(data)
    def flush(self):pass
    def read(self,count):return self.chunks.pop(0) if self.chunks else b''

class LabTransactions(unittest.TestCase):
    def test_interleaved_events_are_not_lost_after_reply(self):
        state=build_frame(0x45,0xC0,b'\x09')
        confirm=build_frame(0x44,0x80,b'\x00\x01\x04')
        p=Port([state+build_frame(0x65,0x36,b'\x00')+confirm])
        c=Connection(p)
        c.command(0x25,0x36,bytes([2,0,0,60,1]))
        self.assertEqual([f.identity for f in c.events],['45C0','4480'])
    def test_fragmented_response(self):
        f=build_frame(0x61,1,b'\x59\x80');p=Port([f[:2],f[2:5],f[5:]])
        self.assertEqual(Connection(p).request(0x21,1),b'\x59\x80')
    def test_failure_is_not_reported_as_success(self):
        c=Connection(Port([build_frame(0x65,0x36,b'\x83')]))
        with self.assertRaisesRegex(RuntimeError,'83'):c.command(0x25,0x36,bytes([2,0,0,60,1]))
    def test_rpc_error_is_actionable(self):
        c=Connection(Port([build_frame(0x60,0,bytes([2,0x25,0x40]))]))
        with self.assertRaisesRegex(RuntimeError,'RPC error 2'):c.command(0x25,0x40,b'\x64\x00')
    def test_journal_includes_all_received_frames(self):
        reply=build_frame(0x61,1,b'\x59\x80');event=build_frame(0x45,0xC0,b'\x09')
        journal=io.StringIO();c=Connection(Port([reply+event]),journal)
        c.request(0x21,1)
        self.assertEqual(len(journal.getvalue().splitlines()),3)

if __name__=='__main__':unittest.main()
