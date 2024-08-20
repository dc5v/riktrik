#!/bin/bash

pid=$(pgrep tictacdb)

if [ -n "$pid" ]; then
  kill "$pid"
fi

gcc ./src/main.c -g -o ./bin/tictacdb -luuid -ljson-c -lpthread -lm -lz -lyaml

./bin/tictacdb