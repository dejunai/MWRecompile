import sys
sys.path.insert(0, __file__.rsplit('/',1)[0])
from espscripts import scripts
a=scripts(sys.argv[1]); b=scripts(sys.argv[2])
A={k.lower():v for k,v in a.items()}; B={k.lower():v for k,v in b.items()}
n=sys.argv[3].lower(); x=A[n]['scdt']; y=B[n]['scdt']
i=next((k for k in range(min(len(x),len(y))) if x[k]!=y[k]), min(len(x),len(y)))
w=int(sys.argv[4]) if len(sys.argv)>4 else 40
f=lambda z: ''.join(chr(c) if 32<=c<127 else '.' for c in z)
print('orig', x[max(0,i-w):i+w].hex(' ')); print('     ', f(x[max(0,i-w):i+w]))
print('new ', y[max(0,i-w):i+w].hex(' ')); print('     ', f(y[max(0,i-w):i+w]))
