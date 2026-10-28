#!/bin/bash
# Compare sol_base vs sol_ordh on multiple test cases
# Usage: n seed
N=$1
SEED=$2

./gen2 $N $SEED > /tmp/test_input.txt
# Get base result
W_BASE=$(./sol_base < /tmp/test_input.txt | head -1 | awk '{print $1}')
H_BASE=$(./sol_base < /tmp/test_input.txt | head -1 | awk '{print $2}')
A_BASE=$((W_BASE * H_BASE))

# Get ordh result
W_ORDH=$(./sol_ordh < /tmp/test_input.txt | head -1 | awk '{print $1}')
H_ORDH=$(./sol_ordh < /tmp/test_input.txt | head -1 | awk '{print $2}')
A_ORDH=$((W_ORDH * H_ORDH))

echo "n=$N seed=$SEED base=${W_BASE}x${H_BASE}=${A_BASE} ordh=${W_ORDH}x${H_ORDH}=${A_ORDH} diff=$((A_ORDH-A_BASE))"
