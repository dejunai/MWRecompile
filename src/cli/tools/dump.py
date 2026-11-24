import sys
sys.path.insert(0, __file__.rsplit('/',1)[0])
from espscripts import scripts
a={k.lower():v for k,v in scripts(sys.argv[1]).items()}
s=a[sys.argv[2].lower()]
x=s['scdt']
print(s['sctx'].decode('latin-1')[:int(sys.argv[3]) if len(sys.argv)>3 else 800])
for i in range(0,len(x),32):
    c=x[i:i+32]; print('%04x'%i, c.hex(' '), ''.join(chr(b) if 32<=b<127 else '.' for b in c))
