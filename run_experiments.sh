#!/bin/bash
# Usage: ./run_experiments.sh <exe> <numDP> <numCL> <repeats> <out.csv>
EXE=${1:-./flp_omp}; DP=${2:-7000}; CL=${3:-95}; REP=${4:-3}; OUT=${5:-results.csv}
echo "pm,threads,rep,matrix,search,total,solution" > "$OUT"
for pm in 0 1; do
  for t in 1 2 4; do
    for r in $(seq 1 $REP); do
      o=$(OMP_NUM_THREADS=$t "$EXE" $DP $CL $pm)
      m=$(echo "$o" | grep Matricos | awk '{print $NF}')
      s=$(echo "$o" | grep Sprendinio | awk '{print $NF}')
      a=$(echo "$o" | grep Algoritmo | awk '{print $NF}')
      x=$(echo "$o" | grep Geriausias | sed 's/.*: //')
      echo "$pm,$t,$r,$m,$s,$a,\"$x\"" | tee -a "$OUT"
    done
  done
done
