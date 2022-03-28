#!/bin/bash

build/benchmark/delta-debug --init -f examples/Surfnet-63.json > dd_state
echo "Init done"
i=0
FAIL=""
while true; do
    echo -n "[${i}] Starting"
    cat dd_state | build/benchmark/delta-debug --step ${FAIL} -f temp.json > dd_state.tmp && mv dd_state.tmp dd_state || break
    echo -ne "\r[${i}] Delta-debug done"
    build/bin/pdaaal --input temp.json --compare
    EXIT_CODE=$?
    echo -ne "\r[${i}] PDAAAL test done"
    if [ "${EXIT_CODE}" -eq "0" ] ; then
        FAIL=""
        echo -e "\r[${i}] Test succes     "
    else
        FAIL="--fail"
        echo -e "\r[${i}] Test fail       "
    fi
    
    i=$((i+1))
done
rm dd_state.tmp
echo -e "\r[${i}] DONE!     "
i=$((i-1))
echo "*** DD STATE ***"
cat dd_state

echo "*** PDA ***"
cat temp.json
