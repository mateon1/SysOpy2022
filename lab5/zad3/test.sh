#!/usr/bin/env bash

BLKSZ="$(($1))"
NPROD="$(($2))"
NCONS="$(($3))"
# ignore fourth argument, just a description

# For reproducibility, seed random
RANDOM=1

declare -a PRODS

function numtoident() {
    python <<EOF
i = $1
AL = 'abcdefghijklmnopqrstuvwxyz'
AL += AL.upper()
s = ''
while i or not s:
    i, r = divmod(i, len(AL))
    s += AL[r]
print(s)
EOF
}

function testdata() {
    local IDENT="$1"
    local SIZE="$2"
    seq -f "$IDENT%g" 999999999 | tr -d '\n' | head -c $((SIZE - 1)); echo
}

function testsetup() {
    rm -rf ./test
    mkdir ./test

    local LINES="$(seq $((NPROD * 4)) | shuf | head -n $NPROD)"
    local MAXLINE="$(echo "$LINES" | sort -n | tail -n 1)"

    local -a HASH
    local NUM=0

    for lineno in $LINES; do
        local IDENT="$(numtoident $NUM)"
        local LEN=$(( (BLKSZ * 32768 + RANDOM) * 5 / 32768 ))
        testdata "$IDENT" $LEN > "./test/$IDENT.in"
        HASH[lineno]="./test/$IDENT.in"
        NUM=$((NUM+1))
    done

    for lineno in $(seq "$MAXLINE"); do
        if [ -z "${HASH[lineno]}" ]; then
            echo
        else
            cat "${HASH[lineno]}"
            PRODS["${#PRODS[@]}"]="./test/fifo $lineno ${HASH[lineno]} $BLKSZ"
        fi
    done > ./test/expected
}

testsetup

mkfifo ./test/fifo

for prod in "${PRODS[@]}"; do
    #ltrace -e write ./producer $prod &
    ./producer $prod &
done

for cons in $(seq $NCONS); do
    #ltrace -e read ./consumer ./test/fifo ./test/out $BLKSZ &
    ./consumer ./test/fifo ./test/out $BLKSZ &
done

for foo in $(seq $((NCONS + NPROD))); do
    wait -n || exit 1
done

diff -q ./test/out ./test/expected || exit 1
echo Test successful!
