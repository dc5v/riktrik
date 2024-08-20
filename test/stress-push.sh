#!/bin/bash

TAGS=("sensor1" "sensor2" "sensor3" "locationA" "locationB" "deviceA" "deviceB" "temperature" "humidity" "pressure" "voltage")

while true; do
  DATA_LEN=$((RANDOM % 8 + 1))
  DATA_ARR=""

  for ((i=0; i<$DATA_LEN; i++)); do
    DOUBLE_RND=$(awk -v seed=$RANDOM 'BEGIN { srand(seed); print rand() * 1000 }')
    if [ $i -eq 0 ]; then
      DATA_ARR="$DOUBLE_RND"
    else
      DATA_ARR="$DATA_ARR, $DOUBLE_RND"
    fi
  done

  TAG_COUNT=$((RANDOM % 5 + 1))
  TAG_ARR=""

  for ((i=0; i<$TAG_COUNT; i++)); do
    TAG_RND=${TAGS[$RANDOM % ${#TAGS[@]}]}
    if [ $i -eq 0 ]; then
      TAG_ARR="\"$TAG_RND\""
    else
      TAG_ARR="$TAG_ARR, \"$TAG_RND\""
    fi
  done

  JSON=$(cat <<EOF
  {
    "query": "push",
    "data": [ $DATA_ARR ],
    "tags": [ $TAG_ARR ]
  }
EOF
  )

  echo $JSON | nc localhost 8832
  sleep 0.01
done
