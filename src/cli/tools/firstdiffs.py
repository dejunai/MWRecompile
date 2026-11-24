import sys
sys.path.insert(0, __file__.rsplit('/',1)[0])
from espscripts import scripts
f=sys.argv[1]; pat=sys.argv[2].encode() if len(sys.argv)>2 else None
a=scripts(f'testdata/{f}.esm'); b=scripts(f'val/{f}_out.esm')
g=lambda z: ''.join(chr(c) if 32<=c<127 else '.' for c in z)
for n,s in a.items():
    x=s['scdt']; y=b[n]['scdt']
    if x==y: continue
    i=next((k for k in range(min(len(x),len(y))) if x[k]!=y[k]), min(len(x),len(y)))
    if pat and pat not in x[max(0,i-30):i+30]: continue
    print('%-28s o %s\n%-28s n %s'%(n,g(x[max(0,i-24):i+24]),'',g(y[max(0,i-24):i+24])))
