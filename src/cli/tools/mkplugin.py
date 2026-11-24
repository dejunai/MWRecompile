#!/usr/bin/env python3
"""Build a small TES3 plugin for testing MWRecompile.

usage: mkplugin.py OUT.esp --master Morrowind.esm:SIZE ... --script NAME=SOURCEFILE ...
Scripts get a stale/empty SCHD/SCDT so the recompiler has to rebuild them.
"""
import struct, sys, argparse

def sub(t, data):
    return t.encode() + struct.pack('<I', len(data)) + data

def rec(t, body, flags=0):
    return t.encode() + struct.pack('<III', len(body), 0, flags) + body

def tes3(masters, nrec):
    hedr = struct.pack('<fI', 1.3, 0) + b'MWRecompile test'.ljust(32, b'\0') + b'test plugin'.ljust(256, b'\0') + struct.pack('<I', nrec)
    body = sub('HEDR', hedr)
    for name, size in masters:
        body += sub('MAST', name.encode() + b'\0') + sub('DATA', struct.pack('<Q', size))
    return rec('TES3', body)

def scpt(name, text, stale=True):
    nm = name.encode('latin-1')[:31].ljust(32, b'\0')
    schd = nm + struct.pack('<5I', 0, 0, 0, 2 if stale else 0, 0)
    body = sub('SCHD', schd)
    if stale:
        body += sub('SCDT', b'\x01\x01')
    body += sub('SCTX', text.encode('latin-1'))
    return rec('SCPT', body)

ap = argparse.ArgumentParser()
ap.add_argument('out'); ap.add_argument('--master', action='append', default=[])
ap.add_argument('--script', action='append', default=[])
a = ap.parse_args()
masters = []
for m in a.master:
    n, s = m.rsplit(':', 1); masters.append((n, int(s)))
recs = []
for s in a.script:
    n, f = s.split('=', 1)
    txt = open(f, 'rb').read().decode('latin-1')
    recs.append(scpt(n, txt))
open(a.out, 'wb').write(tes3(masters, len(recs)) + b''.join(recs))
print('wrote', a.out, len(recs), 'scripts')
