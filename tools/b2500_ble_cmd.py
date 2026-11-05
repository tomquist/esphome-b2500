#!/usr/bin/env python3
"""Send one command to a B2500 over BLE, as the Marstek app does, and print
the replies.

Frames are 0x73, length, 0x23, command, payload, XOR of all bytes before.
The battery answers with notifications on ff02.

    # read the device info (command 0x03)
    python tools/b2500_ble_cmd.py --name HM_B2500_e18f --cmd 0x03 --payload-hex 01
    # give the device the cloud id the app assigned (command 0x50)
    python tools/b2500_ble_cmd.py --name HM_B2500_e18f --cmd 0x50 --payload c0M0Y0TV9Vk0M000lVvVo0a0

Needs bleak (pip install bleak).
"""

from __future__ import annotations

import argparse
import asyncio
import sys

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover
    sys.exit("needs bleak: pip install bleak")

UUID = "0000{:04x}-0000-1000-8000-00805f9b34fb"
FF01, FF02 = UUID.format(0xFF01), UUID.format(0xFF02)


def frame(cmd: int, payload: bytes) -> bytes:
    body = bytes([0x73, 4 + len(payload) + 1, 0x23, cmd]) + payload
    check = 0
    for b in body:
        check ^= b
    return body + bytes([check])


async def main(args) -> int:
    payload = bytes.fromhex(args.payload_hex) if args.payload_hex else args.payload.encode()
    data = frame(int(args.cmd, 0), payload)
    print(f"scanning for {args.name} ...")
    dev = await BleakScanner.find_device_by_name(args.name, timeout=args.scan)
    if dev is None:
        print("not found (is the phone app still connected? only one central at a time)")
        return 1
    async with BleakClient(dev) as client:
        print(f"connected to {dev.address}")

        def on_notify(_, d: bytearray):
            text = bytes(d[4:-1]).decode("latin1") if len(d) > 5 else ""
            print(f"<- {bytes(d).hex()}  {text!r}")

        await client.start_notify(FF02, on_notify)
        print(f"-> {data.hex()}")
        await client.write_gatt_char(FF01, data, response=True)
        await asyncio.sleep(args.wait)
    return 0


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--name", required=True, help="BLE name, e.g. HM_B2500_e18f")
    p.add_argument("--cmd", required=True, help="command byte, e.g. 0x50")
    g = p.add_mutually_exclusive_group()
    g.add_argument("--payload", default="", help="payload as text")
    g.add_argument("--payload-hex", help="payload as hex")
    p.add_argument("--scan", type=float, default=15.0, help="scan timeout in s")
    p.add_argument("--wait", type=float, default=5.0, help="how long to print replies, in s")
    sys.exit(asyncio.run(main(p.parse_args())))
