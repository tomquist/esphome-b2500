#!/usr/bin/env python3
"""Play the B2500's MCU against an FC41D running hame_at (bench testing).

On the Quectel FC41D-TE-B eval kit, J201 is the module's main UART, the one the
battery MCU drives. Connect it to a PC and run this script on that port: it
sends the AT commands the B2500 MCU sends (from the HMA/HMB MCU images, see
doc/esphome-fc41d-plan.md in the hm2500 research repo), answers status polls
like the battery, and echoes BLE writes back as notifications. ESPHome's logs
stay on J202.

    pip install pyserial
    python3 tools/fc41d_mcu_sim.py /dev/ttyUSB0

Cloud forwarding test against your own TLS broker (e.g. Mosquitto with
`require_certificate true`); the module then opens a real TLS session with
these certificates, as it would with the battery's:

    python3 tools/fc41d_mcu_sim.py /dev/ttyUSB0 --broker 192.168.1.10:8883 \
        --ca ca.pem --cert client.pem --key client.key

Then publish "cd=1" to hame_energy/HMA-1/App/<id>/ctrl on that broker and watch
the simulated battery answer on .../device/<id>/ctrl.
"""

import argparse
import sys
import threading
import time

try:
    import serial  # pyserial
except ImportError:  # pragma: no cover
    sys.exit("pip install pyserial")

DEVICE_TYPE = "HMA-1"
# The battery's state, as its runtime-info reply (cd=1) reports it. Commands
# from the module (cd=17 charging mode, ...) change it, like on a battery.
STATE = {
    "p1": "1", "p2": "1", "w1": "124", "w2": "118", "pe": "63", "vv": "226", "sv": "3", "cs": "0",
    "cd": "0", "am": "0", "o1": "1", "o2": "1", "do": "90", "lv": "240", "cj": "2", "kn": "1180",
    "g1": "110", "g2": "105", "b1": "0", "b2": "0", "md": "0",
    "d1": "1", "e1": "0:0", "f1": "23:59", "h1": "200",
    "d2": "0", "e2": "0:0", "f2": "23:59", "h2": "80",
    "d3": "0", "e3": "0:0", "f3": "23:59", "h3": "80",
    "d4": "0", "e4": "0:0", "f4": "23:59", "h4": "80",
    "d5": "0", "e5": "0:0", "f5": "23:59", "h5": "80",
    "sg": "1", "sp": "80", "st": "65535", "tl": "17", "th": "21", "tc": "0", "tf": "0",
    "fc": "202310231502", "id": "5", "a0": "63", "a1": "0", "a2": "0", "l0": "1", "l1": "0",
    "bc": "540", "bs": "380", "pt": "910", "it": "350", "c0": "255", "c1": "0", "m0": "-120",
    "m1": "65535", "m2": "65535", "m3": "215", "lmo": "800", "lmi": "1000", "lmf": "0", "ct_t": "3",
    "tc_dis": "1", "ws": "-61",
}
CT_DIAG = {"ct_t": "3", "phase_t": "4", "dchrg_t": "1", "seq_s": "0", "c0": "10", "c1": "20", "c2": "30",
           "cp": "60", "op": "200"}
ELECTRICAL = {"p1": "1", "p2": "1", "m1": "32100", "m2": "31800", "c1": "3860", "c2": "3710", "w1": "124",
              "w2": "118", "e1": "0", "e2": "0", "o1": "1", "o2": "1", "i1": "29400", "i2": "29300",
              "c3": "3740", "c4": "3580", "g1": "110", "g2": "105", "bb": "9", "bv": "50120", "bc": "180",
              "sb": "0", "sv": "0", "sc": "0", "lb": "0", "lv": "0", "lc": "0"}
CELL_MV = {f"a{h}": str(3290 + i) for i, h in enumerate("0123456789abcd")} | {"ae": "0", "af": "0"}


def kv(d):
    return ",".join(f"{k}={v}" for k, v in d.items())


# Cell info as the MCU sends it: soc_t1_t2_mv1.._mv14, as text.
CELLS = "63_17_21_" + "_".join(["3301"] * 14)


class Sim:
    def __init__(self, port, args):
        self.ser = serial.Serial(port, 115200, timeout=0.05)
        self.args = args
        self.lines = []
        self.lock = threading.Condition()
        # One command at a time, like the real MCU.
        self.cmd_lock = threading.Lock()
        self.http_body_pending = False
        self.dev_id = "aabbccddeeff"
        self.stop = False

    # -- I/O -----------------------------------------------------------------
    def reader(self):
        buf = b""
        while not self.stop:
            chunk = self.ser.read(256)
            if not chunk:
                continue
            buf += chunk
            while True:
                # Lines are framed "\r\n<text>\r\n", except the Wi-Fi/BLE state
                # URCs (no leading CRLF, as on a stock module); anything else is
                # a raw BLE write.
                bare = buf.startswith((b"+QSTASTAT:", b"+QBLESTAT:", b"+QBLEMTU:"))
                if bare:
                    end = buf.find(b"\r\n")
                    if end < 0:
                        break
                    line = buf[:end].decode(errors="replace")
                    buf = buf[end + 2 :]
                    print(f"<MOD {line}")
                    self.on_line(line)
                elif buf.startswith(b"\r\n"):
                    end = buf.find(b"\r\n", 2)
                    if end < 0:
                        break
                    line = buf[2:end].decode(errors="replace")
                    buf = buf[end + 2 :]
                    print(f"<MOD {line}")
                    self.on_line(line)
                elif buf.startswith(b"CONNECT") or buf.startswith(b"> "):
                    end = buf.find(b"\r\n")
                    if end < 0:
                        break
                    buf = buf[end + 2 :]
                else:
                    end = buf.find(b"\r\n")
                    raw, buf = (buf, b"") if end < 0 else (buf[:end], buf[end:])
                    if raw and self.http_body_pending:
                        self.http_body_pending = False
                        print(f"<MOD (HTTP body) {raw.decode(errors='replace')}")
                    elif raw:
                        print(f"<MOD (raw {len(raw)} bytes) {raw.hex()}")
                        self.on_ble_write(raw)
                    if end < 0:
                        break

    def send(self, cmd, wait=("OK", "ERROR"), timeout=5.0):
        with self.cmd_lock:
            with self.lock:
                start = len(self.lines)
            if cmd.startswith("AT+QHTTPREAD"):
                self.http_body_pending = True
            print(f"MCU> {cmd}")
            self.ser.write(cmd.encode() + b"\r\n")
            deadline = time.time() + timeout
            with self.lock:
                while time.time() < deadline:
                    for line in self.lines[start:]:
                        if any(line.startswith(w) for w in wait):
                            return line
                    self.lock.wait(0.1)
            print(f"!! no {wait} for {cmd}")
            return None

    def wait_line(self, prefix, timeout):
        deadline = time.time() + timeout
        with self.lock:
            while time.time() < deadline:
                if any(line.startswith(prefix) for line in self.lines):
                    return True
                self.lock.wait(0.1)
        return False

    def on_line(self, line):
        with self.lock:
            self.lines.append(line)
            self.lock.notify_all()
        if line.startswith("+QMTRECV:"):
            # +QMTRECV: 0,0,"topic",len,"payload" (the payload may contain commas)
            parts = line.split('"', 3)  # quotes 1-2 hold the topic, 3 opens the payload
            payload = parts[3].rstrip().rstrip('"') if len(parts) == 4 else ""
            threading.Thread(target=self.on_command, args=(payload,), daemon=True).start()

    def on_ble_write(self, data):
        # Echo app frames back, as a stand-in for the battery's reply.
        threading.Thread(target=self.notify, args=(data,), daemon=True).start()

    # -- battery behaviour ---------------------------------------------------
    def publish(self, payload):
        topic = f"hame_energy/{DEVICE_TYPE}/device/{self.dev_id}/ctrl"
        self.send(f'AT+QMTPUB=0,0,0,0,"{topic}",{len(payload)},"{payload}"', wait=("+QMTPUB",))

    def publish_status(self):
        self.publish(kv(STATE))

    def on_command(self, payload):
        """Apply a command the way the B2500 v2 does (hm2mqtt's command map)."""
        args = {}
        for item in payload.split(","):
            k, _, v = item.partition("=")
            args[k.strip()] = v.strip()
        cd = args.get("cd", "")
        md = args.get("md")
        if cd in ("1", "01"):
            self.publish_status()
        elif cd == "28":
            self.publish(kv(CT_DIAG))
        elif cd == "16":
            self.publish(kv(ELECTRICAL))
        elif cd == "13":
            self.publish(kv(CELL_MV))
        elif cd in ("17", "3") and md is not None:
            STATE["cs"] = md
        elif cd in ("18", "4") and md is not None:
            STATE["md"] = md
        elif cd in ("19", "5") and md is not None:
            STATE["do"] = md
        elif cd in ("20", "7"):
            STATE["md"] = "0"  # applying a schedule ends adaptive mode
            for n in range(1, 6):
                if f"a{n}" in args:
                    STATE[f"d{n}"], STATE[f"e{n}"] = args[f"a{n}"], args.get(f"b{n}", "0:0")
                    STATE[f"f{n}"], STATE[f"h{n}"] = args.get(f"e{n}", "0:0"), args.get(f"v{n}", "0")
        elif cd == "22" and md is not None:
            STATE["c0"] = md
        elif cd == "31" and "touchuan_disa" in args:
            STATE["tc_dis"] = args["touchuan_disa"]
        elif cd == "27" and "dchrg" in args:
            CT_DIAG["dchrg_t"] = args["dchrg"]
        elif cd == "27" and "seq_check" in args:
            CT_DIAG["seq_s"] = "3"
        elif cd in ("10", "11"):
            print(f"== battery {'restart' if cd == '10' else 'factory reset'} requested (ignored)")
        else:
            print(f"== unhandled command: {payload}")
            return
        print(f"== applied {payload}")

    def notify(self, data):
        self.send(f"AT+QBLEGATTSNTFY=ff02,{len(data)},{data.hex()}")
        if data[:1] == b"\x73":  # an app frame: the battery also sends its cell info
            self.send(f"AT+QBLEGATTSNTFY=ff02,{CELLS}")

    def upload(self, kind, path):
        with open(path, "rb") as f:
            pem = f.read()
        if self.send(f'AT+QSSLCERT="{kind}",2,{len(pem)}', wait=("CONNECT", "ERROR")) != "CONNECT":
            return
        with self.cmd_lock:
            self.ser.write(pem)
            time.sleep(0.5)

    def boot(self):
        self.send("AT")
        self.send("AT+QVERSION")
        addr = self.send("AT+QBLEADDR?", wait=("+QBLEADDR",))
        if addr:
            self.dev_id = addr.split(":", 1)[1].replace(":", "").strip().lower()
        self.send("AT+QBLEINIT=2")
        self.send(f"AT+QBLENAME={DEVICE_TYPE}_{self.dev_id[-4:].upper()}")
        self.send("AT+QBLEGATTSSRV=FF00")
        for c in ("ff01", "ff02", "ff06"):
            self.send(f"AT+QBLEGATTSCHAR={c}")
        self.send("AT+QBLEADVPARAM=150,150")
        self.send("AT+QBLEADVSTART")
        # No AT+QBLETRANMODE here: it switches the module to transparent mode
        # at once (no AT answers until "+++"). The B2500 sends it only for
        # bulk transfers from the app, never during setup.
        self.send("AT+QSTAST", wait=("+QSTAST",))
        # Like the battery: no network use before the station has an address.
        if not self.wait_line("+QSTASTAT:GOT_IP", 30):
            print("!! no GOT_IP from the module")
        self.send("AT+QGETWIFISTATE")
        self.send("AT+QGETIP=station")

    def http_time(self):
        url = (
            f"http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid={self.dev_id}"
            f"&fcv=202409090159&aid={DEVICE_TYPE}&sv=226&mv=1"
        )
        self.send(f'AT+QHTTPCFG="url","{url}"')
        result = self.send("AT+QHTTPGET=60", wait=("+QHTTPGET", "ERROR"), timeout=20)
        if result is None or not result.startswith("+QHTTPGET: 0,"):
            print(f"!! time request failed: {result}")
            return
        self.send("AT+QHTTPREAD=60", wait=("+QHTTPREAD",))
        self.http_body_pending = False

    def mqtt(self):
        a = self.args
        host, port = "eu.hamedata.com", 8883
        tls = True
        if a.broker:
            host, _, p = a.broker.partition(":")
            port = int(p or 8883)
            tls = bool(a.cert)
        if tls and a.cert:
            if a.ca:
                self.upload("CA", a.ca)
            self.upload("User Cert", a.cert)
            self.upload("User Key", a.key)
        self.send('AT+QMTCFG="version",0,4')
        self.send('AT+QMTCFG="keepalive",0,60')
        self.send(f'AT+QMTCFG="ssl",0,{1 if tls else 0},2')
        self.send(f'AT+QMTOPEN=0,"{host}",{port}', wait=("+QMTOPEN",))
        self.send(f'AT+QMTCONN=0,"mst_{self.dev_id}"', wait=("+QMTCONN",))
        self.send(f'AT+QMTSUB=0,1,"hame_energy/{DEVICE_TYPE}/App/{self.dev_id}/ctrl",0', wait=("+QMTSUB",))
        self.publish_status()

    def run(self):
        threading.Thread(target=self.reader, daemon=True).start()
        self.boot()
        self.http_time()
        self.mqtt()
        print("== running: status every 60 s; Ctrl-C to stop ==")
        try:
            while True:
                time.sleep(60)
                self.publish_status()
        except KeyboardInterrupt:
            self.stop = True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="serial port of the module's main UART (TE-B: J201)")
    ap.add_argument("--broker", help="host:port of a test broker (default: the Hame cloud host, no certificates)")
    ap.add_argument("--ca", help="CA certificate (PEM) to upload")
    ap.add_argument("--cert", help="client certificate (PEM) to upload")
    ap.add_argument("--key", help="client key (PEM) to upload")
    args = ap.parse_args()
    if bool(args.cert) != bool(args.key):
        ap.error("--cert and --key go together")
    Sim(args.port, args).run()


if __name__ == "__main__":
    main()
