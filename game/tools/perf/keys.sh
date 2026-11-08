#!/bin/sh
cd "/Users/rester159/genesis ports/_capcom/willow/game"
tr ';' '\n' < tests/traces/route.keys | awk -F: -v t=100 '{split($1,a,"-"); printf "%d-%d:%s;", a[1]+t, a[2]+t, $2}' > out/perf/route100.keys
echo "100-3500:R;$(python3 -c "print(';'.join(f'{t}-{t+2}:A' for t in range(150,3500,8)))");$(python3 -c "print(';'.join(f'{t}-{t+20}:UB' for t in range(300,3500,200)))")" > out/perf/s5top.keys
