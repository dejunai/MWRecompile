#!/usr/bin/env python3
"""Compare script records between an original and a recompiled TES3 file."""
import sys, collections
sys.path.insert(0, __file__.rsplit('/',1)[0])
from espscripts import scripts
a = scripts(sys.argv[1]); b = scripts(sys.argv[2])
cat = collections.Counter(); diffs = []
for n, s in a.items():
    t = b[n]
    hd = s['hdr'] != t['hdr']; vd = s['scvr'] != t['scvr']; dd = s['scdt'] != t['scdt']
    if not (hd or vd or dd):
        cat['match'] += 1; continue
    key = ('H' if hd else '-') + ('V' if vd else '-') + ('D' if dd else '-')
    cat[key] += 1; diffs.append((n, key, s, t))
print(dict(cat), 'total', len(a))
if len(sys.argv) > 3:
    for n, key, s, t in diffs:
        if sys.argv[3] != '*' and n.lower() != sys.argv[3].lower(): continue
        print('==', n, key, s['hdr'], t['hdr'])
        if s['scvr'] != t['scvr']: print('  vars orig', s['scvr']); print('  vars new ', t['scvr'])
        x, y = s['scdt'], t['scdt']
        i = next((k for k in range(min(len(x), len(y))) if x[k] != y[k]), min(len(x), len(y)))
        print('  first data diff at', i, 'len', len(x), len(y))
        print('  orig', x[max(0,i-16):i+32].hex(' '))
        print('  new ', y[max(0,i-16):i+32].hex(' '))
