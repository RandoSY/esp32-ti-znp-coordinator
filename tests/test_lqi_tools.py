import json
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from znp_lqi_snapshot import decode_record,read_only_request
from lqi_mqtt_capture import observation,summarize

class LqiTools(unittest.TestCase):
    def test_read_retry_is_identical_bounded_and_visible(self):
        class Link:
            def __init__(self):self.calls=[]
            def request(self,*args):
                self.calls.append(args)
                if len(self.calls)==1:raise RuntimeError('Timeout waiting for 67F1')
                return b'answer'
        c=Link();timeouts=[]
        self.assertEqual(read_only_request(c,b'\x00',1,timeouts,'summary'),b'answer')
        self.assertEqual(c.calls,[(0x27,0xF1,b'\x00')]*2)
        self.assertEqual(timeouts,[{'request':'summary','attempt':1}])
        c=Link()
        with self.assertRaisesRegex(RuntimeError,'summary: Timeout'):read_only_request(c,b'\x00',0,[],'summary')
        self.assertEqual(len(c.calls),1)
    def test_rejected_read_is_not_retried(self):
        class Link:
            calls=0
            def request(self,*args):self.calls+=1;raise RuntimeError('Firmware rejected command')
        c=Link()
        with self.assertRaisesRegex(RuntimeError,'rejected'):read_only_request(c,b'\x00',2,[],'summary')
        self.assertEqual(c.calls,1)
    def test_zero_is_preserved(self):
        r=observation('{"linkquality":0,"contact":false,"network_key":"DO_NOT_COPY"}','test')
        self.assertEqual(r['mqtt_lqi'],0);self.assertTrue(r['mqtt_lqi_is_u8']);self.assertNotIn('network_key',json.dumps(r))
    def test_invalid_does_not_become_zero(self):
        for value in [None,-1,256,True,'173',1.5]:
            r=observation(json.dumps({'linkquality':value}));self.assertIsNone(r['mqtt_lqi']);self.assertFalse(r['mqtt_lqi_is_u8'])
    def test_statistics_do_not_claim_rf_delivery(self):
        rows=[observation(json.dumps({'linkquality':lqi,'contact':contact})) for lqi,contact in [(173,True),(0,False),(0,False),(142,True)]]
        s=summarize(rows);self.assertEqual(s['zero_lqi_publications'],2);self.assertEqual(s['observed_contact_transitions_after_first_state'],2);self.assertEqual(s['same_state_publications'],1);self.assertIsNone(s['delivery_failures'])
    def test_raw_negative_and_unavailable_fields_are_explicit(self):
        p=b'\x00\x01'+struct.pack('<IIiHHHH8B',7,123,-1,0xBEEF,0x500,0x104,9,1,1,0,0,0,6,6,0)
        r=decode_record(p);self.assertEqual(r['sdk_lqi_raw'],-1);self.assertFalse(r['raw_lqi_in_u8_range']);self.assertEqual(r['znp_lqi_byte'],0);self.assertTrue(r['encoded']);self.assertIsNone(r['sdk_rssi_raw'])
    def test_unencoded_byte_is_not_claimed_as_measurement(self):
        p=b'\x00\x01'+struct.pack('<IIiHHHH8B',7,123,173,1,1,1,1,1,1,0,173,0,0,1,0)
        r=decode_record(p);self.assertIsNone(r['znp_lqi_byte']);self.assertIsNone(r['znp_sequence'])

if __name__=='__main__':unittest.main()
