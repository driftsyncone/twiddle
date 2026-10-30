#!/usr/bin/env python3
"""Turn a raw serial capture from screenshot_dump.cpp into tab<N>.png files.
usage: decode.py raw.log OUTDIR"""
import base64, struct, sys, zlib

raw = open(sys.argv[1], "rb").read().decode("latin1").splitlines()
out = sys.argv[2]


def png(path, w, h, rows):
    data = b"".join(b"\0" + r for r in rows)
    ch = lambda t, c: struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c))
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + ch(b"IDAT", zlib.compress(data)) + ch(b"IEND", b""))


tab, cur, w, h, n = -1, None, 0, 0, 0
for line in raw:
    line = line.strip()
    if line.startswith("TAB "):
        tab = int(line.split()[1])
    elif line.startswith("SHOT_BEGIN"):
        _, w, h = line.split()
        w, h, cur = int(w), int(h), []
    elif line.startswith("SHOT_END") and cur is not None:
        rows = [x for x in cur if len(x) == w * 4]  # drops log lines interleaved in the dump
        if len(rows) == h:
            png(f"{out}/tab{tab}.png", w, h, [base64.b64decode(x) for x in rows])
            n += 1
        else:
            print(f"tab {tab}: {len(rows)} of {h} rows, skipped (board reset during the dump?)")
        cur = None
    elif cur is not None:
        cur.append(line)
print(f"{n} screenshot(s) written to {out}")
