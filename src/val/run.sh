#!/bin/bash
# usage: val/run.sh  -> rebuild and compare all three vanilla masters
cd /workspace/mwrecompile
export WINEDEBUG=-all WINEPREFIX=/workspace/mwrecompile/.wineprefix-noncache
make -j8 >val/make.log 2>&1 || { grep -E " error|undefined" val/make.log; exit 1; }
for f in Morrowind Tribunal Bloodmoon; do
  wine ./MWRecompile.exe testdata/$f.esm -k -q -o val/${f}_out.esm 2>val/$f.err
  echo "$f: errors=$(grep -c ': line' val/$f.err) $(python3 cli/tools/compare.py testdata/$f.esm val/${f}_out.esm)"
done
