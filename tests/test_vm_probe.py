"""Offline ZNP VM-probe tests (no pyserial or USB device required)."""
import unittest
from pathlib import Path
import importlib.util
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("znp_vm_probe",ROOT / "tools" / "znp_vm_probe.py")
module = importlib.util.module_from_spec(spec)
import sys
sys.modules["znp_vm_probe"] = module
spec.loader.exec_module(module)
class FrameTests(unittest.TestCase):
    def test_ping(self):
        p=module.FrameParser()
        r=p.feed(bytes.fromhex("FE 02 61 01 11 00 73"))
        self.assertEqual(len(r),1)
        self.assertEqual(r[0].identity,"6101")
        self.assertEqual(module.interpret("SYS_PING",r[0].payload)["capabilities_mask"],"0x0011")
    def test_incremental(self):
        b=module.build_frame(0x21,1)
        p=module.FrameParser()
        self.assertEqual(p.feed(b[:2]),[])
        self.assertEqual(p.feed(b[2:4]),[])
        self.assertEqual(len(p.feed(b[4:])),1)
    def test_bad_checksum_resync(self):
        p=module.FrameParser()
        good=module.build_frame(0x21,2)
        bad=good[:-1]+bytes([good[-1]^1])
        self.assertEqual(len(p.feed(b'\x34\x35'+bad+good)),1)
        self.assertEqual(p.bad_fcs,1)
        self.assertGreaterEqual(p.skipped_bytes,3)
    def test_two_frames(self):
        p=module.FrameParser()
        a=module.build_frame(0x21,1)
        b=module.build_frame(0x21,2)
        self.assertEqual([f.identity for f in p.feed(a+b)],["2101","2102"])
    def test_version(self):
        x=module.interpret("SYS_VERSION", bytes([2,0xEE,0,2,0,2,0,0,0]))
        self.assertEqual(x["product"],0xEE)
        self.assertEqual(x["revision"],2)
if __name__=='__main__': unittest.main(verbosity=2)
