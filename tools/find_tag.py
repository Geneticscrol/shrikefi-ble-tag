#!/usr/bin/env python3
"""
find_tag.py - laptop "finder" for the ShrikeFi BLE Tag.

Scans for the tag (by name and/or iBeacon UUID), prints a live RSSI bar and a
rough distance estimate, and can ask the tag to blink/beep via the standard
Immediate Alert Service (0x1802 / Alert Level 0x2A06).

    pip install bleak
    python find_tag.py                       # live RSSI "hot / cold" view
    python find_tag.py --uuid 5F1C0DE0-...   # match your own iBeacon UUID
    python find_tag.py --alert 2             # connect and make the tag blink fast
    python find_tag.py --alert 0             # stop the alert

Works on Windows, Linux (BlueZ) and macOS. Note: macOS (like iOS) hides
iBeacon packets from apps, so on a Mac the tag is matched by its name from the
connectable "Find Me" frame instead.

Distance is only an estimate: RSSI swings by +/-5 dB with body blocking,
orientation and reflections. Use it as "warmer / colder", not a tape measure.
"""
import argparse
import asyncio
import sys
import time
import uuid as uuidlib

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover
    sys.exit("bleak is not installed. Run:  pip install bleak")

APPLE_COMPANY_ID = 0x004C
ALERT_LEVEL_CHAR = "00002a06-0000-1000-8000-00805f9b34fb"


def parse_ibeacon(manufacturer_data):
    """Return (uuid, major, minor, measured_power) or None."""
    data = manufacturer_data.get(APPLE_COMPANY_ID)
    if not data or len(data) < 23 or data[0] != 0x02 or data[1] != 0x15:
        return None
    beacon_uuid = str(uuidlib.UUID(bytes=bytes(data[2:18]))).upper()
    major = int.from_bytes(data[18:20], "big")
    minor = int.from_bytes(data[20:22], "big")
    measured_power = int.from_bytes(data[22:23], "big", signed=True)
    return beacon_uuid, major, minor, measured_power


def estimate_distance(rssi, measured_power=-59, n=2.0):
    """Log-distance path-loss model. n ~2 open space, 2.5-4 indoors."""
    return 10 ** ((measured_power - rssi) / (10 * n))


def bar(rssi):
    # map -100..-30 dBm to 0..30 chars
    level = max(0, min(30, int((rssi + 100) * 30 / 70)))
    return "#" * level + "." * (30 - level)


def hint(rssi):
    if rssi > -55:
        return "VERY HOT - it's right here"
    if rssi > -67:
        return "hot"
    if rssi > -80:
        return "warm"
    return "cold"


async def scan(args):
    want_uuid = args.uuid.upper() if args.uuid else None
    smoothed = {}
    last_print = 0.0
    print(f"Scanning for name='{args.name}'" + (f" or UUID={want_uuid}" if want_uuid else "") + " ... Ctrl+C to stop\n")

    def on_adv(device, adv):
        nonlocal last_print
        beacon = parse_ibeacon(adv.manufacturer_data)
        name = adv.local_name or device.name or ""
        match_name = args.name and name == args.name
        match_uuid = beacon and (want_uuid is None or beacon[0] == want_uuid)
        if not (match_name or match_uuid):
            return
        prev = smoothed.get(device.address, adv.rssi)
        rssi = 0.7 * prev + 0.3 * adv.rssi  # simple exponential smoothing
        smoothed[device.address] = rssi
        now = time.time()
        if now - last_print < 0.5:
            return
        last_print = now
        mp = beacon[3] if beacon else args.measured_power
        dist = estimate_distance(rssi, mp, args.n)
        kind = f"iBeacon {beacon[1]}/{beacon[2]}" if beacon else "Find Me"
        print(f"{device.address}  {kind:<14} RSSI {rssi:6.1f} dBm  [{bar(rssi)}]  ~{dist:4.1f} m  {hint(rssi)}")

    scanner = BleakScanner(on_adv)
    await scanner.start()
    try:
        if args.timeout:
            await asyncio.sleep(args.timeout)
        else:
            while True:
                await asyncio.sleep(1)
    finally:
        await scanner.stop()


async def alert(args):
    print(f"Looking for '{args.name}' ...")
    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: (adv.local_name or d.name) == args.name, timeout=15.0
    )
    if device is None:
        sys.exit("Tag not found. Is it powered and advertising (LED blinking)?")
    print(f"Connecting to {device.address} ...")
    async with BleakClient(device) as client:
        await client.write_gatt_char(ALERT_LEVEL_CHAR, bytes([args.alert]), response=False)
        print(f"Alert level {args.alert} sent ({['stop', 'mild', 'HIGH'][args.alert]}).")
        await asyncio.sleep(1.0)


def main():
    p = argparse.ArgumentParser(description="Find a ShrikeFi BLE Tag from your laptop")
    p.add_argument("--name", default="ShrikeFi-Tag", help="TAG_NAME from config.h")
    p.add_argument("--uuid", help="IBEACON_UUID from config.h (optional)")
    p.add_argument("--measured-power", type=int, default=-59, help="RSSI at 1 m, for Find Me frames")
    p.add_argument("--n", type=float, default=2.5, help="path-loss exponent (2 open air, 3 indoors)")
    p.add_argument("--timeout", type=float, default=0, help="stop scanning after N seconds (0 = forever)")
    p.add_argument("--alert", type=int, choices=[0, 1, 2], help="connect and write Alert Level 0/1/2")
    args = p.parse_args()
    try:
        asyncio.run(alert(args) if args.alert is not None else scan(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
