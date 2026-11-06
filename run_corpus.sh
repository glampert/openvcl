#!/usr/bin/env bash
# run_corpus.sh VCL INPUT_DIR OUT_DIR: runs the original vcl natively on every
# INPUT_DIR/*.vcl, the way the corpus was recorded (`vcl -t1 -uCORPUS
# NAME.vcl`, in a directory holding only the input), REPEATS times each.
# SHARD/SHARDS pick every SHARDS-th input. Each run keeps stdout, stderr, the
# exit status, wall seconds and its count of watchdog time-outs in
# OUT_DIR/NAME/rN/; ARGS overrides the vcl options.
set -u
vcl=$(realpath "$1"); inputs=$(realpath "$2"); out=$(realpath -m "$3")
repeats=${REPEATS:-3}; shard=${SHARD:-0}; shards=${SHARDS:-1}; args=${ARGS:--t1 -uCORPUS}
mkdir -p "$out"
index=0
for input in "$inputs"/*.vcl; do
  if [ $((index % shards)) -ne "$shard" ]; then index=$((index + 1)); continue; fi
  index=$((index + 1))
  name=$(basename "$input" .vcl)
  for run in $(seq 1 "$repeats"); do
    dir="$out/$name/r$run"; work=$(mktemp -d)
    mkdir -p "$dir"; cp "$input" "$work/$name.vcl"
    start=$(date +%s.%N)
    # shellcheck disable=SC2086
    (cd "$work" && "$vcl" $args "$name.vcl" > "$dir/stdout.bin" 2> "$dir/stderr.bin"); status=$?
    end=$(date +%s.%N)
    echo "$status" > "$dir/exit"
    echo "$end - $start" | bc > "$dir/seconds"
    grep -c "time out" "$dir/stderr.bin" > "$dir/timeouts" || true
    sha=$(sha256sum "$dir/stdout.bin" | cut -c1-12)
    echo "$name r$run exit=$status seconds=$(cat "$dir/seconds") timeouts=$(cat "$dir/timeouts") stdout=$sha"
    rm -rf "$work"
  done
done
