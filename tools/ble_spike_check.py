#!/usr/bin/env python3
"""Check examples/fc41d/ble-spike.yaml over Bluetooth from this computer.

The spike firmware echoes every write on ff01 as a notification on ff02. This
script plays the phone: it connects, lists the service, subscribes to ff02,
writes test data to ff01 and checks that each write comes back unchanged and
how fast. It also writes to ff06 and reads ff02. With --duration it keeps
reconnecting for that long (a soak test while Wi-Fi is busy) and prints a
summary at the end.

    pip install bleak
    python3 tools/ble_spike_check.py
    python3 tools/ble_spike_check.py --duration 3600

On macOS the terminal app needs Bluetooth permission (asked on the first run).
Close nRF Connect or other apps that might hold the connection.
Output also goes to ble-spike-check-<timestamp>.log.
"""

import argparse
import asyncio
import os
import statistics
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover
    sys.exit("pip install bleak")

UUID = "0000{:04x}-0000-1000-8000-00805f9b34fb"
FF01, FF02, FF06 = UUID.format(0xFF01), UUID.format(0xFF02), UUID.format(0xFF06)


class Stats:
    def __init__(self):
        self.sessions = self.session_fail = 0
        self.ok = self.lost = self.mismatch = self.write_fail = self.extra = 0
        self.rtt = []


class Check:
    def __init__(self, args):
        self.args = args
        self.stats = Stats()
        self.t0 = time.monotonic()
        self.log = open(time.strftime("ble-spike-check-%Y%m%d-%H%M%S.log"), "w", encoding="utf-8")
        self.queue = asyncio.Queue()

    def say(self, text):
        line = f"{time.monotonic() - self.t0:8.2f} {text}"
        print(line)
        self.log.write(line + "\n")
        self.log.flush()

    def on_notify(self, _ch, data):
        self.queue.put_nowait((time.monotonic(), bytes(data)))

    async def echo(self, client, payload, response):
        while not self.queue.empty():
            self.queue.get_nowait()
        t = time.monotonic()
        try:
            await client.write_gatt_char(FF01, payload, response=response)
        except Exception as e:  # noqa: BLE001 - report whatever bleak raises
            self.stats.write_fail += 1
            self.say(f"  write {len(payload)} bytes failed: {e!r}")
            return
        # Wait for the echo; other notifications (a battery, or
        # fc41d_mcu_sim.py, also sends its cell info) are counted, not failed.
        deadline = t + self.args.timeout
        others = []
        while True:
            left = deadline - time.monotonic()
            try:
                when, data = await asyncio.wait_for(self.queue.get(), max(left, 0.001))
            except asyncio.TimeoutError:
                break
            if data == payload:
                self.stats.ok += 1
                self.stats.extra += len(others)
                self.stats.rtt.append((when - t) * 1000)
                return
            others.append(data)
        if others:
            self.stats.mismatch += 1
            self.say(f"  {len(payload)} bytes: echo differs: sent {payload.hex()} got {[d.hex() for d in others]}")
        else:
            self.stats.lost += 1
            self.say(f"  {len(payload)} bytes: no echo within {self.args.timeout} s")

    async def session(self, first):
        self.stats.sessions += 1
        dev = await BleakScanner.find_device_by_name(self.args.name, timeout=self.args.scan)
        if dev is None:
            self.stats.session_fail += 1
            self.say(f"{self.args.name} not found (is the spike firmware running and nothing else connected?)")
            return False
        async with BleakClient(dev) as client:
            mtu = getattr(client, "mtu_size", 23)
            if first:
                self.say(f"connected to {self.args.name}, MTU {mtu}")
                for service in client.services:
                    for ch in service.characteristics:
                        self.say(f"  {service.uuid[4:8]}/{ch.uuid[4:8]}: {', '.join(ch.properties)}")
                try:
                    self.say(f"  read ff02: {(await client.read_gatt_char(FF02)).hex() or '(empty)'}")
                except Exception as e:  # noqa: BLE001
                    self.say(f"  read ff02 failed: {e!r}")
            await client.start_notify(FF02, self.on_notify)
            biggest = max(1, min(self.args.max_size, mtu - 3))
            sizes = sorted({1, 6, 20, min(100, biggest), biggest})
            before = self.stats.ok
            for i in range(self.args.rounds):
                size = sizes[i % len(sizes)]
                payload = bytearray(os.urandom(size))
                if payload[0] == 0x73:  # not an app frame: a battery would answer it
                    payload[0] = 0x74
                await self.echo(client, bytes(payload), response=True)
            if "write-without-response" in client.services.get_characteristic(FF01).properties:
                await self.echo(client, b"no-response", response=False)
            try:
                await client.write_gatt_char(FF06, b"\x04\x05", response=True)
            except Exception as e:  # noqa: BLE001
                self.say(f"  write ff06 failed: {e!r}")
            self.say(f"session {self.stats.sessions}: {self.stats.ok - before} echoes ok, sizes {sizes}")
            await client.stop_notify(FF02)
        return True

    async def run(self):
        end = self.t0 + self.args.duration
        first = True
        while True:
            try:
                if await self.session(first):
                    first = False
            except Exception as e:  # noqa: BLE001
                self.stats.session_fail += 1
                self.say(f"session {self.stats.sessions} failed: {e!r}")
            if time.monotonic() >= end:
                break
            await asyncio.sleep(self.args.pause)
        s = self.stats
        self.say("---")
        self.say(f"sessions {s.sessions}, failed {s.session_fail}")
        self.say(f"echoes ok {s.ok}, lost {s.lost}, wrong {s.mismatch}, writes failed {s.write_fail}")
        if s.extra:
            self.say(f"other notifications in between: {s.extra}")
        if s.rtt:
            self.say(
                f"round trip ms: min {min(s.rtt):.0f}, median {statistics.median(s.rtt):.0f}, max {max(s.rtt):.0f}"
            )
        good = s.ok > 0 and not (s.lost or s.mismatch or s.write_fail or s.session_fail)
        self.say("RESULT: " + ("PASS" if good else "FAIL"))
        return good


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", default="HMA-1_SPIK", help="BLE name of the spike firmware")
    ap.add_argument("--rounds", type=int, default=20, help="echo tests per connection")
    ap.add_argument("--duration", type=float, default=0, help="keep reconnecting for this many seconds")
    ap.add_argument("--pause", type=float, default=10, help="seconds between connections")
    ap.add_argument("--max-size", type=int, default=244, help="largest write in bytes")
    ap.add_argument("--timeout", type=float, default=3, help="seconds to wait for each echo")
    ap.add_argument("--scan", type=float, default=20, help="seconds to look for the device")
    args = ap.parse_args()
    ok = asyncio.run(Check(args).run())
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
