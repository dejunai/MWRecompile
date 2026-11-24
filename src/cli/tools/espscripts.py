#!/usr/bin/env python3
"""Helpers to read SCPT records from a TES3 file (validation only)."""
import struct, sys
def scripts(path):
    b = open(path, 'rb').read(); p = 0; out = {}
    while p < len(b):
        t = b[p:p+4]; sz, unk, fl = struct.unpack_from('<III', b, p+4); body = b[p+16:p+16+sz]; p += 16+sz
        if t != b'SCPT': continue
        q = 0; subs = {}; order = []
        while q < len(body):
            st = body[q:q+4].decode('latin-1'); ss, = struct.unpack_from('<I', body, q+4)
            subs[st] = body[q+8:q+8+ss]; order.append(st); q += 8+ss
        name = subs['SCHD'][:32].split(b'\0')[0].decode('latin-1')
        hdr = struct.unpack_from('<5I', subs['SCHD'], 32)
        out[name] = dict(hdr=hdr, scvr=subs.get('SCVR', b''), scdt=subs.get('SCDT', b''), sctx=subs.get('SCTX', b''), order=order, flags=fl)
    return out
