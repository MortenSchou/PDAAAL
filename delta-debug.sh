#!/bin/bash

#BIN_DIR="build"
BIN_DIR="cmake-build-release"

${BIN_DIR}/benchmark/delta-debug --init -f examples/Switch-67-conf-8.json > dd_state
echo "Init done"
i=0
FAIL=""
while true; do
    echo -n "[${i}] Starting"
    <dd_state ${BIN_DIR}/benchmark/delta-debug --step "${FAIL}" -f temp.json >dd_state.tmp && mv dd_state.tmp dd_state || break
    echo -ne "\r[${i}] Delta-debug done"
    ${BIN_DIR}/bin/pdaaal --input temp.json --compare
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
<dd_state ${BIN_DIR}/benchmark/delta-debug --simplify -f temp-simp.json

echo "*** DD STATE ***"
cat dd_state

echo "*** PDA ***"
cat temp.json

echo "*** Simplified PDA ***"
cat temp-simp.json
