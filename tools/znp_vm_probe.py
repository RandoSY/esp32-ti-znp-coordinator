#!/usr/bin/env python3
"""TI ZNP serial smoke tester for the Ubuntu guest VM.

Goals:
  * Verify USB forwarding AND binary exchange independently of Zigbee2MQTT.
  * Use only non-mutating ZNP requests: PING, VERSION, GET_EXTADDR, DEVICE_INFO.
  * Record actual TX/RX bytes, latency, checksum, and command identity.
  * Do NOT impersonate a ZNP coordinator, modify NVRAM or reset a network.

ZNP MT frame: FE LEN CMD0 CMD1 PAYLOAD... FCS, where FCS is XOR of
LEN, CMD0, CMD1 and PAYLOAD. A command response may be interleaved with
unsolicited asynchronous events. We therefore search by command ID, not by
assuming the next bytes received must be the reply we wanted.
"""
from __future__ import annotations
import argparse
import json
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone

SOF = 0xFE


def build_frame(cmd0: int, cmd1: int, payload: bytes = b"") -> bytes:
    if len(payload) > 255:
        raise ValueError("ZNP payload may not exceed 255 bytes")
    fcs = len(payload) ^ cmd0 ^ cmd1
    for x in payload:
        fcs ^= x
    return bytes([SOF, len(payload), cmd0, cmd1]) + payload + bytes([fcs])


@dataclass(frozen=True)
class Frame:
    cmd0: int
    cmd1: int
    payload: bytes
    raw: bytes

    @property
    def identity(self) -> str:
        return f"{self.cmd0:02X}{self.cmd1:02X}"


class FrameParser:
    """Loss-tolerant incremental parser with checksum and resynchronization.

    The parser caps its retained buffer. On invalid lengths/checksums, search
    for the next SOF instead of treating invalid data as a complete frame.
    """
    def __init__(self) -> None:
        self.buf = bytearray()
        self.bad_fcs = 0
        self.skipped_bytes = 0
        self.frames = 0

    def feed(self, data: bytes) -> list[Frame]:
        self.buf.extend(data)
        result: list[Frame] = []
        while True:
            k = self.buf.find(bytes([SOF]))
            if k < 0:
                self.skipped_bytes += len(self.buf)
                self.buf.clear()
                break
            if k > 0:
                self.skipped_bytes += k
                del self.buf[:k]
            if len(self.buf) < 5:
                break
            count = self.buf[1] + 5
            if len(self.buf) < count:
                break
            raw = bytes(self.buf[:count])
            fcs = 0
            for x in raw[1:-1]:
                fcs ^= x
            if fcs != raw[-1]:
                self.bad_fcs += 1
                self.skipped_bytes += 1
                del self.buf[:1]
                continue
            result.append(Frame(raw[2], raw[3], raw[4:-1], raw))
            self.frames += 1
            del self.buf[:count]
        return result


COMMANDS = [
    ("SYS_PING", 0x21, 0x01),
    ("SYS_VERSION", 0x21, 0x02),
    ("SYS_GET_EXTADDR", 0x21, 0x04),
    ("UTIL_GET_DEVICE_INFO", 0x27, 0x00),
]


def interpret(name: str, payload: bytes) -> dict:
    if name == "SYS_PING" and len(payload) == 2:
        return {"capabilities_mask": f"0x{int.from_bytes(payload, 'little'):04X}"}
    if name == "SYS_VERSION" and len(payload) >= 5:
        out = {"transport_rev": payload[0], "product": payload[1],
               "major": payload[2], "minor": payload[3], "maintenance": payload[4]}
        if len(payload) >= 9:
            out["revision"] = int.from_bytes(payload[5:9], "little")
        return out
    if name == "SYS_GET_EXTADDR" and len(payload) == 8:
        return {"ieee_address": "0x" + payload[::-1].hex().upper()}
    if name == "UTIL_GET_DEVICE_INFO" and len(payload) >= 14:
        return {"status": payload[0], "ieee_address": "0x" + payload[1:9][::-1].hex().upper(),
                "short_addr": f"0x{int.from_bytes(payload[9:11], 'little'):04X}",
                "device_type_mask": payload[11], "device_state": payload[12],
                "associated_devices_reported": payload[13]}
    return {"payload_hex": payload.hex(" ").upper(), "note": "unparsed or unsupported reply shape"}


def run_probe(serial_port, timeout: float, jsonl: str | None) -> bool:
    parser = FrameParser()
    journal = open(jsonl, "a", encoding="utf-8") if jsonl else None

    def record(direction: str, raw: bytes, **extra) -> None:
        iso = datetime.now(timezone.utc).isoformat()
        info = {"utc": iso, "dir": direction, "hex": raw.hex(" ").upper(), **extra}
        print(f"{direction}: {info['hex']}")
        if journal:
            journal.write(json.dumps(info, sort_keys=True) + "\n")
            journal.flush()

    passed = 0
    try:
        for name, cmd0, cmd1 in COMMANDS:
            # Discard old partial bytes; an unsolicited event may still arrive.
            parser.buf.clear()
            request = build_frame(cmd0, cmd1)
            print("\n" + name)
            record("TX", request, command=name)
            serial_port.write(request)
            serial_port.flush()
            deadline = time.monotonic() + timeout
            expected = (cmd0 | 0x40, cmd1)  # SREQ 0x20 -> SRSP 0x60.
            matched = None
            started = time.monotonic()
            while time.monotonic() < deadline:
                chunk = serial_port.read(128)
                for f in parser.feed(chunk):
                    record("RX", f.raw, identity=f.identity)
                    if (f.cmd0, f.cmd1) == expected:
                        matched = f
                        break
                if matched:
                    break
            if matched:
                ms = round((time.monotonic() - started) * 1000, 1)
                parsed = interpret(name, matched.payload)
                print("PASS", name, f"({ms} ms)", json.dumps(parsed, sort_keys=True))
                if journal:
                    journal.write(json.dumps({"command": name, "result": "PASS", "ms": ms, "decoded": parsed}) + "\n")
                passed += 1
            else:
                print("FAIL", name, "no matching SRSP received")
                if journal:
                    journal.write(json.dumps({"command": name, "result": "FAIL", "reason": "timeout"}) + "\n")
        print(f"\nRESULT: {passed}/{len(COMMANDS)} read-only command checks passed",
              f"bad checksums={parser.bad_fcs}, noise bytes={parser.skipped_bytes}")
        return passed == len(COMMANDS)
    finally:
        if journal:
            journal.close()


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Read-only TI ZNP command probe for USB-attached ESP32 or TI dongle; no virtual COM ports.",
        epilog="Examples: python3 znp_vm_probe.py --list ; "
               "python3 znp_vm_probe.py --port /dev/ttyACM0 --jsonl znp_probe.jsonl")
    ap.add_argument("--list", action="store_true", help="List candidate serial devices and exit")
    ap.add_argument("--port", help="Guest Linux tty, e.g. /dev/ttyACM0 or /dev/serial/by-id/...")
    ap.add_argument("--baud", type=int, default=115200, help="Baud rate (default 115200)")
    ap.add_argument("--timeout", type=float, default=2.0, help="Per-command response timeout in seconds")
    ap.add_argument("--jsonl", help="Append TX/RX transcript and decoded results to JSONL file")
    args = ap.parse_args(argv)
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("Missing pyserial. Install: python3 -m pip install pyserial", file=sys.stderr)
        return 2
    if args.list:
        devices = list(list_ports.comports())
        for dev in devices:
            print(f"{dev.device:22s} {dev.description} VID:PID={dev.vid}:{dev.pid}")
        return 0
    if not args.port:
        ap.error("specify --port or --list; this tool never guesses a COM port")
    try:
        # No assert/automatic reset: no DTR or RTS toggle to provoke bootloader.
        ser = serial.Serial(port=None, baudrate=args.baud, timeout=0.1, rtscts=False)
        ser.dtr = False
        ser.rts = False
        ser.port = args.port
        ser.open()
        try:
            return 0 if run_probe(ser, args.timeout, args.jsonl) else 1
        finally:
            ser.close()
    except (OSError, serial.SerialException) as err:
        print(f"Unable to open or use {args.port}: {err}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
