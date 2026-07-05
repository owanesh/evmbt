#!/bin/sh
set -u

if [ "${1:-}" = "--worker" ]; then
  idx="$2"
  root="$3"
  timeout_s="$4"
  out_dir="${5:-}"
  line_no=$(cat "$root/line_$idx")
  bytecode_sha=$(cat "$root/sha_$idx")
  work="$root/case_$idx"

  mkdir -p "$work"
  (
    cd "$work" || exit 1
    timeout "$timeout_s" standalone-evmtrans "$root/input_$idx.bin" --runtime-input -o "$work/out.wasm"
  ) > "$work/stdout.log" 2> "$work/stderr.log"
  rc=$?

  if [ "$rc" -eq 0 ] && [ -s "$work/out.wasm" ]; then
    echo ok > "$root/result_$idx"
    if [ -n "$out_dir" ]; then
      mkdir -p "$out_dir"
      cp "$work/out.wasm" "$out_dir/${line_no}_${bytecode_sha}.wasm"
    fi
  elif [ "$rc" -eq 124 ]; then
    echo timeout > "$root/result_$idx"
  else
    echo fail > "$root/result_$idx"
  fi
  exit 0
fi

dataset="${DATASET:-${1:-}}"
limit="${LIMIT:-${2:-0}}"
timeout_s="${TIMEOUT:-${3:-20}}"
jobs="${JOBS:-${4:-8}}"
out_dir="${OUT:-${5:-}}"

if [ -z "$dataset" ]; then
  echo "usage: DATASET=/path/bytecodes.txt sh /work/deployment/bulk.sh [limit] [timeout_s] [jobs] [output_dir]" >&2
  exit 2
fi

root="/tmp/evmtrans_dataset"
rm -rf "$root"
mkdir -p "$root"

hex2bin='
function hx(c) { return index("0123456789abcdef", tolower(c)) - 1 }
{
  for (i = 1; i <= length($0); i += 2) {
    printf "%c", hx(substr($0, i, 1)) * 16 + hx(substr($0, i + 1, 1))
  }
}'

n=0
line_no=0
invalid=0
empty=0
while IFS= read -r line; do
  line_no=$((line_no + 1))
  if [ "$limit" -gt 0 ] && [ "$n" -ge "$limit" ]; then
    break
  fi

  hex=$(printf "%s" "$line" | sed 's/^0x//; s/[[:space:]]//g')
  if [ -z "$hex" ]; then
    empty=$((empty + 1))
    continue
  fi
  if [ $(( ${#hex} % 2 )) -ne 0 ] || printf "%s" "$hex" | grep -qi '[^0-9a-f]'; then
    invalid=$((invalid + 1))
    continue
  fi

  n=$((n + 1))
  bytecode_sha=$(printf "%s" "$hex" | sha256sum | awk '{print $1}')
  printf "%s" "$hex" | awk "$hex2bin" > "$root/input_$n.bin"
  printf "%s\n" "$line_no" > "$root/line_$n"
  printf "%s\n" "$bytecode_sha" > "$root/sha_$n"
  printf "%s\n" "$n" >> "$root/indices"
done < "$dataset"

if [ "$n" -gt 0 ]; then
  export root timeout_s out_dir
  xargs -P "$jobs" -I{} sh "$0" --worker {} "$root" "$timeout_s" "$out_dir" < "$root/indices"
fi

ok=$(grep -l '^ok$' "$root"/result_* 2>/dev/null | wc -l | tr -d ' ')
timeout_count=$(grep -l '^timeout$' "$root"/result_* 2>/dev/null | wc -l | tr -d ' ')
runtime_fail=$(grep -l '^fail$' "$root"/result_* 2>/dev/null | wc -l | tr -d ' ')
fail=$((runtime_fail + timeout_count + invalid + empty))

printf "FINAL total=%s ok=%s fail=%s timeout=%s runtime_fail=%s invalid=%s empty=%s jobs=%s timeout_s=%s output_dir=%s\n" \
  "$n" "$ok" "$fail" "$timeout_count" "$runtime_fail" "$invalid" "$empty" "$jobs" "$timeout_s" "${out_dir:-none}"
