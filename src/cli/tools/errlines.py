import sys,re
sys.path.insert(0, __file__.rsplit('/',1)[0])
from espscripts import scripts
f=sys.argv[1]; pat=sys.argv[2] if len(sys.argv)>2 else ''
a={k.lower():v for k,v in scripts(f'testdata/{f}.esm').items()}
for line in open(f'val/{f}.err'):
    m=re.match(r'[^:]*: ([^:]*): line (\d+), col (\d+): (.*)',line)
    if not m or pat not in m.group(4): continue
    n,l=m.group(1),int(m.group(2))
    src=a[n.lower()]['sctx'].decode('latin-1').replace('\r','').split('\n')
    print('%-24s L%-4d %-60s | %s'%(n,l,src[l-1].strip()[:60] if l-1<len(src) else '?',m.group(4)[:70]))
