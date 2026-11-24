#!/usr/bin/env python3
import sys
sys.path.insert(0, __file__.rsplit('/',1)[0])
from espscripts import scripts
s = scripts(sys.argv[1])
d = {k.lower(): v for k, v in s.items()}
t = d[sys.argv[2].lower()]['sctx'].decode('latin-1').replace('\r', '')
pat = sys.argv[3].lower() if len(sys.argv) > 3 else None
for i, l in enumerate(t.split('\n')):
    if pat is None or pat in l.lower(): print(i, l)
