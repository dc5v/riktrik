#!/bin/bash

TAGS=("sensor1" "sensor2" "sensor3" "locationA" "locationB" "deviceA" "deviceB" "temperature" "humidity" "pressure" "voltage")
CONDITIONS=("and" "or" "nand" "nor" "")

while true; do

  TAG_COUNT=$((RANDOM % 3 + 1))
  TAG_ARR=""

  for ((i=0; i<$TAG_COUNT; i++)); do

    TAG_RND=${TAGS[$RANDOM % ${#TAGS[@]}]}
    if [ $i -eq 0 ]; then
      TAG_ARR="\"$TAG_RND\""
    else
      TAG_ARR="$TAG_ARR, \"$TAG_RND\""
    fi

  done

  CND=${CONDITIONS[$((RANDOM % ${#CONDITIONS[@]}))]}
  EPOCH=$(date +%s%3N)

  JSON=$(cat <<EOF
{
  "query": "evaluate",
  "CND": "$CND",
  "tags": [ $TAG_ARR ]
}
EOF
  )

  echo "$JSON" # | nc localhost 8832

  sleep 1
done
