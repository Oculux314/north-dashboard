#!/usr/bin/env bash

URL="https://xxx/api/mutation"
URL_PATH="api:postLog"

voltage=12.0
current=0.0

while true; do
  # Timestamp (ms)
  ts=$(($(date +%s%N)/1000000))

  # Voltage: random small drift, clamp to 11.5–12.5
  v_raw=$((RANDOM % 20 - 10))
  v_delta=$(printf "%.3f" "$(echo "$v_raw/1000" | bc -l)")
  voltage=$(printf "%.3f" "$(echo "$voltage + $v_delta" | bc -l)")
  if (( $(echo "$voltage < 11.5" | bc -l) )); then voltage=11.5; fi
  if (( $(echo "$voltage > 12.5" | bc -l) )); then voltage=12.5; fi

  # Current: wide swing -5A to +20A
  i_raw=$((RANDOM % 2500 - 500))
  i_delta=$(printf "%.3f" "$(echo "$i_raw/100" | bc -l)")
  current=$(printf "%.3f" "$(echo "$current + $i_delta" | bc -l)")

  # Post
  curl -s "$URL" \
    -H "Content-Type: application/json" \
    -d "{\"path\": \"$URL_PATH\", \"args\": {\"timestamp\": $ts, \"voltage\": $voltage, \"current\": $current}, \"format\": \"json\"}" \
    > /dev/null

  echo "$ts  V=$voltage  I=$current"

  sleep 1
done
