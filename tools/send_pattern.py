#!/usr/bin/env python3
"""Validate a .crochet pattern and send it to the machine over Wi-Fi.

Examples:
  python tools/send_pattern.py patterns/leaf.crochet --check
  python tools/send_pattern.py patterns/chain-cord-stem.crochet --ip 192.168.1.50 --start
"""
import argparse, json, re, sys, time, urllib.request, urllib.error

OPS = {"CH": 1, "SC": 1, "DC": 1, "ROUND": 1, "TURN": 0, "JOIN": 0, "PAUSE": 2}  # 1 = needs count, 2 = text
RUNS = {"CH", "PAUSE"}  # executed by the current hardware build
SECONDS_PER_STITCH = 2.5


def parse(path):
    steps, errors = [], []
    with open(path, encoding="utf-8") as f:
        for no, raw in enumerate(f, 1):
            line = raw.split("#")[0].strip()
            if not line:
                continue
            m = re.match(r"^(\w+)\s*(.*)$", line)
            op, arg = m.group(1).upper(), m.group(2).strip()
            if op not in OPS:
                errors.append(f"Line {no}: unknown instruction '{m.group(1)}'")
                continue
            n = 0
            if OPS[op] == 1:
                if not arg.isdigit() or not 1 <= int(arg) <= 999:
                    errors.append(f"Line {no}: {op} needs a whole number from 1 to 999")
                    continue
                n = int(arg)
            steps.append((op, n, arg))
    return steps, errors


def summary(steps):
    total = sum(n for _, n, _ in steps)
    runnable = sum(n for op, n, _ in steps if op in RUNS)
    skipped = sum(1 for op, _, _ in steps if op not in RUNS)
    mins = round(runnable * SECONDS_PER_STITCH / 60)
    return (f"{len(steps)} steps, {total} stitches in total. The current build can run "
            f"{runnable} (about {mins} min); {skipped} step(s) will be skipped.")


def to_program(steps):
    out = []
    for op, n, arg in steps:
        out.append(op + (f" {arg}" if op == "PAUSE" and arg else f" {n}" if n else ""))
    return "\n".join(out)


def call(base, path, data=None):
    req = urllib.request.Request(base + path, data=data.encode() if data else None,
                                 headers={"Content-Type": "text/plain"} if data else {})
    with urllib.request.urlopen(req, timeout=10) as r:
        return r.read().decode()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pattern")
    ap.add_argument("--ip", help="machine IP address")
    ap.add_argument("--check", action="store_true", help="validate only, do not send")
    ap.add_argument("--start", action="store_true", help="start after sending and follow progress")
    a = ap.parse_args()

    steps, errors = parse(a.pattern)
    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        sys.exit(1)
    print(summary(steps))
    if a.check:
        return
    if not a.ip:
        sys.exit("Provide --ip, or use --check to validate only.")
    base = "http://" + a.ip
    try:
        print(call(base, "/program", to_program(steps)))
        if not a.start:
            return
        print(call(base, "/start"))
        while True:
            time.sleep(1)
            s = json.loads(call(base, "/status"))
            print(f"{s['state']}: step {min(s['step'] + 1, s['steps'])}/{s['steps']} {s['msg']}")
            if s["state"] == "paused":
                input("Paused. Press Enter to resume...")
                call(base, "/resume")
            elif s["state"] in ("finished", "idle"):
                break
    except urllib.error.URLError as e:
        sys.exit(f"Cannot reach the machine: {e.reason}")


if __name__ == "__main__":
    main()
