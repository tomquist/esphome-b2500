#!/usr/bin/env python3
"""Record how a real FC41D answers the commands the B2500 MCU sends.

Run against the module's main UART (FC41D-TE-B: the M_UART_USB port) while
the module still has its stock Quectel firmware. It sends the B2500 MCU's
command set (hm2500 research repo, doc/esphome-fc41d-plan.md §4) one at a
time and records every byte the module sends back, with timestamps, so the
response formats and timing of hame_at's emulator (components/hame_at) can
be checked against a real module.

    pip install pyserial
    python3 tools/fc41d_probe.py PORT --ssid MyWifi --password secret

PORT is the serial device of the main UART: /dev/cu.usbserial-... or
/dev/cu.SLAB_USBtoUART... on macOS, /dev/ttyUSB<n> on Linux, COM<n> on
Windows. Run with --list to see the serial ports and their USB details.

Optional steps:
  --ble-wait 60   wait up to 60 s for a phone: connect to HMA-1_TEST with
                  nRF Connect, enable notifications on ff02, and write a few
                  bytes to ff01/ff06 when asked
  --udp-port N    open a UDP service on port N; when asked, send a line
                  from another terminal: nc -u <module-ip> N, type hello,
                  Enter (keep nc open: the module answers "pong")
  --mqtt-host H   MQTT broker without TLS (default test.mosquitto.org), with
                  --mqtt-port, --mqtt-user, --mqtt-password (masked)

Output: fc41d-probe-<timestamp>.log (readable) and .jsonl (exact bytes).
The Wi-Fi password is masked in both. The probe joins Wi-Fi with
AT+QSTAAPINFO, which (unlike AT+QSTAAPINFODEF) does not save the credentials
in the module's flash.
"""

import argparse
import json
import re
import sys
import threading
import time

try:
    import serial  # pyserial
except ImportError:  # pragma: no cover
    sys.exit("pip install pyserial")


def list_ports():
    from serial.tools import list_ports as lp

    ports = sorted(lp.comports(), key=lambda p: p.device)
    if not ports:
        print("No serial ports found. Check the USB cable (some only charge) and,")
        print("on older macOS, the Silicon Labs CP210x driver.")
    for p in ports:
        print(f"{p.device:40} {p.description or ''}  {p.hwid or ''}")
    print("The TE-B has two CP2102N ports: plug in only M_UART_USB to tell which one is the main UART.")


class Probe:
    def __init__(self, port, baud, out, secrets):
        self.ser = serial.Serial(port, baud, timeout=0.02)
        self.t0 = time.monotonic()
        self.rx = bytearray()
        self.lock = threading.Condition()
        self.stop = False
        self.secrets = [x for x in secrets if x]
        self.log = open(out + ".log", "w", encoding="utf-8")
        self.jsonl = open(out + ".jsonl", "w", encoding="utf-8")
        threading.Thread(target=self._reader, daemon=True).start()

    def now(self):
        return time.monotonic() - self.t0

    def _record(self, direction, data):
        t = self.now()
        for secret in self.secrets:
            data = data.replace(secret.encode("latin1"), b"***")
        text = data.decode("latin1")
        self.jsonl.write(json.dumps({"t": round(t, 4), "dir": direction, "hex": data.hex(), "text": text}) + "\n")
        shown = text.replace("\r", "\\r").replace("\n", "\\n")
        self.log.write(f"{t:9.3f} {direction} {shown}\n")
        self.log.flush()
        self.jsonl.flush()
        print(f"{t:9.3f} {direction} {shown}")

    def _reader(self):
        while not self.stop:
            chunk = self.ser.read(512)
            if chunk:
                self._record("<", chunk)
                with self.lock:
                    self.rx.extend(chunk)
                    self.lock.notify_all()

    def note(self, text):
        self.log.write(f"# {text}\n")
        print(f"# {text}")

    def send(self, data):
        if isinstance(data, str):
            data = data.encode("latin1")
        with self.lock:
            self.rx.clear()
        if data:
            self._record(">", data)
            self.ser.write(data)

    def wait_for(self, tokens, timeout):
        """Wait until one of tokens is in what the module sent since the last
        send. FINAL matches a final result line (OK / ERROR / +CME ERROR); the
        module sends a bare "OK\\r\\n", not "\\r\\nOK\\r\\n"."""
        deadline = time.monotonic() + timeout
        with self.lock:
            while time.monotonic() < deadline:
                text = self.rx.decode("latin1")
                for tok in tokens:
                    if tok is FINAL:
                        m = FINAL_RE.search(text)
                        if m:
                            return m.group(1)
                    elif tok in text:
                        return tok
                self.lock.wait(0.05)
        return None

    def cmd(self, line, wait=None, timeout=5.0, settle=0.5):
        """Send a command, wait for its final result, then keep listening a bit
        for URCs that follow (their delay after OK is part of what we measure)."""
        wait = wait or (FINAL,)
        self.send(line + "\r\n")
        result = self.wait_for(wait, timeout)
        if result is None:
            self.note(f"no answer within {timeout}s")
            if not self.alive():
                self.note("module stopped answering AT; trying to recover")
                self.recover()
        time.sleep(settle)
        return result

    def alive(self, timeout=2.0):
        self.send("AT\r\n")
        return self.wait_for((FINAL,), timeout) is not None

    def escape(self):
        """Leave a transparent mode: 1 s of silence, "+++", 1 s of silence."""
        time.sleep(1.2)
        self.send("+++")
        self.wait_for((FINAL,), 1.5)
        time.sleep(0.5)

    def recover(self):
        self.escape()
        if self.alive():
            self.note("+++ brought the AT interface back")
            return True
        self.note("press the module's RESET button (waiting 30 s)")
        if self.wait_for(("ready",), 30):
            time.sleep(1)
            return True
        self.note("module did not recover; the remaining steps will fail")
        return False

    def quiet(self, seconds):
        time.sleep(seconds)


FINAL = object()
FINAL_RE = re.compile(r"(?:^|\n)(OK|ERROR|\+CME ERROR[^\r\n]*)\r\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", nargs="?", help="module main UART (FC41D-TE-B: M_UART_USB)")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--ssid", help="Wi-Fi to join (needed for the network steps)")
    ap.add_argument("--password", default="")
    ap.add_argument("--ble-wait", type=int, default=0, help="seconds to wait for a phone on BLE")
    ap.add_argument("--udp-port", type=int, default=0, help="open a UDP service on this port")
    ap.add_argument("--mqtt-host", default="test.mosquitto.org", help="MQTT broker without TLS (e.g. Home Assistant's)")
    ap.add_argument("--mqtt-port", type=int, default=1883)
    ap.add_argument("--mqtt-user", default="")
    ap.add_argument("--mqtt-password", default="")
    args = ap.parse_args()
    if args.list or not args.port:
        list_ports()
        return

    out = time.strftime("fc41d-probe-%Y%m%d-%H%M%S")
    try:
        p = Probe(args.port, args.baud, out, [args.password, args.mqtt_password])
    except serial.SerialException as e:
        print(f"Cannot open {args.port}: {e}\n")
        list_ports()
        sys.exit(1)

    p.note("press the module's RESET button now to record the boot banner (waiting 10 s)")
    if p.wait_for(("ready",), 10):
        p.quiet(2)
    else:
        p.note("no ready banner; carrying on")

    p.note("system")
    p.cmd("AT")
    p.cmd("ATE0")
    p.cmd("ATE1")
    p.cmd("AT", settle=0.2)  # does ATE1 echo?
    p.cmd("ATE0")
    p.cmd("AT+QECHO=0")
    p.cmd("AT+QVERSION")
    p.cmd("AT+QWLMAC")
    p.cmd("AT+QBLEADDR?")
    p.cmd("AT+QSSLCERT?")
    p.cmd("AT+QSTAST")
    p.cmd("AT+QGETWIFISTATE")
    p.cmd("AT+QGETIP=station")
    p.cmd("AT+QBLESTAT")
    p.cmd("AT+NOSUCHCMD")

    if args.ssid:
        p.note("Wi-Fi join (not saved)")
        p.send(f"AT+QSTAAPINFO={args.ssid},{args.password}\r\n")
        got = p.wait_for(("GOT_IP",), 40)
        p.note(f"join result: {got}")
        p.quiet(3)
        p.cmd("AT+QSTAST")
        p.cmd("AT+QGETWIFISTATE")
        p.cmd("AT+QGETIP=station")

        p.note("HTTP: time request (measures the +QHTTPGET delay after OK)")
        p.cmd('AT+QHTTPCFG="url","http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid=000000000000&fcv=0&aid=HMA-1&sv=0"')
        p.cmd("AT+QHTTPGET=60", settle=0.1)
        p.wait_for(("+QHTTPGET",), 65)
        p.quiet(1)
        # The body runs straight into "OK" ("..._0_0_0OK"): wait for the URC.
        p.cmd("AT+QHTTPREAD=60", wait=("+QHTTPREAD:",), timeout=65)

        p.note(f"MQTT against {args.mqtt_host}:{args.mqtt_port}, no TLS")
        p.cmd('AT+QMTCFG="version",0,4')
        p.cmd('AT+QMTCFG="keepalive",0,60')
        p.cmd(f'AT+QMTOPEN=0,"{args.mqtt_host}",{args.mqtt_port}', settle=0.1)
        p.wait_for(("+QMTOPEN",), 30)
        p.quiet(0.5)
        login = f',"{args.mqtt_user}","{args.mqtt_password}"' if args.mqtt_user else ""
        p.cmd(f'AT+QMTCONN=0,"fc41d-probe"{login}', settle=0.1)
        p.wait_for(("+QMTCONN",), 30)
        p.quiet(0.5)
        topic = "fc41d-probe/" + out
        p.cmd(f'AT+QMTSUB=0,1,"{topic}/#",0', settle=0.1)
        p.wait_for(("+QMTSUB",), 30)
        p.quiet(0.5)
        payload = "p1=1,p2=0,w1=24"
        p.note("publish to a topic we subscribed to: does the broker's echo arrive as +QMTRECV?")
        p.cmd(f'AT+QMTPUB=0,0,0,0,"{topic}/ctrl",{len(payload)},"{payload}"', settle=0.1)
        p.wait_for(("+QMTRECV",), 10)
        p.quiet(2)
        p.cmd("AT+QMTDISC=0", settle=1)
        p.cmd("AT+QMTCLOSE=0", settle=1)

        if args.udp_port:
            port = args.udp_port
            p.note(f"UDP service on port {port}, buffer mode: send a line from nc now (30 s)")
            p.cmd(f'AT+QIOPEN=1,"UDP SERVICE","127.0.0.1",0,{port},0', settle=0.1)
            p.wait_for(("+QIOPEN",), 10)
            p.wait_for(('+QIURC: "recv"',), 30)
            p.quiet(1)
            p.cmd("AT+QIRD=1,0")
            p.cmd("AT+QIRD=1,1500")
            m = re.search(r'\+QIRD: \d+,"([\d.]+)",(\d+)', p.rx.decode("latin1"))
            p.cmd("AT+QICLOSE=1", settle=1)

            p.note(f"UDP service on port {port}, push mode as the B2500 opens it: send another line (30 s)")
            p.cmd(f'AT+QIOPEN=1,"UDP SERVICE","127.0.0.1",0,{port},1', settle=0.1)
            p.wait_for(("+QIOPEN",), 10)
            p.wait_for(('+QIURC: "recv"',), 30)
            p.quiet(1)
            if m:
                ip, rport = m.group(1), m.group(2)
                p.note(f"reply to {ip}:{rport} as the B2500 sends: nc should print 'pong' twice")
                p.cmd(f'AT+QISEND=1,4,"pong","{ip}",{rport}', settle=1)
                p.cmd(f'AT+QISEND=1,5,"{ip}",{rport}', wait=(">",), settle=0.2)
                p.send("pong\n")
                p.wait_for((FINAL,), 5)
                p.quiet(1)
            p.cmd("AT+QISTATE?")
            p.cmd("AT+QICLOSE=1", settle=1)

    p.note("BLE setup as the B2500 MCU does it")
    p.cmd("AT+QBLEINIT=2")
    p.cmd("AT+QBLENAME=HMA-1_TEST")
    p.cmd("AT+QBLEGATTSSRV=FF00")
    for c in ("ff01", "ff02", "ff06"):
        p.cmd(f"AT+QBLEGATTSCHAR={c}")
    p.cmd("AT+QBLEADVPARAM=150,150")
    p.cmd("AT+QBLEADVSTART")
    p.cmd("AT+QBLESTAT")

    if args.ble_wait:
        p.note(f"BLE: in nRF Connect connect to HMA-1_TEST, enable notifications on ff02 ({args.ble_wait} s)")
        p.send("")
        got = p.wait_for(("CONNECTED",), args.ble_wait)
        p.note(f"connect URC: {got}")
        p.cmd("AT+QBLESTAT")
        p.note("now write a few bytes to ff01, then to ff06 (30 s): how do writes reach the UART?")
        p.send("")
        p.quiet(30)
        p.note("notifications from the MCU side: hex, then text (watch ff02 on the phone)")
        p.cmd("AT+QBLEGATTSNTFY=ff02,6,730623030154")
        p.cmd("AT+QBLEGATTSNTFY=ff02,10_24_25_3162_3160_3161")
        p.quiet(3)

    p.note("QBLETRANMODE=ff02 as the B2500 MCU sends it; does the module keep answering AT?")
    p.cmd("AT+QBLETRANMODE=ff02")
    if p.alive():
        p.note("still in AT mode after QBLETRANMODE")
        p.cmd("AT+QBLESTAT")
    else:
        p.note("no AT answers after QBLETRANMODE: transparent mode")
        if args.ble_wait:
            p.note("UART bytes now go to the phone? watch ff02 for 'hello-from-uart'")
            p.send("hello-from-uart")
            p.note("write a few bytes to ff01 again (20 s): do they arrive raw?")
            p.quiet(20)
        p.escape()
        if p.alive():
            p.note("+++ left transparent mode")
            p.cmd("AT+QBLESTAT")
        else:
            p.recover()

    p.note("done")
    p.stop = True
    time.sleep(0.2)
    print(f"\nWrote {out}.log and {out}.jsonl")


if __name__ == "__main__":
    main()
