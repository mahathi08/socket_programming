#!/bin/bash

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

CONFIG="$ROOT/config.txt"
VN_SCRIPT="$ROOT/vn/main.py"
ON_IP="172.23.32.143"
BASE_PORT=5001

if [[ ! -f "$CONFIG" ]]; then
    echo "Error: config file not found: $CONFIG"
    exit 1
fi

ROWS=$(grep -Ec '^[[:space:]]*[^#[:space:]]' "$CONFIG")
N=$((ROWS + 1))
echo "number of vns: $N"

if [[ ! -f "$VN_SCRIPT" ]]; then
    echo "Error: VN script not found: $VN_SCRIPT"
    exit 1
fi

for ((i = 0; i < N; i++)); do
    PORT=$((BASE_PORT + i))
    echo "starting vn $((i + 1)) on UDP port $PORT"
    python3 "$VN_SCRIPT" "$ON_IP" "$PORT" &
    sleep 1
done

wait
